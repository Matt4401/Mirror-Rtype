/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** Gameplay rule destroying player shots and the enemies they hit.
*/

#pragma once

#include <cstddef>

#include "collision/GameCollisionEvents.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"

namespace client::gameplay {

/**
 * @brief Applies `ShotHitEvent`: a player shot that touches an enemy destroys both, one that touches an obstacle is
 * destroyed alone.
 *
 * The events are published after the physics step, so the entities are killed right away. A shot touching two enemies
 * in the same step only destroys the first one: the physics system drops the contacts of an entity killed meanwhile.
 * The registry and the bus must outlive the rule.
 */
class ShotHitRule {
  public:
    /**
     * @brief Subscribes to the `ShotHitEvent` of a bus.
     * @param bus Bus on which `CollisionRouter` publishes.
     * @param registry Registry owning the shots and their targets.
     */
    ShotHitRule(ecs::EventBus& bus, ecs::Registry& registry);

    /** @brief Unsubscribes from the bus. */
    ~ShotHitRule();

    /** @brief Deleted copy constructor. */
    ShotHitRule(const ShotHitRule&) = delete;
    /** @brief Deleted copy assignment operator. */
    ShotHitRule& operator=(const ShotHitRule&) = delete;
    /** @brief Deleted move constructor. */
    ShotHitRule(ShotHitRule&&) = delete;
    /** @brief Deleted move assignment operator. */
    ShotHitRule& operator=(ShotHitRule&&) = delete;

    /** @brief Number of enemies destroyed since the rule was created. */
    [[nodiscard]] std::size_t destroyedEnemies() const noexcept { return _destroyedEnemies; }

  private:
    /** @brief Kills the shot, and the target when it is an enemy hit by a player shot. */
    void onShotHit(const rtype::server::collision::ShotHitEvent& hit);

    /** @brief Bus the rule is subscribed to. */
    ecs::EventBus* _bus;
    /** @brief Registry owning the entities. */
    ecs::Registry* _registry;
    /** @brief Subscription token of `onShotHit`. */
    ecs::EventBus::EventToken _token;
    /** @brief Enemies destroyed so far. */
    std::size_t _destroyedEnemies = 0;
};

}  // namespace client::gameplay
