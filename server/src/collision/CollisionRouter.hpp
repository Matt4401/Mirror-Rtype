/**
 * @file CollisionRouter.hpp
 * @brief Turns the generic engine contact events into R-Type collision events.
 */

#pragma once

#include <optional>
#include <variant>

#include "collision/GameCollisionEvents.hpp"
#include "events/EventBus.hpp"
#include "events/PhysicsEvents.hpp"

namespace rtype::server::collision {

/** @brief Any R-Type collision event a contact can produce. */
using GameCollision = std::variant<ShotHitEvent, ShipContactEvent, PowerUpTouchedEvent>;

/**
 * @brief Classifies a contact by the collision categories of its two entities.
 *
 * The order of the entities in the contact does not matter. A pair with no R-Type meaning gives `std::nullopt`.
 *
 * @param contact Contact published by the physics system.
 * @return The matching R-Type event, if any.
 */
[[nodiscard]] std::optional<GameCollision> classify(const events::CollisionBeginEvent& contact) noexcept;

/**
 * @brief Subscribes to `events::CollisionBeginEvent` and republishes each contact as an R-Type event.
 *
 * It does not change any state: rules (damage, kills, pickups) subscribe to the events it publishes.
 * The event bus must outlive the router.
 */
class CollisionRouter {
  public:
    /**
     * @brief Subscribes to the contact events of a bus.
     * @param bus Bus receiving both the contact events and the R-Type events.
     */
    explicit CollisionRouter(ecs::EventBus& bus);

    /** @brief Unsubscribes from the bus. */
    ~CollisionRouter();

    /** @brief Deleted copy constructor. */
    CollisionRouter(const CollisionRouter&) = delete;
    /** @brief Deleted copy assignment operator. */
    CollisionRouter& operator=(const CollisionRouter&) = delete;
    /** @brief Deleted move constructor. */
    CollisionRouter(CollisionRouter&&) = delete;
    /** @brief Deleted move assignment operator. */
    CollisionRouter& operator=(CollisionRouter&&) = delete;

  private:
    /** @brief Classifies a contact, logs it and publishes the matching R-Type event. */
    void onContact(const events::CollisionBeginEvent& contact);

    /** @brief Bus the router is subscribed to. */
    ecs::EventBus* _bus;
    /** @brief Subscription token of `onContact`. */
    ecs::EventBus::EventToken _token;
};

}  // namespace rtype::server::collision
