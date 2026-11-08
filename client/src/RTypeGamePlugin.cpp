/**
 * @file RTypeGamePlugin.cpp
 * @brief Implementation of RTypeGamePlugin demonstration game logic.
 */

#include "RTypeGamePlugin.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <numbers>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "log/Logger.hpp"
#include "assets/AssetKinds.hpp"
#include "assets/AssetManager.hpp"
#include "assets/render/RenderAssetsPlugin.hpp"
#include "assets/scripting/AssetScriptingPlugin.hpp"
#include "components/AnimationComponent.hpp"
#include "components/CameraComponent.hpp"
#include "components/ParallaxLayer.hpp"
#include "components/Renderable.hpp"
#include "components/SpriteInstances.hpp"
#include "components/Transform.hpp"
#include "config/ConfigFile.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "game/ReplicationCodec.hpp"
#include "gameplay/AimedSprite.hpp"
#include "gameplay/BossEffects.hpp"
#include "gameplay/FaceMotion.hpp"
#include "gameplay/PlayerLives.hpp"
#include "gameplay/ShipBlink.hpp"
#include "input/InputPlugin.hpp"
#include "input/InputState.hpp"
#include "input/RTypeBindings.hpp"
#include "looks/Looks.hpp"
#include "net/ReplicationEvents.hpp"
#include "protocol/Messages.hpp"
#include "render/RenderPlugin.hpp"
#include "systems/render/AnimationPlugin.hpp"
#include "ui/UiPlugin.hpp"

