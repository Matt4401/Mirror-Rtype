#include "gameplay/GameplayPlugin.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numeric>
#include <span>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "LogTag.hpp"
#include "Logger.hpp"
#include "collision/CollisionRouter.hpp"
#include "collision/GameCollisionEvents.hpp"
#include "components/Replicated.hpp"
#include "components/Transform.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "game/ReplicationCodec.hpp"
#include "game/ShipMovement.hpp"
#include "net/ReplicationEvents.hpp"
#include "net/ReplicationSystems.hpp"
#include "prefabs/CollisionCategories.hpp"
#include "prefabs/PrefabParams.hpp"
#include "prefabs/alien_scout/AlienScout.hpp"
#include "prefabs/player_bullet/PlayerBullet.hpp"
#include "protocol/Messages.hpp"
#include "systems/physics/PhysicsPlugin.hpp"

namespace rtype::server::gameplay {

namespace {

constexpr float SCENE_WIDTH = 1280.0F;
/** @brief Margin past the screen edges after which bullets and enemies are destroyed. */
constexpr float OFFSCREEN_MARGIN = 64.0F;
/** @brief Spawn abscissa of the enemies, just past the right edge. */
constexpr float ENEMY_SPAWN_X = SCENE_WIDTH + 48.0F;
/** @brief Spawn heights, cycled through so the waves are deterministic. */
constexpr std::array<float, 6> ENEMY_SPAWN_HEIGHTS{160.0F, 420.0F, 280.0F, 560.0F, 220.0F, 480.0F};
/** @brief Horizontal offset of a bullet from the center of the ship that fires it. */
constexpr float SHOT_OFFSET_X = 40.0F;

/**
 * @brief Default level rule: the kill quota is reached, or the level lasted its maximum duration.
 * @param progress Progress of the level.
 * @param level The level.
 * @return `true` if the level is cleared.
 */
bool quotaOrDuration(const LevelProgress& progress, const LevelInfo& level) {
    return (level.killQuota > 0 && progress.kills >= level.killQuota) ||
           (level.maxDuration > 0.0F && progress.elapsed >= level.maxDuration);
}

}  // namespace

std::vector<LevelInfo> defaultLevels() {
    return {
        LevelInfo{.name = "Outer Rim", .killQuota = 10, .maxDuration = 60.0F, .spawnInterval = 1.2F},
        LevelInfo{.name = "Asteroid Belt", .killQuota = 15, .maxDuration = 75.0F, .spawnInterval = 0.9F},
        LevelInfo{.name = "Bydo Core", .killQuota = 20, .maxDuration = 90.0F, .spawnInterval = 0.7F},
    };
}

void GameplayPlugin::setLevels(std::vector<LevelInfo> levels) {
    if (levels.empty()) {
        return;
    }
    if (levels.size() > protocol::MAX_LEVELS) {
        levels.resize(protocol::MAX_LEVELS);
    }
    _levels = std::move(levels);
}

void GameplayPlugin::setLevelFinished(LevelFinishedRule rule) {
    _levelFinished = rule ? std::move(rule) : LevelFinishedRule{quotaOrDuration};
}

void GameplayPlugin::startGame() {
    _levelIndex = 0;
    _shipsSeen = false;
    beginLevel();
}

void GameplayPlugin::placeShips() {
    _registry->runSystem<ecs::Transform, ecs::Replicated, game::EntityNetState>(
        [](ecs::Entity /*entity*/, ecs::Transform& transform, const ecs::Replicated& replicated,
           const game::EntityNetState& state) {
            if (replicated.kind != game::KIND_PLAYER_SHIP) {
                return;
            }
            transform.x = SHIP_SPAWN_X;
            transform.y = SHIP_SPAWN_FIRST_Y + (SHIP_SPAWN_SPACING_Y * static_cast<float>(state.subtype));
        });
}

void GameplayPlugin::beginLevel() {
    const LevelInfo& level = _levels.at(_levelIndex);
    _phase = Phase::Playing;
    _progress = LevelProgress{.index = _levelIndex};
    _enemySpawnTimer = level.spawnInterval;
    log::info(LOG_TAG, "level ", _levelIndex + 1, '/', _levels.size(), " \"", level.name, "\" starts");
    _bus->publish(protocol::LevelStart{
        .levelIndex = static_cast<std::uint8_t>(_levelIndex),
        .levelCount = static_cast<std::uint8_t>(_levels.size()),
        .name = level.name.substr(0, protocol::MAX_LEVEL_NAME_LENGTH),
    });
}

void GameplayPlugin::clearLevel() {
    killLevelEntities();
    placeShips();
    _pendingShots.clear();
    log::info(LOG_TAG, "level ", _levelIndex + 1, " cleared");
    _bus->publish(protocol::LevelComplete{
        .levelIndex = static_cast<std::uint8_t>(_levelIndex),
        .score = totalScore(),
        .playerScores = playerScores(),
    });
    if (_levelIndex + 1 >= _levels.size()) {
        endGame(protocol::GameOverReason::Victory);
        return;
    }
    ++_levelIndex;
    _phase = Phase::Intermission;
    _intermission = INTERMISSION_DURATION;
}

void GameplayPlugin::endGame(protocol::GameOverReason reason) {
    _phase = Phase::Over;
    killLevelEntities();
    log::info(LOG_TAG, "game over (", reason == protocol::GameOverReason::Victory ? "victory" : "defeat", "), score ",
              totalScore());
    _bus->publish(protocol::GameOver{.reason = reason, .score = totalScore(), .playerScores = playerScores()});
}

void GameplayPlugin::killLevelEntities() {
    ecs::Registry& registry = *_registry;
    _toDestroy.clear();
    registry.runSystem<ecs::Replicated>([this](const ecs::Entity entity, const ecs::Replicated& replicated) {
        if (replicated.kind != game::KIND_PLAYER_SHIP) {
            _toDestroy.push_back(entity);
        }
    });
    registry.killEntities(_toDestroy);
    _toDestroy.clear();
}

bool GameplayPlugin::everyShipDestroyed() {
    bool anyShip = false;
    _registry->runSystem<ecs::Replicated>([&anyShip](const ecs::Entity /*entity*/, const ecs::Replicated& replicated) {
        anyShip = anyShip || replicated.kind == game::KIND_PLAYER_SHIP;
    });
    _shipsSeen = _shipsSeen || anyShip;
    return _shipsSeen && !anyShip;
}

std::vector<std::uint32_t> GameplayPlugin::playerScores() const {
    const auto played = std::span(_scores).first(_playerCount);
    return {played.begin(), played.end()};
}

std::uint32_t GameplayPlugin::totalScore() const {
    const auto played = std::span(_scores).first(_playerCount);
    return std::accumulate(played.begin(), played.end(), std::uint32_t{0});
}

void GameplayPlugin::build(engine::PluginBuilder& builder) { builder.require<systems::PhysicsPlugin>(); }

void GameplayPlugin::init(engine::Engine& engine) {
    if (!_levelFinished) {
        _levelFinished = quotaOrDuration;
    }
    _registry = &engine.registry();
    _bus = &engine.eventBus();
    _router = std::make_unique<collision::CollisionRouter>(*_bus);
    _inputToken =
        _bus->subscribe<ecs::net::InputApplied>([this](const ecs::net::InputApplied& event) { onInput(event); });
    _hitToken =
        _bus->subscribe<collision::ShotHitEvent>([this](const collision::ShotHitEvent& hit) { onShotHit(hit); });
}

void GameplayPlugin::shutdown(engine::Engine& /*engine*/) {
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
    if ((hit.targetCategory & ::prefabs::collision::ENEMY) != 0 && registry.isAlive(hit.target)) {
        if (registry.hasComponent<game::EntityNetState>(hit.shot)) {
            const std::uint8_t shooter = registry.getComponents<game::EntityNetState>().get(hit.shot).subtype;
            if (shooter < _scores.size()) {
                _scores.at(shooter) += POINTS_PER_KILL;
            }
            ++_progress.kills;
        }
        registry.killEntity(hit.target);
    }
    registry.killEntity(hit.shot);
}

void GameplayPlugin::fixedUpdate(engine::Engine& /*engine*/, const ecs::time::Time& time) {
    for (float& cooldown : _shotCooldowns) {
        cooldown = std::max(cooldown - time.deltaTime, 0.0F);
    }
    for (const PendingShot& shot : _pendingShots) {
        fire(shot);
    }
    _pendingShots.clear();

    switch (_phase) {
        case Phase::Over:
            return;
        case Phase::Intermission:
            _intermission -= time.deltaTime;
            if (_intermission <= 0.0F) {
                beginLevel();
            }
            break;
        case Phase::Free:
        case Phase::Playing: {
            const float interval =
                _phase == Phase::Playing ? _levels.at(_levelIndex).spawnInterval : ENEMY_SPAWN_INTERVAL;
            _enemySpawnTimer -= time.deltaTime;
            if (_enemySpawnTimer <= 0.0F) {
                _enemySpawnTimer += interval;
                spawnEnemy();
            }
            break;
        }
    }

    killOffscreenEntities();

    if (_phase == Phase::Playing) {
        _progress.elapsed += time.deltaTime;
        if (everyShipDestroyed()) {
            endGame(protocol::GameOverReason::AllDead);
            return;
        }
        if (_levelFinished(_progress, _levels.at(_levelIndex))) {
            clearLevel();
        }
    }

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

void GameplayPlugin::spawnEnemy() {
    ecs::Registry& registry = *_registry;
    const float y = ENEMY_SPAWN_HEIGHTS.at(_enemiesSpawned % ENEMY_SPAWN_HEIGHTS.size());
    ++_enemiesSpawned;
    const ecs::Entity enemy = registry.spawnEntity();
    ::prefabs::buildAlienScout(enemy, registry, {{"x", ENEMY_SPAWN_X}, {"y", y}});
    registry.addComponent(enemy, game::EntityNetState{});
    ecs::net::replicate(registry, enemy, game::KIND_ALIEN_SCOUT);
}

void GameplayPlugin::killOffscreenEntities() {
    ecs::Registry& registry = *_registry;
    std::vector<ecs::Entity> toDestroy;
    registry.runSystem<ecs::Transform, ecs::Replicated>(
        [&toDestroy](const ecs::Entity entity, const ecs::Transform& transform, const ecs::Replicated& replicated) {
            const bool bulletGone =
                replicated.kind == game::KIND_PLAYER_BULLET && transform.x > SCENE_WIDTH + OFFSCREEN_MARGIN;
            const bool enemyGone = replicated.kind == game::KIND_ALIEN_SCOUT && transform.x < -OFFSCREEN_MARGIN;
            if (bulletGone || enemyGone) {
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
