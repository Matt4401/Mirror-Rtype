#pragma once
#include <cstdint>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace ecs {

/**
 * @brief Decoupled publish-subscribe event bus connecting engine systems and scripts.
 *
 * Provides a type-safe pub-sub messaging architecture:
 * - Systems or scripts subscribe to specific event types (`CollisionEvent`, etc.).
 * - Type erasure (`std::move_only_function<void(const void*)>`) allows storing callbacks generically.
 * - Supports listener removal via subscription tokens (`EventToken`).
 */
class EventBus {
  public:
    /**
     * @brief Unique identifier returned when subscribing to an event, used for unsubscription.
     */
    using EventToken = uint32_t;

    /** @brief Default constructor initializing an empty event bus. */
    EventBus() = default;
    /** @brief Default destructor. */
    ~EventBus() = default;

    /** @brief Deleted copy constructor. */
    EventBus(const EventBus&) = delete;
    /** @brief Deleted copy assignment operator. */
    EventBus& operator=(const EventBus&) = delete;
    /** @brief Deleted move constructor. */
    EventBus(EventBus&&) = delete;
    /** @brief Deleted move assignment operator. */
    EventBus& operator=(EventBus&&) = delete;

    /**
     * @brief Subscribes a callback to an event type.
     *
     * @tparam EventType The event struct/class to listen for.
     * @param callback Move-only callable receiving a const reference to the event.
     * @return Unique `EventToken` for unsubscribing later.
     */
    template <typename EventType>
    EventToken subscribe(std::move_only_function<void(const EventType&)> callback);

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
    /**
     * @brief Internal tracking structure pairing a subscription token with a type-erased callback.
     */
    struct Listener {
        /** @brief Unique cancellation token identifying this listener. */
        EventToken token;
        /** @brief Type-erased invocation callback receiving an opaque event pointer. */
        std::move_only_function<void(const void*)> callback;
    };

    /** @brief Map of event types to their active subscriber callback lists. */
    std::unordered_map<std::type_index, std::vector<Listener>> _listeners;
    /** @brief Monotonically increasing counter issuing unique event tokens. */
    EventToken _nextToken = 0;
};

}  // namespace ecs

#include "EventBus.tpp"  // IWYU pragma: export