namespace client {

namespace {

namespace log = engine::log;

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
/** @brief Side of the box drawn for an alien gunner until its look is configured, in pixels. */
constexpr float GUNNER_PLACEHOLDER_SIZE = 40.0F;
/** @brief Side of the box drawn for an alien scout restyled without a sprite, in pixels: its hitbox. */
constexpr float SCOUT_BOX_SIZE = 32.0F;
/** @brief Color of an alien gunner drawn as a box. */
constexpr ecs::Rgba COLOR_GUNNER = {.r = 90, .g = 200, .b = 230, .a = 255};
/** @brief Side of the box drawn for an alien elite until its look is configured, in pixels. */
constexpr float ELITE_PLACEHOLDER_SIZE = 56.0F;
/** @brief Color of an alien elite drawn as a box. */
constexpr ecs::Rgba COLOR_ELITE = {.r = 240, .g = 170, .b = 40, .a = 255};
/** @brief Color of an explosion burst drawn as a box. */
constexpr ecs::Rgba COLOR_EXPLOSION = {.r = 255, .g = 190, .b = 70, .a = 255};
/** @brief Draw layer of the explosions, in front of the bosses. */
constexpr int EXPLOSION_LAYER = 4;
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
    int frameStride = 0;
    std::vector<int> frameWidths;
    bool flipX = false;
    int layer = 0;
    float scale = SPRITE_SCALE;
    float frameDuration = DEFAULT_FRAME_DURATION;
    bool loop = true;
};

/** @brief Built-in box of an entity kind: its size and color when `looks.toml` gives it no sprite. */
struct DefaultBox {
    /** @brief Width, in pixels. */
    float width = 0.0F;
    /** @brief Height, in pixels. */
    float height = 0.0F;
    /** @brief Fill color. */
    ecs::Rgba color;
};

/**
 * @brief Gives the built-in box of the kinds `looks.toml` may restyle.
 * @param kind Replicated kind.
 * @return Its box; a white scout-sized box for any other kind.
 */
DefaultBox defaultBox(const std::uint32_t kind) {
    if (kind == rtype::game::KIND_ALIEN_GUNNER) {
        return {.width = GUNNER_PLACEHOLDER_SIZE, .height = GUNNER_PLACEHOLDER_SIZE, .color = COLOR_GUNNER};
    }
    if (kind == rtype::game::KIND_MISSILE) {
        return {.width = rtype::game::MISSILE_WIDTH, .height = rtype::game::MISSILE_HEIGHT, .color = COLOR_MISSILE};
    }
    if (kind == rtype::game::KIND_POWER_UP) {
        return {.width = rtype::game::POWER_UP_SIZE, .height = rtype::game::POWER_UP_SIZE, .color = COLOR_POWER_UP};
    }
    return {.width = SCOUT_BOX_SIZE, .height = SCOUT_BOX_SIZE, .color = COLOR_WHITE};
}

/** @brief Gives a replicated entity its sprite and its size on screen. */
void applySprite(ecs::Registry& registry, const ecs::Entity entity, const EntitySprite& sprite) {
    auto& transform = registry.getComponents<ecs::Transform>().get(entity);
    // A negative scale mirrors the sprite.
    transform.scaleX = sprite.flipX ? -sprite.scale : sprite.scale;
    transform.scaleY = sprite.scale;
    const int firstWidth = sprite.frameWidths.empty() ? sprite.frameWidth : sprite.frameWidths.front();
    registry.addComponent<ecs::Renderable>(
        entity,
        ecs::Renderable{
            .textureId = sprite.texture,
            .sourceRect = {.x = sprite.startX, .y = sprite.startY, .width = firstWidth, .height = sprite.frameHeight},
            .origin = {.x = 0.5F, .y = 0.5F},
            .tint = COLOR_WHITE,
            .layer = sprite.layer,
        });
    registry.addComponent<ecs::AnimationComponent>(entity, ecs::AnimationComponent{
                                                               .frameWidth = sprite.frameWidth,
                                                               .frameHeight = sprite.frameHeight,
                                                               .frameCount = sprite.frameCount,
                                                               .startX = sprite.startX,
                                                               .frameStride = sprite.frameStride,
                                                               .frameWidths = sprite.frameWidths,
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
    // Runs scripts/rtype/Assets.wren, which declares the textures resolved in init.
    builder.require<assets::AssetScriptingPlugin>();
    // Plugs the texture loader into the manager, so that the global group really loads in init.
    builder.require<assets::RenderAssetsPlugin>();
    builder.require<::input::InputPlugin>();
    builder.require<systems::AnimationPlugin>();
    builder.require<ui::UiPlugin>();
}

void RTypeGamePlugin::init(engine::Engine& engine) {
    auto& manager = engine.registry().getResource<assets::AssetManager>();
    manager.loadGroup(assets::AssetManager::GLOBAL_GROUP);
    // Fail fast, as before the asset manager: a sprite missing from the start is a broken install, not something to
    // play through with checkerboards.
    const auto texture = [&manager](const std::string_view alias) {
        switch (manager.status<assets::Texture>(alias)) {
            case assets::AssetStatus::Loaded:
                return manager.get<assets::Texture>(alias);
            case assets::AssetStatus::Failed:
                throw std::runtime_error("cannot load texture '" + std::string(alias) +
                                         "' — is assets/ next to the executable?");
            case assets::AssetStatus::Declared:
            case assets::AssetStatus::Unknown:
                break;
        }
        throw std::runtime_error("texture '" + std::string(alias) +
                                 "' is not declared by scripts/rtype/Assets.wren (or no texture loader is installed)");
    };

    _shipTexId = texture("player_ship");
    // The HUD draws the lives with the ship of the player, at rest.
    engine.registry().emplaceResource<gameplay::ShipIconSheet>(gameplay::ShipIconSheet{
        .texture = _shipTexId,
        .frameWidth = SHIP_FRAME_WIDTH,
        .frameHeight = SHIP_FRAME_HEIGHT,
        .frames = SHIP_FRAME_COUNT,
    });
    _bydosTexId = texture("bydos");
    _bulletTexId = texture("player_bullet");

    _starSmallId = texture("star_small");
    _starMediumId = texture("star_medium");
    _starLargeId = texture("star_large");

    loadLooks(manager);

    // The key bindings come from the settings: `ClientScenesPlugin` sets them up.
    ecs::Registry& registry = engine.registry();
    spawnStarfield(registry, _starSmallId, _starMediumId, _starLargeId);
    registry.addComponent<ecs::CameraComponent>(registry.spawnEntity(), ecs::CameraComponent{});
    // The local player's actions, sent to the server by the network plugins. Nothing of the game is spawned here:
    // ships, enemies and bullets come from the snapshots of the server.
    registry.addComponent<InputState>(registry.spawnEntity(), InputState{.playerIndex = 0, .actions = {}});

    _spawned = engine.eventBus().subscribe<ecs::net::ReplicatedEntitySpawned>(
        [this, &engine](const ecs::net::ReplicatedEntitySpawned& event) { onEntitySpawned(engine, event); });
    _despawned = engine.eventBus().subscribe<ecs::net::ReplicatedEntityDespawned>(
        [this, &engine](const ecs::net::ReplicatedEntityDespawned& event) { onEntityDespawned(engine, event); });
}

void RTypeGamePlugin::onEntitySpawned(engine::Engine& engine, const ecs::net::ReplicatedEntitySpawned& event) const {
    ecs::Registry& registry = engine.registry();
    const std::uint8_t subtype = registry.hasComponent<rtype::game::EntityNetState>(event.entity)
                                     ? registry.getComponents<rtype::game::EntityNetState>().get(event.entity).subtype
                                     : 0;
    if (event.kind == rtype::game::KIND_PLAYER_SHIP) {
        // Each row of the spritesheet is one ship color, so that players can tell their ships apart.
        const int row = subtype;
        applySprite(registry, event.entity,
                    {
                        .texture = _shipTexId,
                        .frameWidth = SHIP_FRAME_WIDTH,
                        .frameHeight = SHIP_FRAME_HEIGHT,
                        .frameCount = SHIP_FRAME_COUNT,
                        .startY = row * SHIP_FRAME_HEIGHT,
                        .layer = 3,
                    });
    } else if (const looks::Look* custom = entityLook(event.kind, subtype);
               custom != nullptr && event.kind != rtype::game::KIND_ALIEN_ELITE) {
        // A look from looks.toml replaces the built-in one; the box keeps the built-in size without a sprite.
        const auto [width, height, color] = defaultBox(event.kind);
        applyLook(registry, event.entity, custom, {width, height}, color, 2);
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
    } else if (event.kind == rtype::game::KIND_ALIEN_GUNNER) {
        applyBox(registry, event.entity, GUNNER_PLACEHOLDER_SIZE, GUNNER_PLACEHOLDER_SIZE, COLOR_GUNNER, 2);
    } else if (event.kind == rtype::game::KIND_ALIEN_ELITE) {
        applyLook(registry, event.entity, entityLook(event.kind, subtype),
                  {ELITE_PLACEHOLDER_SIZE, ELITE_PLACEHOLDER_SIZE}, COLOR_ELITE, 2);
        // Tough enough to need several shots: it flashes on each hit, like a boss.
        const std::uint8_t health = registry.getComponents<rtype::game::EntityNetState>().get(event.entity).health;
        registry.addComponent(
            event.entity,
            gameplay::HitFlash{.spawnHealth = health,
                               .lastHealth = health,
                               .baseTint = registry.getComponents<ecs::Renderable>().get(event.entity).tint});
    } else if (event.kind == rtype::game::KIND_BOSS_CORE || event.kind == rtype::game::KIND_BOSS_PART) {
        applyBossLook(registry, event.entity, event.kind == rtype::game::KIND_BOSS_PART);
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

void RTypeGamePlugin::applyBossLook(ecs::Registry& registry, const ecs::Entity entity, const bool part) const {
    // The subtype picks the look: the variant of a boss, the look of a part.
    const std::uint8_t id = registry.getComponents<rtype::game::EntityNetState>().get(entity).subtype;
    const auto& configured = part ? _looks.parts : _looks.bosses;
    const auto found = configured.find(id);
    const looks::Look* look = found != configured.end() ? &found->second : nullptr;
    if (part) {
        applyLook(registry, entity, look, {BOSS_PART_PLACEHOLDER_SIZE, BOSS_PART_PLACEHOLDER_SIZE}, COLOR_BOSS_PART, 3);
    } else {
        applyLook(registry, entity, look, {rtype::game::BOSS_CORE_WIDTH, rtype::game::BOSS_CORE_HEIGHT}, COLOR_BOSS, 2);
    }
    // A look with damaged stages switches to them as the hit points go down.
    const auto& stages = part ? _looks.partStages : _looks.bossStages;
    if (stages.contains(id)) {
        registry.addComponent(entity, gameplay::LookStage{.id = id, .part = part});
    }
    // Whatever can be worn down (a boss, a turret) flashes on each hit.
    const std::uint8_t health = registry.getComponents<rtype::game::EntityNetState>().get(entity).health;
    if (!part || health > 1) {
        registry.addComponent(
            entity, gameplay::HitFlash{.spawnHealth = health,
                                       .lastHealth = health,
                                       .baseTint = registry.getComponents<ecs::Renderable>().get(entity).tint});
    }
}

void RTypeGamePlugin::onEntityDespawned(engine::Engine& engine,
                                        const ecs::net::ReplicatedEntityDespawned& event) const {
    ecs::Registry& registry = engine.registry();
    if (!registry.hasComponent<gameplay::HitFlash>(event.entity) ||
        !gameplay::diedInCombat(registry.getComponents<gameplay::HitFlash>().get(event.entity)) ||
        !registry.hasComponent<ecs::Transform>(event.entity) || !registry.hasComponent<ecs::Renderable>(event.entity)) {
        return;
    }
    const auto& transform = registry.getComponents<ecs::Transform>().get(event.entity);
    const auto& source = registry.getComponents<ecs::Renderable>().get(event.entity).sourceRect;
    // A box is the placeholder texture scaled up; a sprite is its frame scaled up.
    // A mirrored sprite has a negative scale: only its size matters here.
    const float width =
        (source.width > 0 ? static_cast<float>(source.width) : PLACEHOLDER_PIXELS) * std::fabs(transform.scaleX);
    const float height =
        (source.height > 0 ? static_cast<float>(source.height) : PLACEHOLDER_PIXELS) * std::fabs(transform.scaleY);
    if (event.kind == rtype::game::KIND_BOSS_CORE) {
        spawnExplosion(registry, gameplay::explosionBursts(transform.x, transform.y, width, height));
    } else {
        spawnExplosion(registry, {gameplay::BurstSpec{.x = transform.x, .y = transform.y, .delay = 0.0F}});
    }
}

void RTypeGamePlugin::spawnExplosion(ecs::Registry& registry, const std::vector<gameplay::BurstSpec>& bursts) const {
    const looks::SpriteLook* look = _looks.explosion.has_value() ? &*_looks.explosion : nullptr;
    const auto texture = look != nullptr ? _lookTextures.find(look->texture) : _lookTextures.end();
    if (texture == _lookTextures.end() || texture->second == ecs::NO_TEXTURE) {
        look = nullptr;
    }
    for (const gameplay::BurstSpec& spec : bursts) {
        const ecs::Entity burst = registry.spawnEntity();
        registry.addComponent(burst, ecs::Transform{.x = spec.x, .y = spec.y});
        gameplay::ExplosionBurst state{.delay = spec.delay};
        if (look != nullptr) {
            applySprite(registry, burst,
                        {
                            .texture = texture->second,
                            .frameWidth = look->frameWidth,
                            .frameHeight = look->frameHeight,
                            .frameCount = look->frames,
                            .startY = look->y,
                            .startX = look->x,
                            .layer = EXPLOSION_LAYER,
                            .scale = look->scale,
                            .frameDuration = look->frameDuration,
                            .loop = false,
                        });
            state.startScale = look->scale;
            state.growth = 1.0F;
            state.duration = look->frameDuration * static_cast<float>(look->frames);
            state.tint = COLOR_WHITE;
        } else {
            applyBox(registry, burst, gameplay::EXPLOSION_BURST_SIZE, gameplay::EXPLOSION_BURST_SIZE, COLOR_EXPLOSION,
                     EXPLOSION_LAYER);
            state.startScale = gameplay::EXPLOSION_BURST_SIZE / PLACEHOLDER_PIXELS;
            state.tint = COLOR_EXPLOSION;
        }
        registry.addComponent<gameplay::ExplosionBurst>(burst, state);
    }
}

void RTypeGamePlugin::loadLooks(assets::AssetManager& manager) {
    // Relative to the content root, like the assets: the client runs from any directory.
    const std::filesystem::path path = manager.root() / looks::LOOKS_PATH;
    if (!std::filesystem::exists(path)) {
        log::info(LOG_TAG, looks::LOOKS_PATH, " not found: the bosses are drawn as boxes");
        return;
    }
    std::vector<std::string> warnings;
    _looks = looks::readLooks(config::ConfigFile::load(path), warnings);
    for (const std::string& warning : warnings) {
        log::warning(LOG_TAG, looks::LOOKS_PATH, ": ", warning);
    }
    const auto loadTexture = [this, &manager](const looks::Look& look) {
        if (!look.sprite.has_value() || _lookTextures.contains(look.sprite->texture)) {
            return;
        }
        // Through the asset manager, which caches the files; a file that does not load leaves a box.
        ecs::TextureId texture = manager.load<assets::Texture>(look.sprite->texture);
        if (manager.status<assets::Texture>(look.sprite->texture) != assets::AssetStatus::Loaded) {
            log::warning(LOG_TAG, "cannot load ", look.sprite->texture, ": that look is drawn as a box");
            texture = ecs::NO_TEXTURE;
        }
        _lookTextures.emplace(look.sprite->texture, texture);
    };
    for (auto* table : {&_looks.bosses, &_looks.parts}) {
        for (const auto& [id, look] : *table) {
            loadTexture(look);
        }
    }
    for (const auto& [prefab, look] : _looks.entities) {
        loadTexture(look);
        _entityLooks.emplace(rtype::protocol::makeEntityKind(prefab), &look);
    }
    for (const auto& [key, look] : _looks.entitySubtypes) {
        loadTexture(look);
        _entitySubtypeLooks.emplace(std::pair{rtype::protocol::makeEntityKind(key.first), key.second}, &look);
    }
}

const looks::Look* RTypeGamePlugin::entityLook(const std::uint32_t kind, const std::uint8_t subtype) const {
    if (const auto exact = _entitySubtypeLooks.find({kind, subtype}); exact != _entitySubtypeLooks.end()) {
        return exact->second;
    }
    const auto found = _entityLooks.find(kind);
    return found != _entityLooks.end() ? found->second : nullptr;
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
                            .frameStride = sprite.frameStride,
                            .frameWidths = sprite.frameWidths,
                            .flipX = sprite.flipX,
                            .layer = layer,
                            .scale = sprite.scale,
                            .frameDuration = sprite.frameDuration,
                            .loop = sprite.loop,
                        });
            if (sprite.faceMotion) {
                // A mirrored sheet faces left: that side is the one to turn towards the move.
                registry.addComponent(entity,
                                      gameplay::FaceMotion{.forward = sprite.flipX ? std::numbers::pi_v<float> : 0.0F});
            }
            if (sprite.directional) {
                // Its frames are directions, not an animation: the one facing the ships is picked every frame.
                registry.removeComponent<ecs::AnimationComponent>(entity);
                registry.addComponent(entity, gameplay::AimedSprite{.startX = sprite.x,
                                                                    .startY = sprite.y,
                                                                    .frameWidth = sprite.frameWidth,
                                                                    .frameHeight = sprite.frameHeight,
                                                                    .frameStride = sprite.frameStride,
                                                                    .frames = sprite.frames,
                                                                    .firstAngle = sprite.firstAngle});
            }
            return;
        }
    }
    const float width = look != nullptr && look->width > 0.0F ? look->width : box.first;
    const float height = look != nullptr && look->height > 0.0F ? look->height : box.second;
    applyBox(registry, entity, width, height, color, layer);
}

void RTypeGamePlugin::updateLookStages(ecs::Registry& registry) const {
    // Collected first: switching a look replaces components of the entity, which must not happen mid-iteration.
    std::vector<std::pair<ecs::Entity, int>> changes;
    registry.runSystem<gameplay::LookStage, gameplay::HitFlash, rtype::game::EntityNetState>(
        [this, &changes](const ecs::Entity entity, const gameplay::LookStage& stage, const gameplay::HitFlash& flash,
                         const rtype::game::EntityNetState& state) {
            const auto& table = stage.part ? _looks.partStages : _looks.bossStages;
            const auto found = table.find(stage.id);
            if (found == table.end()) {
                return;
            }
            std::vector<float> thresholds;
            thresholds.reserve(found->second.size());
            for (const looks::HealthStage& entry : found->second) {
                thresholds.push_back(entry.belowHealth);
            }
            const int wanted = gameplay::healthStage(thresholds, state.health, flash.spawnHealth);
            if (wanted != stage.current) {
                changes.emplace_back(entity, wanted);
            }
        });
    for (const auto& [entity, wanted] : changes) {
        auto& stage = registry.getComponents<gameplay::LookStage>().get(entity);
        stage.current = wanted;
        const looks::Look* look = nullptr;
        if (wanted >= 0) {
            look = &(stage.part ? _looks.partStages : _looks.bossStages)
                        .at(stage.id)
                        .at(static_cast<std::size_t>(wanted))
                        .look;
        } else {
            const auto& base = stage.part ? _looks.parts : _looks.bosses;
            const auto found = base.find(stage.id);
            look = found != base.end() ? &found->second : nullptr;
        }
        if (stage.part) {
            applyLook(registry, entity, look, {BOSS_PART_PLACEHOLDER_SIZE, BOSS_PART_PLACEHOLDER_SIZE}, COLOR_BOSS_PART,
                      3);
        } else {
            applyLook(registry, entity, look, {rtype::game::BOSS_CORE_WIDTH, rtype::game::BOSS_CORE_HEIGHT}, COLOR_BOSS,
                      2);
        }
        // The flash goes back to the tint of the new look.
        registry.getComponents<gameplay::HitFlash>().get(entity).baseTint =
            registry.getComponents<ecs::Renderable>().get(entity).tint;
    }
}

void RTypeGamePlugin::update(engine::Engine& engine, const ecs::time::Time& time) {
    _elapsed += time.deltaTime;
    updateLookStages(engine.registry());
    gameplay::blinkInvulnerableShips(engine.registry(), _elapsed);
    gameplay::updateHitFlashes(engine.registry(), time.deltaTime);
    gameplay::updateExplosions(engine.registry(), time.deltaTime);
    gameplay::aimSprites(engine.registry());
}

void RTypeGamePlugin::lateUpdate(engine::Engine& engine, const ecs::time::Time& /*time*/) {
    // After the interpolation of the update stage, which writes the positions and the rotations.
    gameplay::faceMotion(engine.registry());
}

void RTypeGamePlugin::shutdown(engine::Engine& engine) {
    if (_spawned) {
        engine.eventBus().unsubscribe<ecs::net::ReplicatedEntitySpawned>(*_spawned);
        _spawned.reset();
    }
    if (_despawned) {
        engine.eventBus().unsubscribe<ecs::net::ReplicatedEntityDespawned>(*_despawned);
        _despawned.reset();
    }
}

}  // namespace client
