#include "gameplay/CombatPlugin.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numeric>
#include <optional>
#include <span>
#include <sstream>
#include <vector>

#include "LogTag.hpp"
#include "Logger.hpp"
#include "collision/CollisionRouter.hpp"
#include "collision/GameCollisionEvents.hpp"
#include "components/Transform.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "game/ReplicationCodec.hpp"
#include "game/ShipMovement.hpp"
#include "gameplay/GameplayComponents.hpp"
#include "gameplay/GameplayEvents.hpp"
#include "hierarchy/Hierarchy.hpp"
#include "net/ReplicationEvents.hpp"
#include "net/ReplicationSystems.hpp"
#include "prefabs/CollisionCategories.hpp"
#include "prefabs/EnemyMotion.hpp"
#include "prefabs/Health.hpp"
#include "prefabs/boss_core/BossCore.hpp"
#include "prefabs/boss_part/BossPart.hpp"
#include "prefabs/player_bullet/PlayerBullet.hpp"
#include "prefabs/power_up/PowerUp.hpp"
#include "protocol/Messages.hpp"
#include "systems/physics/PhysicsPlugin.hpp"

namespace rtype::server::gameplay {

namespace {

/** @brief Horizontal offset of a bullet from the center of the ship that fires it. */
constexpr float SHOT_OFFSET_X = 40.0F;

}  // namespace

void CombatPlugin::build(engine::PluginBuilder& builder) { builder.require<systems::PhysicsPlugin>(); }

void CombatPlugin::init(engine::Engine& engine) {
    _registry = &engine.registry();
    _bus = &engine.eventBus();
    _router = std::make_unique<collision::CollisionRouter>(*_bus);
    _inputToken =
        _bus->subscribe<ecs::net::InputApplied>([this](const ecs::net::InputApplied& event) { onInput(event); });
    _hitToken =
        _bus->subscribe<collision::ShotHitEvent>([this](const collision::ShotHitEvent& hit) { onShotHit(hit); });
    _contactToken = _bus->subscribe<collision::ShipContactEvent>(
        [this](const collision::ShipContactEvent& contact) { onShipContact(contact); });
    _powerUpToken = _bus->subscribe<collision::PowerUpTouchedEvent>(
        [this](const collision::PowerUpTouchedEvent& touch) { onPowerUpTouched(touch); });
    _levelToken = _bus->subscribe<LevelFinished>([this](const LevelFinished& finished) { onLevelFinished(finished); });
}

void CombatPlugin::shutdown(engine::Engine& /*engine*/) {
    if (_hitToken) {
        _bus->unsubscribe<collision::ShotHitEvent>(*_hitToken);
        _hitToken.reset();
    }
    if (_contactToken) {
        _bus->unsubscribe<collision::ShipContactEvent>(*_contactToken);
        _contactToken.reset();
    }
    if (_powerUpToken) {
        _bus->unsubscribe<collision::PowerUpTouchedEvent>(*_powerUpToken);
        _powerUpToken.reset();
    }
    if (_inputToken) {
        _bus->unsubscribe<ecs::net::InputApplied>(*_inputToken);
        _inputToken.reset();
    }
    if (_levelToken) {
        _bus->unsubscribe<LevelFinished>(*_levelToken);
        _levelToken.reset();
    }
    _router.reset();
}

void CombatPlugin::setPlayerCount(const std::size_t playerCount) noexcept {
    _playerCount = std::min(playerCount, _scores.size());
}

std::uint32_t CombatPlugin::score(const std::size_t playerIndex) const noexcept {
    return playerIndex < _scores.size() ? _scores.at(playerIndex) : 0;
}

std::vector<std::uint32_t> CombatPlugin::playerScores() const {
    const auto played = std::span(_scores).first(_playerCount);
    return {played.begin(), played.end()};
}

void CombatPlugin::onLevelFinished(const LevelFinished& finished) {
    std::vector<std::uint32_t> scores = playerScores();
    const std::uint32_t total = std::accumulate(scores.begin(), scores.end(), std::uint32_t{0});
    _bus->publish(protocol::LevelComplete{.levelIndex = finished.levelIndex, .score = total, .playerScores = scores});
    if (finished.lastLevel) {
        log::info(LOG_TAG, "game won, score ", total);
        _bus->publish(
            protocol::GameOver{.reason = protocol::GameOverReason::Victory, .score = total, .playerScores = scores});
    }
}

void CombatPlugin::onInput(const ecs::net::InputApplied& event) {
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
    _shotCooldowns.at(index) = _rapidFire.at(index) > 0.0F ? RAPID_FIRE_COOLDOWN : SHOT_COOLDOWN;
    const auto& transform = registry.getComponents<ecs::Transform>().get(event.entity);
    // Fired at the next fixed step: the inputs are applied while the replication plugin walks the input buffers.
    _pendingShots.push_back({.playerIndex = index, .x = transform.x, .y = transform.y});
}

void CombatPlugin::onShotHit(const collision::ShotHitEvent& hit) {
    ecs::Registry& registry = *_registry;
    if (!registry.isAlive(hit.shot)) {
        return;
    }
    if (!hit.byPlayer) {
        // An enemy shot stops on what it hits; a ship that just respawned lets it through.
        if ((hit.targetCategory & ::prefabs::collision::PLAYER) != 0) {
            if (!isVulnerable(hit.target)) {
                return;
            }
            log::info(LOG_TAG, "a ship was shot down");
            _bus->publish(ShipDestroyed{.ship = hit.target});
        }
        registry.killEntity(hit.shot);
        return;
    }
    // The contacts are published after the physics step: a shot touching two enemies in the same step only gets one.
    const std::optional<ecs::Entity> damaged =
        (hit.targetCategory & ::prefabs::collision::ENEMY) != 0 && registry.isAlive(hit.target) ? damagedBy(hit.target)
                                                                                                : std::nullopt;
    if (damaged.has_value() && takeHit(*damaged)) {
        if (registry.hasComponent<game::EntityNetState>(hit.shot)) {
            const std::uint8_t shooter = registry.getComponents<game::EntityNetState>().get(hit.shot).subtype;
            if (shooter < _scores.size()) {
                _scores.at(shooter) += POINTS_PER_KILL;
            }
        }
        // An enemy carrying loot leaves it where it died; spawned at the next step, outside of the contact callbacks.
        if (registry.hasComponent<::prefabs::DropsPowerUp>(*damaged) &&
            registry.hasComponent<ecs::Transform>(*damaged)) {
            const auto& at = registry.getComponents<ecs::Transform>().get(*damaged);
            _pendingDrops.push_back({.x = at.x, .y = at.y});
        }
        // Destroying a boss destroys its parts too: they are its children in the hierarchy.
        registry.killEntity(*damaged);
    }
    registry.killEntity(hit.shot);
}

void CombatPlugin::onShipContact(const collision::ShipContactEvent& contact) {
    if (!isVulnerable(contact.ship) || !_registry->isAlive(contact.other)) {
        return;
    }
    const bool terrain = (contact.otherCategory & ::prefabs::collision::OBSTACLE) != 0;
    log::info(LOG_TAG, terrain ? "a ship crashed into the terrain" : "a ship crashed into an enemy");
    _bus->publish(ShipDestroyed{.ship = contact.ship});
}

void CombatPlugin::onPowerUpTouched(const collision::PowerUpTouchedEvent& touch) {
    ecs::Registry& registry = *_registry;
    if (!registry.isAlive(touch.powerUp) || !registry.isAlive(touch.ship)) {
        return;
    }
    registry.killEntity(touch.powerUp);
    if (!registry.hasComponent<game::EntityNetState>(touch.ship)) {
        return;
    }
    const std::uint8_t index = registry.getComponents<game::EntityNetState>().get(touch.ship).subtype;
    if (index < _rapidFire.size()) {
        _rapidFire.at(index) = RAPID_FIRE_DURATION;
        log::info(LOG_TAG, "player ", static_cast<int>(index) + 1, " picked up a power-up: rapid fire");
    }
}

bool CombatPlugin::isVulnerable(const ecs::Entity ship) const {
    return _registry->isAlive(ship) && !_registry->hasComponent<Respawning>(ship);
}

std::optional<ecs::Entity> CombatPlugin::damagedBy(const ecs::Entity target) const {
    const ecs::Registry& registry = *_registry;
    if (registry.hasComponent<::prefabs::BossPart>(target)) {
        switch (registry.getComponents<::prefabs::BossPart>().get(target).role) {
            case ::prefabs::BossPartRole::WeakPoint:
                return hierarchy::parentOf(registry, target);
            case ::prefabs::BossPartRole::Turret:
                return target;
            case ::prefabs::BossPartRole::Armor:
                return std::nullopt;
        }
    }
    if (registry.hasComponent<::prefabs::BossCore>(target)) {
        const auto children = hierarchy::childrenOf(registry, target);
        const bool shielded = std::ranges::any_of(children, [&registry](const ecs::Entity child) {
            return registry.hasComponent<::prefabs::BossPart>(child) &&
                   registry.getComponents<::prefabs::BossPart>().get(child).role == ::prefabs::BossPartRole::WeakPoint;
        });
        if (shielded) {
            return std::nullopt;
        }
    }
    return target;
}

bool CombatPlugin::takeHit(const ecs::Entity target) {
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

void CombatPlugin::fixedUpdate(engine::Engine& /*engine*/, const ecs::time::Time& time) {
    for (float& cooldown : _shotCooldowns) {
        cooldown = std::max(cooldown - time.deltaTime, 0.0F);
    }
    for (float& rapidFire : _rapidFire) {
        rapidFire = std::max(rapidFire - time.deltaTime, 0.0F);
    }
    for (const PendingShot& shot : _pendingShots) {
        fire(shot);
    }
    _pendingShots.clear();
    for (const PendingDrop& drop : _pendingDrops) {
        const ecs::Entity powerUp = _registry->spawnEntity();
        ::prefabs::buildPowerUp(powerUp, *_registry, {{"x", drop.x}, {"y", drop.y}});
        _registry->addComponent(powerUp, game::EntityNetState{});
        ecs::net::replicate(*_registry, powerUp, game::KIND_POWER_UP);
    }
    _pendingDrops.clear();

    _scoreLogTimer -= time.deltaTime;
    if (_scoreLogTimer <= 0.0F) {
        _scoreLogTimer += SCORE_LOG_INTERVAL;
        logScores();
    }
}

void CombatPlugin::fire(const PendingShot& shot) {
    ecs::Registry& registry = *_registry;
    const ecs::Entity bullet = registry.spawnEntity();
    ::prefabs::buildPlayerBullet(bullet, registry, {{"x", shot.x + SHOT_OFFSET_X}, {"y", shot.y}});
    registry.addComponent(bullet, game::EntityNetState{.subtype = shot.playerIndex});
    ecs::net::replicate(registry, bullet, game::KIND_PLAYER_BULLET);
}

void CombatPlugin::logScores() const {
    std::ostringstream line;
    line << "scores:";
    for (std::size_t index = 0; index < _playerCount; ++index) {
        line << " P" << (index + 1) << '=' << _scores.at(index);
    }
    log::info(LOG_TAG, line.str());
}

}  // namespace rtype::server::gameplay
