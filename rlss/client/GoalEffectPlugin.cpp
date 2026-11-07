#include "GoalEffectPlugin.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <optional>
#include <random>
#include <vector>

#include "Arena.hpp"
#include "AssetsPlugin.hpp"
#include "BallViewPlugin.hpp"
#include "CameraPlugin.hpp"
#include "HudPlugin.hpp"
#include "components/AnimationComponent.hpp"
#include "components/Renderable.hpp"
#include "components/Transform.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "core/services/IScreenDrawService.hpp"
#include "core/services/IWindowService.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "game/Components.hpp"
#include "protocol/Protocol.hpp"
#include "systems/render/AnimationPlugin.hpp"

namespace rlss::client {

namespace {

constexpr int PHASE_GOAL = static_cast<int>(protocol::MatchPhase::Goal);

// Textures, as drawn by rlss/assets/generate_textures.py.
constexpr int EXPLOSION_FRAMES = 20;
constexpr int EXPLOSION_FRAME = 192;
constexpr float GLOW_SIZE = 256.0F;
constexpr float SHOCKWAVE_SIZE = 256.0F;
constexpr float SPARK_WIDTH = 128.0F;
constexpr float SPARK_HEIGHT = 24.0F;
constexpr float PUFF_SIZE = 96.0F;

/** @brief Seconds per frame of the fireball: 20 frames in 1.1 s. */
constexpr float EXPLOSION_FRAME_TIME = 0.055F;
/** @brief Width of the fireball, in uu (the ball is 160 wide). */
constexpr float EXPLOSION_WIDTH = 1250.0F;
constexpr int SPARKS = 56;
constexpr int PUFFS = 12;
/** @brief The ball stays hidden this long after the celebration, until the snapshots show it at the center. */
constexpr float HIDDEN_AFTER_GOAL = 0.3F;
/** @brief Seconds the screen flash lasts, and its opacity at first. */
constexpr float FLASH_TIME = 0.35F;
constexpr float FLASH_ALPHA = 120.0F;

constexpr int LAYER_SMOKE = 18;
constexpr int LAYER_LIGHT = 19;
constexpr int LAYER_SHOCKWAVE = 20;
constexpr int LAYER_FIREBALL = 21;
constexpr int LAYER_SPARKS = 22;

constexpr ecs::Rgba WHITE{.r = 255, .g = 255, .b = 255, .a = 255};
constexpr ecs::Rgba GOLD{.r = 255, .g = 214, .b = 90, .a = 255};

ecs::Rgba lighten(const Rgb color, const float amount) {
    const auto channel = [amount](const std::uint8_t value) {
        return static_cast<std::uint8_t>(static_cast<float>(value) + ((255.0F - static_cast<float>(value)) * amount));
    };
    return {.r = channel(color.r), .g = channel(color.g), .b = channel(color.b), .a = 255};
}

ecs::Rgba darken(const Rgb color, const float factor) {
    const auto channel = [factor](const std::uint8_t value) {
        return static_cast<std::uint8_t>(static_cast<float>(value) * factor);
    };
    return {.r = channel(color.r), .g = channel(color.g), .b = channel(color.b), .a = 255};
}

/** @brief Spawns one particle at (x, y). */
ecs::Entity spawnParticle(ecs::Registry& registry, const float x, const float y, const ecs::TextureId texture,
                          const ecs::Rgba tint, const int layer, const GoalParticle& particle) {
    const ecs::Entity entity = registry.spawnEntity();
    registry.addComponent(entity, ecs::Transform{
                                      .x = x,
                                      .y = y,
                                      .scaleX = particle.startWidth / particle.textureWidth,
                                      .scaleY = particle.startHeight / particle.textureHeight,
                                  });
    ecs::Renderable renderable;
    renderable.textureId = texture;
    renderable.tint = tint;
    renderable.tint.a = static_cast<std::uint8_t>(255.0F * particle.startAlpha);
    renderable.layer = layer;
    registry.addComponent(entity, ecs::Renderable{renderable});
    registry.addComponent(entity, GoalParticle{particle});
    return entity;
}

}  // namespace

void GoalEffectPlugin::build(engine::PluginBuilder& builder) {
    builder.require<AssetsPlugin>();
    builder.require<CameraPlugin>();
    builder.require<HudPlugin>();
    builder.require<BallViewPlugin>();
    builder.require<systems::AnimationPlugin>();
}

void GoalEffectPlugin::init(engine::Engine& engine) {
    const GameAssets& assets = engine.registry().getResource<GameAssets>();
    _explosionBlue = assets.explosionBlue;
    _explosionOrange = assets.explosionOrange;
    _shockwave = assets.shockwave;
    _spark = assets.spark;
    _glow = assets.glow;
    _puff = assets.puff;
    _screen = Typeface(engine.service<engine::IScreenDrawService>(), assets.font);
}

void GoalEffectPlugin::update(engine::Engine& engine, const ecs::time::Time& time) {
    const float dt = time.deltaTime;
    ecs::Registry& registry = engine.registry();
    std::optional<game::MatchState> current;
    registry.runSystem<game::MatchState>(
        [&current](ecs::Entity /*entity*/, const game::MatchState& state) { current = state; });

    if (current && _last && current->phase == PHASE_GOAL && _last->phase != PHASE_GOAL) {
        // The team whose score went up; a status missed in between counts for blue unless orange's went up.
        const bool blueScored = current->blueScore > _last->blueScore || current->orangeScore <= _last->orangeScore;
        const auto& rigs = registry.getComponents<game::CameraRig>();
        const ecs::Entity camera = engine.getPlugin<CameraPlugin>().camera();
        const auto& transforms = registry.getComponents<ecs::Transform>();
        if (rigs.contains(camera) && rigs.get(camera).ball != game::NO_ENTITY) {
            const auto ball = static_cast<ecs::Entity>(rigs.get(camera).ball);
            if (transforms.contains(ball)) {
                const ecs::Transform where = transforms.get(ball);
                explode(engine, where.x, where.y, blueScored);
            }
        }
    }
    _last = current;

    // The ball is gone during the celebration, and a little after, until the snapshots put it back at the center.
    const bool celebrating = current && current->phase == PHASE_GOAL;
    _hiddenFor = celebrating ? HIDDEN_AFTER_GOAL : std::max(0.0F, _hiddenFor - dt);
    // Every frame: a ball spawned during the celebration (a player joining) must be hidden too.
    showBall(engine, !celebrating && _hiddenFor <= 0.0F);

    stepParticles(engine, dt);

    _flashAge += dt;
    engine::IScreenDrawService* screen = _screen.service();
    if (_flashAge < FLASH_TIME && screen != nullptr) {
        const auto& window = engine.service<engine::IWindowService>();
        ecs::Rgba color = _flashColor;
        color.a = static_cast<std::uint8_t>(FLASH_ALPHA * (1.0F - (_flashAge / FLASH_TIME)));
        screen->fillRect(
            {
                .x = 0.0F,
                .y = 0.0F,
                .width = static_cast<float>(window.width()),
                .height = static_cast<float>(window.height()),
            },
            color);
    }
}

void GoalEffectPlugin::explode(engine::Engine& engine, const float x, const float y, const bool blueScored) {
    ecs::Registry& registry = engine.registry();
    const Rgb team = blueScored ? BLUE_TEAM : ORANGE_TEAM;
    const ecs::Rgba light = lighten(team, 0.45F);
    _flashAge = 0.0F;
    _flashColor = lighten(team, 0.6F);

    // A burst of light, then the shockwave racing out.
    spawnParticle(registry, x, y, _glow, light, LAYER_LIGHT,
                  {
                      .life = 0.6F,
                      .startWidth = 500.0F,
                      .startHeight = 500.0F,
                      .endWidth = 2600.0F,
                      .endHeight = 2600.0F,
                      .textureWidth = GLOW_SIZE,
                      .textureHeight = GLOW_SIZE,
                      .startAlpha = 0.95F,
                  });
    spawnParticle(registry, x, y, _shockwave, light, LAYER_SHOCKWAVE,
                  {
                      .life = 0.75F,
                      .startWidth = 180.0F,
                      .startHeight = 180.0F,
                      .endWidth = 3000.0F,
                      .endHeight = 3000.0F,
                      .textureWidth = SHOCKWAVE_SIZE,
                      .textureHeight = SHOCKWAVE_SIZE,
                  });
    spawnParticle(registry, x, y, _shockwave, WHITE, LAYER_SHOCKWAVE,
                  {
                      .life = 0.45F,
                      .startWidth = 120.0F,
                      .startHeight = 120.0F,
                      .endWidth = 1700.0F,
                      .endHeight = 1700.0F,
                      .textureWidth = SHOCKWAVE_SIZE,
                      .textureHeight = SHOCKWAVE_SIZE,
                      .startAlpha = 0.8F,
                  });

    // The fireball: an animated strip, played once.
    const ecs::TextureId sheet = blueScored ? _explosionBlue : _explosionOrange;
    if (sheet != ecs::NO_TEXTURE) {
        const ecs::Entity fireball =
            spawnParticle(registry, x, y, sheet, WHITE, LAYER_FIREBALL,
                          {
                              .life = EXPLOSION_FRAME_TIME * static_cast<float>(EXPLOSION_FRAMES),
                              .startWidth = EXPLOSION_WIDTH,
                              .startHeight = EXPLOSION_WIDTH,
                              .endWidth = EXPLOSION_WIDTH * 1.1F,
                              .endHeight = EXPLOSION_WIDTH * 1.1F,
                              .textureWidth = static_cast<float>(EXPLOSION_FRAME),
                              .textureHeight = static_cast<float>(EXPLOSION_FRAME),
                              .fades = false,
                          });
        registry.getComponents<ecs::Renderable>().get(fireball).sourceRect = {
            .x = 0,
            .y = 0,
            .width = EXPLOSION_FRAME,
            .height = EXPLOSION_FRAME,
        };
        registry.addComponent(fireball, ecs::AnimationComponent{
                                            .frameWidth = EXPLOSION_FRAME,
                                            .frameHeight = EXPLOSION_FRAME,
                                            .frameCount = EXPLOSION_FRAMES,
                                            .frameDuration = EXPLOSION_FRAME_TIME,
                                            .loop = false,
                                        });
    }

    // Sparks flying out in every direction and falling, in white, gold and the team color.
    std::uniform_real_distribution<float> angle(0.0F, 2.0F * std::numbers::pi_v<float>);
    std::uniform_real_distribution<float> unit(0.0F, 1.0F);
    const std::array<ecs::Rgba, 3> sparkColors{WHITE, GOLD, light};
    for (int i = 0; i < SPARKS; ++i) {
        const float direction = angle(_random);
        const float speed = 700.0F + (1700.0F * unit(_random));
        const float length = 60.0F + (90.0F * unit(_random));
        spawnParticle(registry, x, y, _spark, sparkColors.at(static_cast<std::size_t>(i) % sparkColors.size()),
                      LAYER_SPARKS,
                      {
                          .life = 0.55F + (0.8F * unit(_random)),
                          .vx = std::cos(direction) * speed,
                          .vy = std::sin(direction) * speed,
                          .gravity = 900.0F,
                          .drag = 1.2F,
                          .startWidth = length,
                          .startHeight = 14.0F,
                          .endWidth = length * 0.4F,
                          .endHeight = 8.0F,
                          .textureWidth = SPARK_WIDTH,
                          .textureHeight = SPARK_HEIGHT,
                          .alignToVelocity = true,
                      });
    }
    // Smoke drifting up and spreading.
    for (int i = 0; i < PUFFS; ++i) {
        const float direction = angle(_random);
        const float speed = 120.0F + (260.0F * unit(_random));
        const float size = 160.0F + (140.0F * unit(_random));
        spawnParticle(registry, x, y, _puff, darken(team, 0.35F), LAYER_SMOKE,
                      {
                          .life = 1.4F + (0.6F * unit(_random)),
                          .vx = std::cos(direction) * speed,
                          .vy = (std::sin(direction) * speed) - 120.0F,
                          .gravity = -60.0F,
                          .drag = 0.8F,
                          .startWidth = size,
                          .startHeight = size,
                          .endWidth = size * 2.6F,
                          .endHeight = size * 2.6F,
                          .textureWidth = PUFF_SIZE,
                          .textureHeight = PUFF_SIZE,
                          .startAlpha = 0.7F,
                      });
    }
}

void GoalEffectPlugin::stepParticles(engine::Engine& engine, const float dt) {
    ecs::Registry& registry = engine.registry();
    std::vector<ecs::Entity> finished;
    registry.runSystem<GoalParticle, ecs::Transform, ecs::Renderable>(
        [dt, &finished](const ecs::Entity entity, GoalParticle& particle, ecs::Transform& transform,
                        ecs::Renderable& renderable) {
            particle.age += dt;
            if (particle.age >= particle.life) {
                finished.push_back(entity);
                return;
            }
            const float t = particle.age / particle.life;
            const float keep = std::max(0.0F, 1.0F - (particle.drag * dt));
            particle.vx *= keep;
            particle.vy = (particle.vy * keep) + (particle.gravity * dt);
            transform.x += particle.vx * dt;
            transform.y += particle.vy * dt;
            // Sizes ease out: fast at first, then slower.
            const float ease = 1.0F - ((1.0F - t) * (1.0F - t));
            const float width = particle.startWidth + ((particle.endWidth - particle.startWidth) * ease);
            const float height = particle.startHeight + ((particle.endHeight - particle.startHeight) * ease);
            transform.scaleX = width / particle.textureWidth;
            transform.scaleY = height / particle.textureHeight;
            if (particle.alignToVelocity) {
                transform.rotation = std::atan2(particle.vy, particle.vx);
            }
            if (particle.fades) {
                renderable.tint.a = static_cast<std::uint8_t>(255.0F * particle.startAlpha * (1.0F - (t * t)));
            }
        });
    for (const ecs::Entity entity : finished) {
        if (registry.isAlive(entity)) {
            registry.killEntity(entity);
        }
    }
}

void GoalEffectPlugin::showBall(engine::Engine& engine, const bool shown) {
    engine.registry().runSystem<game::BallPart, ecs::Renderable>(
        [shown](ecs::Entity /*entity*/, const game::BallPart& /*part*/, ecs::Renderable& renderable) {
            renderable.tint.a = shown ? 255 : 0;
        });
}

void GoalEffectPlugin::shutdown(engine::Engine& engine) {
    ecs::Registry& registry = engine.registry();
    std::vector<ecs::Entity> particles;
    registry.runSystem<GoalParticle>(
        [&particles](const ecs::Entity entity, const GoalParticle& /*particle*/) { particles.push_back(entity); });
    for (const ecs::Entity entity : particles) {
        if (registry.isAlive(entity)) {
            registry.killEntity(entity);
        }
    }
}

}  // namespace rlss::client
