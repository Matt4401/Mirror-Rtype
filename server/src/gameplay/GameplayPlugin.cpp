#include "gameplay/GameplayPlugin.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "LogTag.hpp"
#include "Logger.hpp"
#include "collision/CollisionRouter.hpp"
#include "collision/GameCollisionEvents.hpp"
#include "components/Replicated.hpp"
#include "components/RigidBody2D.hpp"
#include "components/ScriptComponent.hpp"
#include "components/Transform.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "game/ReplicationCodec.hpp"
#include "game/ShipMovement.hpp"
#include "level/Level.hpp"
#include "net/ReplicationEvents.hpp"
#include "net/ReplicationSystems.hpp"
#include "prefabs/CollisionCategories.hpp"
#include "prefabs/Health.hpp"
#include "prefabs/PrefabParams.hpp"
#include "prefabs/alien_scout/AlienScout.hpp"
#include "prefabs/boss_core/BossCore.hpp"
#include "prefabs/missile/Missile.hpp"
#include "prefabs/obstacle/Obstacle.hpp"
#include "prefabs/player_bullet/PlayerBullet.hpp"
#include "prefabs/player_ship/PlayerShip.hpp"
#include "prefabs/power_up/PowerUp.hpp"
#include "protocol/Messages.hpp"
#include "systems/physics/PhysicsPlugin.hpp"
#include "timeline/TimelineEvent.hpp"
#include "timeline/TimelineEvents.hpp"
#include "timeline/TimelinePlugin.hpp"

