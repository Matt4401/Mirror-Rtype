#pragma once
#include <cstdint>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <utility>
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
    EventToken subscribe(std::move_only_function<void(const EventType&)> callback) {
        const auto type = std::type_index(typeid(EventType));
        const EventToken token = _nextToken++;

        std::move_only_function<void(const void*)> erasedCallback =
            [callback = std::move(callback)](const void* event) mutable {
                callback(*static_cast<const EventType*>(event));
            };

        _listeners[type].push_back({token, std::move(erasedCallback)});
        return token;
    }

    /**
     * @brief Unsubscribes a listener identified by its token.
     *
     * @tparam EventType The event type to unsubscribe from.
     * @param token The token returned by `subscribe`.
     */
    template <typename EventType>
    void unsubscribe(EventToken token) {
        const auto type = std::type_index(typeid(EventType));
        if (auto it = _listeners.find(type); it != _listeners.end()) {
            std::erase_if(it->second, [token](const Listener& listener) { return listener.token == token; });
        }
    }

    /**
     * @brief Publishes an event, immediately invoking all registered listeners for its type.
     *
     * @tparam EventType The event type to dispatch.
     * @param event The event instance passed to each subscriber.
     */
    template <typename EventType>
    void publish(const EventType& event) {
        const auto type = std::type_index(typeid(EventType));
        if (auto it = _listeners.find(type); it != _listeners.end()) {
            for (auto& listener : it->second) {
                listener.callback(&event);
            }
        }
    }

  private:
    struct Listener {
        EventToken token;
        std::move_only_function<void(const void*)> callback;
    };

    std::unordered_map<std::type_index, std::vector<Listener>> _listeners;
    EventToken _nextToken = 0;
};

}  // namespace ecs
