/**
 * @file RTypeGamePlugin.cpp
 * @brief Implementation of RTypeGamePlugin demonstration game logic.
 */

#include "RTypeGamePlugin.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Logger.hpp"
#include "components/AnimationComponent.hpp"
#include "components/CameraComponent.hpp"
#include "components/ParallaxLayer.hpp"
#include "components/Renderable.hpp"
#include "components/SpriteInstances.hpp"
#include "components/Transform.hpp"
#include "config/ConfigFile.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "core/services/IInputService.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "game/ReplicationCodec.hpp"
#include "gameplay/ShipBlink.hpp"
#include "input/InputPlugin.hpp"
#include "input/InputState.hpp"
#include "input/RTypeBindings.hpp"
#include "looks/BossLooks.hpp"
#include "net/ReplicationEvents.hpp"
#include "render/RenderPlugin.hpp"
#include "render/texture/TextureStore.hpp"
#include "systems/render/AnimationPlugin.hpp"
#include "ui/UiPlugin.hpp"

namespace client {

namespace {

namespace log = rtype::log;

/** @brief Tag of the messages of this plugin in the log. */
constexpr const char* LOG_TAG = "client";

constexpr float SCENE_WIDTH = 1280.0F;
constexpr float SCENE_HEIGHT = 720.0F;
constexpr float SPRITE_SCALE = 2.0F;
constexpr int SHIP_FRAME_WIDTH = 33;
constexpr int SHIP_FRAME_HEIGHT = 17;
constexpr int SHIP_FRAME_COUNT = 5;
constexpr int BYDOS_FRAME_WIDTH = 33;
constexpr int BYDOS_FRAME_HEIGHT = 36;
constexpr int BYDOS_FRAME_COUNT = 8;
constexpr int BULLET_FRAME_SIZE = 32;
constexpr int BULLET_FRAME_COUNT = 4;
constexpr float BULLET_SCALE = 1.5F;
constexpr float BULLET_FRAME_DURATION = 0.015F;
constexpr float DEFAULT_FRAME_DURATION = 0.12F;
constexpr ecs::Rgba COLOR_WHITE = {.r = 255, .g = 255, .b = 255, .a = 255};
/** @brief Side of the square the renderer draws for an untextured `Renderable`, before the `Transform` scale. */
constexpr float PLACEHOLDER_PIXELS = 32.0F;
/** @brief Height of an obstacle whose subtype carries none (spawned by a script): the default 64 px block. */
constexpr float DEFAULT_OBSTACLE_HEIGHT = 64.0F;
constexpr ecs::Rgba COLOR_BOSS = {.r = 170, .g = 60, .b = 200, .a = 255};
/** @brief Side of the box drawn for a boss part until boss looks are configured, in pixels. */
constexpr float BOSS_PART_PLACEHOLDER_SIZE = 48.0F;
constexpr ecs::Rgba COLOR_BOSS_PART = {.r = 230, .g = 120, .b = 240, .a = 255};
constexpr ecs::Rgba COLOR_MISSILE = {.r = 255, .g = 90, .b = 60, .a = 255};
constexpr ecs::Rgba COLOR_POWER_UP = {.r = 80, .g = 220, .b = 120, .a = 255};
constexpr ecs::Rgba COLOR_TERRAIN = {.r = 110, .g = 90, .b = 70, .a = 255};
constexpr ecs::Rgba COLOR_STAR_SMALL = {.r = 200, .g = 200, .b = 220, .a = 80};
constexpr ecs::Rgba COLOR_STAR_MEDIUM = {.r = 220, .g = 220, .b = 255, .a = 150};
constexpr ecs::Rgba COLOR_STAR_LARGE = {.r = 255, .g = 255, .b = 255, .a = 220};

using ::input::InputState;

struct StarLayer {
    int count = 0;
    float speed = 0.0F;
    ecs::Rgba color{};
    int layer = 0;
    ecs::TextureId texId = ecs::NO_TEXTURE;
};

void spawnStarfield(ecs::Registry& registry, ecs::TextureId smallId, ecs::TextureId mediumId, ecs::TextureId largeId) {
    const std::array<StarLayer, 3> layers{
        {
            {.count = 80, .speed = 25.0F, .color = COLOR_STAR_SMALL, .layer = -3, .texId = smallId},
            {.count = 40, .speed = 60.0F, .color = COLOR_STAR_MEDIUM, .layer = -2, .texId = mediumId},
            {.count = 20, .speed = 120.0F, .color = COLOR_STAR_LARGE, .layer = -1, .texId = largeId},
        },
    };

    int seed = 0;
    for (const auto& def : layers) {
        ecs::SpriteInstances pool;
        pool.instances.reserve(static_cast<std::size_t>(def.count));
        for (int i = 0; i < def.count; ++i) {
            const auto x = static_cast<float>(((seed * 137) + 43) % static_cast<int>(SCENE_WIDTH));
            const auto y = static_cast<float>(((seed * 97) + 17) % static_cast<int>(SCENE_HEIGHT));
            ++seed;
            pool.instances.push_back({.x = x, .y = y});
        }
        const ecs::Entity star = registry.spawnEntity();
        registry.addComponent<ecs::SpriteInstances>(star, std::move(pool));
        registry.addComponent<ecs::Renderable>(
            star, ecs::Renderable{.textureId = def.texId, .tint = def.color, .layer = def.layer});
        registry.addComponent<ecs::ParallaxLayer>(star, ecs::ParallaxLayer{
                                                            .scrollSpeedX = -def.speed,
                                                            .wrapMinX = -8.0F,
                                                            .wrapMaxX = SCENE_WIDTH,
                                                        });
    }
}

/** @brief Sprite of a replicated entity: one frame strip of a spritesheet. */
struct EntitySprite {
    ecs::TextureId texture = ecs::NO_TEXTURE;
    int frameWidth = 0;
    int frameHeight = 0;
    int frameCount = 1;
    int startY = 0;
    int startX = 0;
    int layer = 0;
    float scale = SPRITE_SCALE;
    float frameDuration = DEFAULT_FRAME_DURATION;
    bool loop = true;
};

/** @brief Gives a replicated entity its sprite and its size on screen. */
void applySprite(ecs::Registry& registry, const ecs::Entity entity, const EntitySprite& sprite) {
    auto& transform = registry.getComponents<ecs::Transform>().get(entity);
    transform.scaleX = sprite.scale;
    transform.scaleY = sprite.scale;
    registry.addComponent<ecs::Renderable>(entity, ecs::Renderable{
                                                       .textureId = sprite.texture,
                                                       .sourceRect = {.x = sprite.startX,
                                                                      .y = sprite.startY,
                                                                      .width = sprite.frameWidth,
                                                                      .height = sprite.frameHeight},
                                                       .origin = {.x = 0.5F, .y = 0.5F},
                                                       .tint = COLOR_WHITE,
                                                       .layer = sprite.layer,
                                                   });
    registry.addComponent<ecs::AnimationComponent>(entity, ecs::AnimationComponent{
                                                               .frameWidth = sprite.frameWidth,
                                                               .frameHeight = sprite.frameHeight,
                                                               .frameCount = sprite.frameCount,
                                                               .startX = sprite.startX,
                                                               .startY = sprite.startY,
                                                               .frameDuration = sprite.frameDuration,
                                                               .loop = sprite.loop,
                                                           });
}

/**
 * @brief Draws a replicated entity as a plain colored box, for the entities without a sprite yet (boss, terrain).
 * @param registry Registry owning the entity.
 * @param entity Replicated entity.
 * @param width Width on screen, in pixels.
 * @param height Height on screen, in pixels.
 * @param color Fill color.
 * @param layer Draw layer.
 */
void applyBox(ecs::Registry& registry, const ecs::Entity entity, const float width, const float height,
              const ecs::Rgba color, const int layer) {
    auto& transform = registry.getComponents<ecs::Transform>().get(entity);
    transform.scaleX = width / PLACEHOLDER_PIXELS;
    transform.scaleY = height / PLACEHOLDER_PIXELS;
    registry.addComponent<ecs::Renderable>(entity, ecs::Renderable{
                                                       .origin = {.x = 0.5F, .y = 0.5F},
                                                       .tint = color,
                                                       .layer = layer,
                                                   });
}

}  // namespace

void RTypeGamePlugin::build(engine::PluginBuilder& builder) {
    builder.require<render::RenderPlugin>();
    builder.require<::input::InputPlugin>();
    builder.require<systems::AnimationPlugin>();
    builder.require<ui::UiPlugin>();
}

void RTypeGamePlugin::init(engine::Engine& engine) {
    render::TextureStore* textures = engine.getPlugin<render::RenderPlugin>().textures();
    if (textures == nullptr) {
        throw std::runtime_error("RTypeGamePlugin: TextureStore not available");
    }

    const auto load = [&](const std::string& path) {
        const ecs::TextureId id = textures->load(path);
        if (id == ecs::NO_TEXTURE) {
            throw std::runtime_error("failed to load texture '" + path +
                                     "' — run the client from the project root directory");
        }
        return id;
    };

    _shipTexId = load("sprites/r-typesheet42.gif");
    _bydosTexId = load("sprites/spritesheet5.png");
    _bulletTexId = load("sprites/r-typesheet-amo.png");

    _starSmallId = load("sprites/star_small.png");
    _starMediumId = load("sprites/star_medium.png");
    _starLargeId = load("sprites/star_large.png");

    loadBossLooks(*textures);

    if (engine.mediator().hasService<engine::IInputService>()) {
        client::input::setupDefaultBindings(engine.service<engine::IInputService>().mapping());
    }

    ecs::Registry& registry = engine.registry();
    spawnStarfield(registry, _starSmallId, _starMediumId, _starLargeId);
    registry.addComponent<ecs::CameraComponent>(registry.spawnEntity(), ecs::CameraComponent{});
    // The local player's actions, sent to the server by the network plugins. Nothing of the game is spawned here:
    // ships, enemies and bullets come from the snapshots of the server.
    registry.addComponent<InputState>(registry.spawnEntity(), InputState{.playerIndex = 0, .actions = {}});

    _spawned = engine.eventBus().subscribe<ecs::net::ReplicatedEntitySpawned>(
        [this, &engine](const ecs::net::ReplicatedEntitySpawned& event) { onEntitySpawned(engine, event); });
}

void RTypeGamePlugin::onEntitySpawned(engine::Engine& engine, const ecs::net::ReplicatedEntitySpawned& event) const {
    ecs::Registry& registry = engine.registry();
    if (event.kind == rtype::game::KIND_PLAYER_SHIP) {
        // Each row of the spritesheet is one ship color, so that players can tell their ships apart.
        const int row = registry.getComponents<rtype::game::EntityNetState>().get(event.entity).subtype;
        applySprite(registry, event.entity,
                    {
                        .texture = _shipTexId,
                        .frameWidth = SHIP_FRAME_WIDTH,
                        .frameHeight = SHIP_FRAME_HEIGHT,
                        .frameCount = SHIP_FRAME_COUNT,
                        .startY = row * SHIP_FRAME_HEIGHT,
                        .layer = 3,
                    });
    } else if (event.kind == rtype::game::KIND_ALIEN_SCOUT) {
        applySprite(registry, event.entity,
                    {
                        .texture = _bydosTexId,
                        .frameWidth = BYDOS_FRAME_WIDTH,
                        .frameHeight = BYDOS_FRAME_HEIGHT,
                        .frameCount = BYDOS_FRAME_COUNT,
                        .startY = 0,
                        .layer = 2,
                    });
    } else if (event.kind == rtype::game::KIND_BOSS_CORE || event.kind == rtype::game::KIND_BOSS_PART) {
        // The subtype picks the look: the variant of a boss, the look of a part.
        const bool part = event.kind == rtype::game::KIND_BOSS_PART;
        const std::uint8_t id = registry.getComponents<rtype::game::EntityNetState>().get(event.entity).subtype;
        const auto& configured = part ? _bossLooks.parts : _bossLooks.bosses;
        const auto found = configured.find(id);
        const looks::Look* look = found != configured.end() ? &found->second : nullptr;
        if (part) {
            applyLook(registry, event.entity, look, {BOSS_PART_PLACEHOLDER_SIZE, BOSS_PART_PLACEHOLDER_SIZE},
                      COLOR_BOSS_PART, 3);
        } else {
            applyLook(registry, event.entity, look, {rtype::game::BOSS_CORE_WIDTH, rtype::game::BOSS_CORE_HEIGHT},
                      COLOR_BOSS, 2);
        }
    } else if (event.kind == rtype::game::KIND_OBSTACLE) {
        // A terrain column carries its height in its subtype; the columns touch, so the terrain looks continuous.
        const std::uint8_t units = registry.getComponents<rtype::game::EntityNetState>().get(event.entity).subtype;
        const float height =
            units == 0 ? DEFAULT_OBSTACLE_HEIGHT : static_cast<float>(units) * rtype::game::TERRAIN_HEIGHT_UNIT;
        applyBox(registry, event.entity, rtype::game::TERRAIN_BLOCK_WIDTH, height, COLOR_TERRAIN, 1);
    } else if (event.kind == rtype::game::KIND_MISSILE) {
        applyBox(registry, event.entity, rtype::game::MISSILE_WIDTH, rtype::game::MISSILE_HEIGHT, COLOR_MISSILE, 2);
    } else if (event.kind == rtype::game::KIND_POWER_UP) {
        applyBox(registry, event.entity, rtype::game::POWER_UP_SIZE, rtype::game::POWER_UP_SIZE, COLOR_POWER_UP, 2);
    } else if (event.kind == rtype::game::KIND_PLAYER_BULLET) {
        applySprite(registry, event.entity,
                    {
                        .texture = _bulletTexId,
                        .frameWidth = BULLET_FRAME_SIZE,
                        .frameHeight = BULLET_FRAME_SIZE,
                        .frameCount = BULLET_FRAME_COUNT,
                        .startY = 0,
                        .layer = 2,
                        .scale = BULLET_SCALE,
                        .frameDuration = BULLET_FRAME_DURATION,
                        .loop = false,
                    });
    }
}

void RTypeGamePlugin::loadBossLooks(render::TextureStore& textures) {
    if (!std::filesystem::exists(looks::BOSS_LOOKS_PATH)) {
        log::info(LOG_TAG, looks::BOSS_LOOKS_PATH, " not found: the bosses are drawn as boxes");
        return;
    }
    std::vector<std::string> warnings;
    _bossLooks = looks::readBossLooks(config::ConfigFile::load(looks::BOSS_LOOKS_PATH), warnings);
    for (const std::string& warning : warnings) {
        log::warning(LOG_TAG, looks::BOSS_LOOKS_PATH, ": ", warning);
    }
    for (auto* table : {&_bossLooks.bosses, &_bossLooks.parts}) {
        for (auto& [id, look] : *table) {
            if (!look.sprite.has_value() || _lookTextures.contains(look.sprite->texture)) {
                continue;
            }
            const ecs::TextureId texture = textures.load(look.sprite->texture);
            if (texture == ecs::NO_TEXTURE) {
                log::warning(LOG_TAG, "cannot load ", look.sprite->texture, ": that look is drawn as a box");
            }
            _lookTextures.emplace(look.sprite->texture, texture);
        }
    }
}

void RTypeGamePlugin::applyLook(ecs::Registry& registry, const ecs::Entity entity, const looks::Look* look,
                                const std::pair<float, float> box, const ecs::Rgba color, const int layer) const {
    if (look != nullptr && look->sprite.has_value()) {
        const looks::SpriteLook& sprite = *look->sprite;
        const auto texture = _lookTextures.find(sprite.texture);
        if (texture != _lookTextures.end() && texture->second != ecs::NO_TEXTURE) {
            applySprite(registry, entity,
                        {
                            .texture = texture->second,
                            .frameWidth = sprite.frameWidth,
                            .frameHeight = sprite.frameHeight,
                            .frameCount = sprite.frames,
                            .startY = sprite.y,
                            .startX = sprite.x,
                            .layer = layer,
                            .scale = sprite.scale,
                            .frameDuration = sprite.frameDuration,
                            .loop = sprite.loop,
                        });
            return;
        }
    }
    const float width = look != nullptr && look->width > 0.0F ? look->width : box.first;
    const float height = look != nullptr && look->height > 0.0F ? look->height : box.second;
    applyBox(registry, entity, width, height, color, layer);
}

void RTypeGamePlugin::update(engine::Engine& engine, const ecs::time::Time& time) {
    _elapsed += time.deltaTime;
    gameplay::blinkInvulnerableShips(engine.registry(), _elapsed);
}

void RTypeGamePlugin::shutdown(engine::Engine& engine) {
    if (_spawned) {
        engine.eventBus().unsubscribe<ecs::net::ReplicatedEntitySpawned>(*_spawned);
        _spawned.reset();
    }
}

}  // namespace client
