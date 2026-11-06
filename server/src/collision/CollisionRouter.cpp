#include "collision/CollisionRouter.hpp"

#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

#include "Logger.hpp"
#include "collision/GameCollisionEvents.hpp"
#include "ecs/Entity.hpp"
#include "events/EventBus.hpp"
#include "events/PhysicsEvents.hpp"
#include "prefabs/CollisionCategories.hpp"

namespace rtype::server::collision {

namespace {

namespace category = prefabs::collision;

/** @brief Tag of the collision log messages: `[collision] ...`. */
constexpr std::string_view LOG_TAG = "collision";

/** @brief One entity of a contact with its collision category. */
struct Side {
    ecs::Entity entity;
    std::uint16_t category;
};

/** @brief Whether a side belongs to one of the categories of `mask`. */
constexpr bool is(const Side& side, std::uint16_t mask) noexcept { return (side.category & mask) != 0; }

/** @brief Orders a contact as (`self`, `other`), where `self` is in category `selfMask` and `other` in `otherMask`. */
std::optional<std::pair<Side, Side>> match(const Side& a, const Side& b, std::uint16_t selfMask,
                                           std::uint16_t otherMask) noexcept {
    if (is(a, selfMask) && is(b, otherMask)) {
        return std::pair{a, b};
    }
    if (is(b, selfMask) && is(a, otherMask)) {
        return std::pair{b, a};
    }
    return std::nullopt;
}

}  // namespace

std::optional<GameCollision> classify(const events::CollisionBeginEvent& contact) noexcept {
    const Side a{.entity = contact.entityA, .category = contact.categoryA};
    const Side b{.entity = contact.entityB, .category = contact.categoryB};

    if (const auto pair = match(a, b, category::PLAYER_SHOT, category::ENEMY | category::OBSTACLE)) {
        return ShotHitEvent{.shot = pair->first.entity,
                            .target = pair->second.entity,
                            .targetCategory = pair->second.category,
                            .byPlayer = true};
    }
    if (const auto pair = match(a, b, category::ENEMY_SHOT, category::PLAYER | category::OBSTACLE)) {
        return ShotHitEvent{.shot = pair->first.entity,
                            .target = pair->second.entity,
                            .targetCategory = pair->second.category,
                            .byPlayer = false};
    }
    if (const auto pair = match(a, b, category::PLAYER, category::POWER_UP)) {
        return PowerUpTouchedEvent{.ship = pair->first.entity, .powerUp = pair->second.entity};
    }
    if (const auto pair = match(a, b, category::PLAYER, category::ENEMY | category::OBSTACLE)) {
        return ShipContactEvent{
            .ship = pair->first.entity, .other = pair->second.entity, .otherCategory = pair->second.category};
    }
    return std::nullopt;
}

CollisionRouter::CollisionRouter(ecs::EventBus& bus)
    : _bus(&bus), _token(bus.subscribe<events::CollisionBeginEvent>([this](const events::CollisionBeginEvent& contact) {
          onContact(contact);
      })) {}

CollisionRouter::~CollisionRouter() { _bus->unsubscribe<events::CollisionBeginEvent>(_token); }

void CollisionRouter::onContact(const events::CollisionBeginEvent& contact) {
    const std::optional<GameCollision> collision = classify(contact);
    if (!collision) {
        return;
    }
    std::visit(
        [this](const auto& event) {
            using Event = std::decay_t<decltype(event)>;
            if constexpr (std::is_same_v<Event, ShotHitEvent>) {
                log::debug(LOG_TAG, event.byPlayer ? "player" : "enemy", " shot ", event.shot, " hit ", event.target);
            } else if constexpr (std::is_same_v<Event, ShipContactEvent>) {
                log::debug(LOG_TAG, "ship ", event.ship, " touched ", event.other);
            } else {
                log::debug(LOG_TAG, "ship ", event.ship, " touched power-up ", event.powerUp);
            }
            _bus->publish(event);
        },
        *collision);
}

}  // namespace rtype::server::collision
