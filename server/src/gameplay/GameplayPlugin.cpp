#include "gameplay/GameplayPlugin.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
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
#include "prefabs/PrefabParams.hpp"
#include "prefabs/alien_scout/AlienScout.hpp"
#include "prefabs/missile/Missile.hpp"
#include "prefabs/player_bullet/PlayerBullet.hpp"
#include "prefabs/power_up/PowerUp.hpp"
#include "protocol/Messages.hpp"
#include "systems/physics/PhysicsPlugin.hpp"
#include "timeline/TimelineEvent.hpp"
#include "timeline/TimelineEvents.hpp"
#include "timeline/TimelinePlugin.hpp"

namespace rtype::server::gameplay {

namespace {

constexpr float SCENE_WIDTH = 1280.0F;
/** @brief Margin past the screen edges after which bullets and enemies are destroyed. */
constexpr float OFFSCREEN_MARGIN = 64.0F;
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
    return prefab == ::prefabs::ALIEN_SCOUT_PREFAB;
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
}

void GameplayPlugin::onLevelEvent(const timeline::TimelineEventFired& fired) {
    const timeline::TimelineEvent& event = fired.event;
    if (event.type == level::EVENT_SPAWN) {
        spawnFromLevel(event.params);
    } else if (event.type == level::EVENT_SCROLL) {
        _timeline->setSpeed(event.params.get("speed", _timeline->speed()));
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
    if ((hit.targetCategory & ::prefabs::collision::ENEMY) != 0 && registry.isAlive(hit.target)) {
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

void GameplayPlugin::fixedUpdate(engine::Engine& /*engine*/, const ecs::time::Time& time) {
    for (float& cooldown : _shotCooldowns) {
        cooldown = std::max(cooldown - time.deltaTime, 0.0F);
    }
    for (const PendingShot& shot : _pendingShots) {
        fire(shot);
    }
    _pendingShots.clear();

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