namespace rtype::server::gameplay {

namespace {

constexpr float SCENE_WIDTH = 1280.0F;
/** @brief Height of the scene in world units (pixels). */
constexpr float SCENE_HEIGHT = 720.0F;
/** @brief Margin past the screen edges after which bullets and enemies are destroyed. */
constexpr float OFFSCREEN_MARGIN = 64.0F;
/** @brief Marks a terrain column, built by the gameplay from a level `terrain` event. */
struct TerrainColumn {
    /** @brief Half-width of the column. */
    float halfWidth = game::TERRAIN_COLUMN_WIDTH / 2.0F;
    /** @brief Half-height of the column. */
    float halfHeight = 0.0F;
};

/**
 * @brief Tells whether two boxes overlap.
 * @param a Center of the first box.
 * @param aHalfW Half-width of the first box.
 * @param aHalfH Half-height of the first box.
 * @param b Center of the second box.
 * @param bHalfW Half-width of the second box.
 * @param bHalfH Half-height of the second box.
 * @return True if they overlap (touching edges do not).
 */
bool overlaps(const ecs::Transform& a, float aHalfW, float aHalfH, const ecs::Transform& b, float bHalfW,
              float bHalfH) noexcept {
    return std::fabs(a.x - b.x) < aHalfW + bHalfW && std::fabs(a.y - b.y) < aHalfH + bHalfH;
}

/**
 * @brief Tells whether entities of a kind fly towards the players and are gone once past the left edge: enemies, their
 * shots and the power-ups (some are spawned by scripts).
 * @param kind Replicated kind.
 * @return True for the kinds removed past the left edge.
 */
bool flowsLeft(const std::uint32_t kind) noexcept {
    return kind == game::KIND_ALIEN_SCOUT || kind == protocol::makeEntityKind(::prefabs::MISSILE_PREFAB) ||
           kind == protocol::makeEntityKind(::prefabs::POWER_UP_PREFAB);
}

/** @brief Horizontal offset of a bullet from the center of the ship that fires it. */
constexpr float SHOT_OFFSET_X = 40.0F;

}  // namespace

GameplayPlugin::GameplayPlugin(std::shared_ptr<const level::Level> level) : _level(std::move(level)) {}

void GameplayPlugin::build(engine::PluginBuilder& builder) {
    builder.require<systems::PhysicsPlugin>();
    builder.require<timeline::TimelinePlugin>();
}

bool GameplayPlugin::canSpawn(const std::string_view prefab) noexcept {
    return prefab == ::prefabs::ALIEN_SCOUT_PREFAB || prefab == ::prefabs::BOSS_CORE_PREFAB;
}

void GameplayPlugin::init(engine::Engine& engine) {
    _registry = &engine.registry();
    _bus = &engine.eventBus();
    _router = std::make_unique<collision::CollisionRouter>(*_bus);
    _inputToken =
        _bus->subscribe<ecs::net::InputApplied>([this](const ecs::net::InputApplied& event) { onInput(event); });
    _hitToken =
        _bus->subscribe<collision::ShotHitEvent>([this](const collision::ShotHitEvent& hit) { onShotHit(hit); });
    _levelToken = _bus->subscribe<timeline::TimelineEventFired>(
        [this](const timeline::TimelineEventFired& fired) { onLevelEvent(fired); });
    _timeline = &engine.getPlugin<timeline::TimelinePlugin>();
    startLevel();
}

void GameplayPlugin::startLevel() {
    if (_timeline == nullptr || _level == nullptr) {
        return;
    }
    _timeline->timeline() = _level->timeline;
    _timeline->setSpeed(_level->scrollSpeed);
    _bossFight = false;
    _terrain.clear();
}

void GameplayPlugin::onLevelEvent(const timeline::TimelineEventFired& fired) {
    const timeline::TimelineEvent& event = fired.event;
    if (event.type == level::EVENT_SPAWN) {
        spawnFromLevel(event.params);
    } else if (event.type == level::EVENT_SCROLL) {
        _timeline->setSpeed(event.params.get("speed", _timeline->speed()));
    } else if (event.type == level::EVENT_TERRAIN) {
        startTerrain(event.params);
    } else if (event.type == level::EVENT_BOSS) {
        startBossFight(event.params);
    } else if (event.type == level::EVENT_END) {
        log::info(LOG_TAG, "level '", _level != nullptr ? _level->name : "", "' complete, restarting it");
        startLevel();
    }
}

void GameplayPlugin::spawnFromLevel(const ::prefabs::PrefabParams& params) {
    // The loader only keeps the prefabs accepted by canSpawn(), so the name is known here.
    if (params.get<std::string>("prefab", "") != ::prefabs::ALIEN_SCOUT_PREFAB) {
        return;
    }
    ecs::Registry& registry = *_registry;
    const ecs::Entity enemy = registry.spawnEntity();
    ::prefabs::buildAlienScout(enemy, registry, params);
    registry.addComponent(enemy, game::EntityNetState{});
    ecs::net::replicate(registry, enemy, game::KIND_ALIEN_SCOUT);
}

void GameplayPlugin::startTerrain(const ::prefabs::PrefabParams& params) {
    // Values were validated by the loader; the height is rounded to what a column's subtype can carry.
    const double units = std::round(params.get("height", 0.0) / static_cast<double>(game::TERRAIN_HEIGHT_UNIT));
    const float height = static_cast<float>(std::clamp(units, 1.0, 255.0)) * game::TERRAIN_HEIGHT_UNIT;
    const double length = std::max(params.get("length", 0.0), 0.0);
    const bool top = params.get<std::string>("side", "") == level::TERRAIN_TOP;
    _terrain.push_back(TerrainSection{
        .y = top ? height / 2.0F : SCENE_HEIGHT - (height / 2.0F),
        .height = height,
        .columnsLeft = static_cast<std::size_t>(std::ceil(length / static_cast<double>(game::TERRAIN_COLUMN_WIDTH))),
        .lastColumn = std::nullopt,
    });
}

void GameplayPlugin::updateTerrain() {
    ecs::Registry& registry = *_registry;
    const float velocity = -_timeline->speed();
    for (TerrainSection& section : _terrain) {
        while (section.columnsLeft > 0) {
            // Right edge of the section so far: the right edge of the screen before its first column.
            float rightEdge = SCENE_WIDTH;
            if (section.lastColumn && registry.isAlive(*section.lastColumn)) {
                rightEdge = registry.getComponents<ecs::Transform>().get(*section.lastColumn).x +
                            (game::TERRAIN_COLUMN_WIDTH / 2.0F);
            }
            if (rightEdge > SCENE_WIDTH + OFFSCREEN_MARGIN) {
                break;
            }
            const ecs::Entity column = registry.spawnEntity();
            ::prefabs::buildObstacle(column, registry,
                                     {{"x", rightEdge + (game::TERRAIN_COLUMN_WIDTH / 2.0F)},
                                      {"y", section.y},
                                      {"halfWidth", game::TERRAIN_COLUMN_WIDTH / 2.0F},
                                      {"halfHeight", section.height / 2.0F}});
            // Moved by the gameplay at the scroll speed: kinematic, unlike a static obstacle.
            registry.getComponents<ecs::RigidBody2D>().get(column).bodyType = ecs::BodyType::Kinematic;
            registry.addComponent(column, TerrainColumn{.halfHeight = section.height / 2.0F});
            registry.addComponent(
                column,
                game::EntityNetState{.subtype = static_cast<std::uint8_t>(section.height / game::TERRAIN_HEIGHT_UNIT)});
            ecs::net::replicate(registry, column, game::KIND_OBSTACLE);
            section.lastColumn = column;
            --section.columnsLeft;
        }
    }
    std::erase_if(_terrain, [](const TerrainSection& section) { return section.columnsLeft == 0; });
    registry.runSystem<TerrainColumn, ecs::RigidBody2D>(
        [velocity](ecs::Entity /*column*/, const TerrainColumn& /*tag*/, ecs::RigidBody2D& body) {
            body.velocityX = velocity;
        });
}

void GameplayPlugin::crashShipsIntoTerrain() {
    ecs::Registry& registry = *_registry;
    std::vector<std::pair<ecs::Transform, TerrainColumn>> columns;
    registry.runSystem<TerrainColumn, ecs::Transform>(
        [&columns](ecs::Entity /*column*/, const TerrainColumn& column, const ecs::Transform& transform) {
            columns.emplace_back(transform, column);
        });
    if (columns.empty()) {
        return;
    }
    std::vector<ecs::Entity> crashed;
    registry.runSystem<ecs::Transform, ecs::Replicated>(
        [&](const ecs::Entity ship, const ecs::Transform& transform, const ecs::Replicated& replicated) {
            if (replicated.kind != game::KIND_PLAYER_SHIP) {
                return;
            }
            const bool hit = std::ranges::any_of(columns, [&transform](const auto& column) {
                return overlaps(transform, ::prefabs::PLAYER_SHIP_HALF_WIDTH, ::prefabs::PLAYER_SHIP_HALF_HEIGHT,
                                column.first, column.second.halfWidth, column.second.halfHeight);
            });
            if (hit) {
                crashed.push_back(ship);
            }
        });
    for (const ecs::Entity ship : crashed) {
        log::info(LOG_TAG, "a ship crashed into the terrain");
        registry.killEntity(ship);
    }
}

void GameplayPlugin::startBossFight(const ::prefabs::PrefabParams& params) {
    // The loader only keeps the prefabs accepted by canSpawn(): a boss event names boss_core.
    if (params.get<std::string>("prefab", "") != ::prefabs::BOSS_CORE_PREFAB) {
        return;
    }
    if (!_bossFight) {
        _bossFight = true;
        _speedBeforeBoss = _timeline->speed();
        _timeline->setSpeed(0.0F);
    }
    ecs::Registry& registry = *_registry;
    const ecs::Entity boss = registry.spawnEntity();
    ::prefabs::buildBossCore(boss, registry, params);
    registry.addComponent(boss,
                          game::EntityNetState{.health = registry.getComponents<::prefabs::Health>().get(boss).points});
    if (const auto script = params.find<std::string>("script")) {
        // `scripts/Boss.wren` is the module `<data root>/scripts/Boss`, whose class is `Boss` unless the event names
        // another one.
        const std::filesystem::path path = (_level != nullptr ? _level->dataRoot : std::filesystem::path{}) / *script;
        registry.addComponent(boss, ecs::ScriptComponent{
                                        .moduleName = (path.parent_path() / path.stem()).generic_string(),
                                        .className = params.get<std::string>("class", path.stem().string()),
                                        .hasUpdate = true,
                                    });
    }
    ecs::net::replicate(registry, boss, game::KIND_BOSS_CORE);
    log::info(LOG_TAG, "boss fight: the scroll stops until the boss is destroyed");
}

void GameplayPlugin::updateBossFight() {
    ecs::Registry& registry = *_registry;
    bool bossLeft = false;
    registry.runSystem<::prefabs::BossCore, ecs::Transform, ecs::RigidBody2D>(
        [&bossLeft](ecs::Entity /*boss*/, const ::prefabs::BossCore& core, const ecs::Transform& transform,
                    ecs::RigidBody2D& body) {
            bossLeft = true;
            // Flying in: stops once in place. A script may move it afterwards.
            if (body.velocityX < 0.0F && transform.x <= core.stopX) {
                body.velocityX = 0.0F;
            }
        });
    if (_bossFight && !bossLeft) {
        _bossFight = false;
        _timeline->setSpeed(_speedBeforeBoss);
        log::info(LOG_TAG, "boss destroyed: the level goes on");
    }
}

void GameplayPlugin::shutdown(engine::Engine& /*engine*/) {
    if (_levelToken) {
        _bus->unsubscribe<timeline::TimelineEventFired>(*_levelToken);
        _levelToken.reset();
    }
    _timeline = nullptr;
    if (_hitToken) {
        _bus->unsubscribe<collision::ShotHitEvent>(*_hitToken);
        _hitToken.reset();
    }
    if (_inputToken) {
        _bus->unsubscribe<ecs::net::InputApplied>(*_inputToken);
        _inputToken.reset();
    }
    _router.reset();
}

void GameplayPlugin::setPlayerCount(const std::size_t playerCount) noexcept {
    _playerCount = std::min(playerCount, _scores.size());
}

std::uint32_t GameplayPlugin::score(const std::size_t playerIndex) const noexcept {
    return playerIndex < _scores.size() ? _scores.at(playerIndex) : 0;
}

void GameplayPlugin::onInput(const ecs::net::InputApplied& event) {
    if (!std::ranges::contains(event.input.held, game::ACTION_SHOOT)) {
        return;
    }
    const ecs::Registry& registry = *_registry;
    if (!registry.hasComponent<game::EntityNetState>(event.entity) ||
        !registry.hasComponent<ecs::Transform>(event.entity)) {
        return;
    }
    const std::uint8_t index = registry.getComponents<game::EntityNetState>().get(event.entity).subtype;
    if (index >= _shotCooldowns.size() || _shotCooldowns.at(index) > 0.0F) {
        return;
    }
    _shotCooldowns.at(index) = SHOT_COOLDOWN;
    const auto& transform = registry.getComponents<ecs::Transform>().get(event.entity);
    // Fired at the next fixed step: the inputs are applied while the replication plugin walks the input buffers.
    _pendingShots.push_back({.playerIndex = index, .x = transform.x, .y = transform.y});
}

void GameplayPlugin::onShotHit(const collision::ShotHitEvent& hit) {
    ecs::Registry& registry = *_registry;
    if (!hit.byPlayer || !registry.isAlive(hit.shot)) {
        return;
    }
    // The contacts are published after the physics step: a shot touching two enemies in the same step only gets one.
    if ((hit.targetCategory & ::prefabs::collision::ENEMY) != 0 && registry.isAlive(hit.target) &&
        takeHit(hit.target)) {
        if (registry.hasComponent<game::EntityNetState>(hit.shot)) {
            const std::uint8_t shooter = registry.getComponents<game::EntityNetState>().get(hit.shot).subtype;
            if (shooter < _scores.size()) {
                _scores.at(shooter) += POINTS_PER_KILL;
            }
        }
        registry.killEntity(hit.target);
    }
    registry.killEntity(hit.shot);
}

bool GameplayPlugin::takeHit(const ecs::Entity target) {
    ecs::Registry& registry = *_registry;
    if (!registry.hasComponent<::prefabs::Health>(target)) {
        return true;
    }
    ::prefabs::Health& health = registry.getComponents<::prefabs::Health>().get(target);
    if (health.points > 0) {
        --health.points;
    }
    if (registry.hasComponent<game::EntityNetState>(target)) {
        registry.getComponents<game::EntityNetState>().get(target).health = health.points;
    }
    return health.points == 0;
}

void GameplayPlugin::fixedUpdate(engine::Engine& /*engine*/, const ecs::time::Time& time) {
    for (float& cooldown : _shotCooldowns) {
        cooldown = std::max(cooldown - time.deltaTime, 0.0F);
    }
    for (const PendingShot& shot : _pendingShots) {
        fire(shot);
    }
    _pendingShots.clear();

    updateBossFight();
    updateTerrain();
    crashShipsIntoTerrain();
    killOffscreenEntities();

    _scoreLogTimer -= time.deltaTime;
    if (_scoreLogTimer <= 0.0F) {
        _scoreLogTimer += SCORE_LOG_INTERVAL;
        logScores();
    }
}

void GameplayPlugin::fire(const PendingShot& shot) {
    ecs::Registry& registry = *_registry;
    const ecs::Entity bullet = registry.spawnEntity();
    ::prefabs::buildPlayerBullet(bullet, registry, {{"x", shot.x + SHOT_OFFSET_X}, {"y", shot.y}});
    registry.addComponent(bullet, game::EntityNetState{.subtype = shot.playerIndex});
    ecs::net::replicate(registry, bullet, game::KIND_PLAYER_BULLET);
}

void GameplayPlugin::killOffscreenEntities() {
    ecs::Registry& registry = *_registry;
    std::vector<ecs::Entity> toDestroy;
    registry.runSystem<ecs::Transform, ecs::Replicated>(
        [&toDestroy](const ecs::Entity entity, const ecs::Transform& transform, const ecs::Replicated& replicated) {
            const bool bulletGone =
                replicated.kind == game::KIND_PLAYER_BULLET && transform.x > SCENE_WIDTH + OFFSCREEN_MARGIN;
            const bool enemyGone = flowsLeft(replicated.kind) && transform.x < -OFFSCREEN_MARGIN;
            const bool terrainGone = replicated.kind == game::KIND_OBSTACLE &&
                                     transform.x < -OFFSCREEN_MARGIN - (game::TERRAIN_COLUMN_WIDTH / 2.0F);
            if (bulletGone || enemyGone || terrainGone) {
                toDestroy.push_back(entity);
            }
        });
    for (const ecs::Entity entity : toDestroy) {
        registry.killEntity(entity);
    }
}

void GameplayPlugin::logScores() const {
    std::ostringstream line;
    line << "scores:";
    for (std::size_t index = 0; index < _playerCount; ++index) {
        line << " P" << (index + 1) << '=' << _scores.at(index);
    }
    log::info(LOG_TAG, line.str());
}

}  // namespace rtype::server::gameplay
