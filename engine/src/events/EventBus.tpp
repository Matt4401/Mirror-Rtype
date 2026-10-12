#pragma once
#include "events/EventBus.hpp"

namespace ecs {

template <typename EventType>
EventBus::EventToken EventBus::subscribe(std::move_only_function<void(const EventType&)> callback) {
    const auto type = std::type_index(typeid(EventType));
    const EventToken token = _nextToken++;

    std::move_only_function<void(const void*)> erasedCallback = [callback =
                                                                     std::move(callback)](const void* event) mutable {
        callback(*static_cast<const EventType*>(event));
    };

    _listeners[type].push_back(Listener{.token = token, .callback = std::move(erasedCallback)});
    return token;
}

template <typename EventType>
void EventBus::unsubscribe(EventToken token) {
    const auto type = std::type_index(typeid(EventType));
    if (auto it = _listeners.find(type); it != _listeners.end()) {
        std::erase_if(it->second, [token](const Listener& listener) { return listener.token == token; });
    }
}

template <typename EventType>
void EventBus::publish(const EventType& event) {
    const auto type = std::type_index(typeid(EventType));
    if (auto it = _listeners.find(type); it != _listeners.end()) {
        for (auto& listener : it->second) {
            listener.callback(&event);
        }
    }
}

}  // namespace ecs
