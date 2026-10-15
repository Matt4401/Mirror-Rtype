#pragma once
#include <cstdint>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "utils/MoveOnlyFunction.hpp"

namespace ecs {

/**
 * @brief Decoupled publish-subscribe event bus connecting engine systems and scripts.
 *
 * Provides a type-safe pub-sub messaging architecture:
 * - Systems or scripts subscribe to specific event types (`CollisionEvent`, etc.).
 * - Type erasure (`MoveOnlyFunction<void(const void*)>`) allows storing callbacks generically.
 * - Supports listener removal via subscription tokens (`EventToken`).
 */
class EventBus {
  public:
    /**
     * @brief Unique identifier returned when subscribing to an event, used for unsubscription.
     */
    using EventToken = uint32_t;

    EventBus() = default;
    ~EventBus() = default;

    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;
    EventBus(EventBus&&) = delete;
    EventBus& operator=(EventBus&&) = delete;

    /**
     * @brief Subscribes a callback to an event type.
     *
     * @tparam EventType The event struct/class to listen for.
     * @param callback Move-only callable receiving a const reference to the event.
     * @return Unique `EventToken` for unsubscribing later.
     */
    template <typename EventType>
    EventToken subscribe(MoveOnlyFunction<void(const EventType&)> callback);

    /**
     * @brief Unsubscribes a listener identified by its token.
     *
     * @tparam EventType The event type to unsubscribe from.
     * @param token The token returned by `subscribe`.
     */
    template <typename EventType>
    void unsubscribe(EventToken token);

    /**
     * @brief Publishes an event, immediately invoking all registered listeners for its type.
     *
     * @tparam EventType The event type to dispatch.
     * @param event The event instance passed to each subscriber.
     */
    template <typename EventType>
    void publish(const EventType& event);

  private:
    struct Listener {
        EventToken token;
        MoveOnlyFunction<void(const void*)> callback;
    };

    std::unordered_map<std::type_index, std::vector<Listener>> _listeners;
    EventToken _nextToken = 0;
};

}  // namespace ecs

#include "EventBus.tpp"  // IWYU pragma: export
