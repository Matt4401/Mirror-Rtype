#pragma once
#include <memory>
#include <utility>
#include <vector>

#include "events/EventBus.hpp"

namespace ecs {

template <typename EventType>
EventBus::EventToken EventBus::subscribe(MoveOnlyFunction<void(const EventType&)> callback) {
    const auto type = std::type_index(typeid(EventType));
    const EventToken token = _nextToken++;

    MoveOnlyFunction<void(const void*)> erasedCallback = [callback = std::move(callback)](const void* event) mutable {
        callback(*static_cast<const EventType*>(event));
    };

    auto binding =
        std::make_shared<ListenerBinding>(ListenerBinding{.active = true, .callback = std::move(erasedCallback)});

    auto it = _listeners.find(type);
    if (it == _listeners.end() || !it->second) {
        auto newList = std::make_shared<std::vector<Listener>>();
        newList->push_back(Listener{.token = token, .binding = std::move(binding)});
        _listeners[type] = std::move(newList);
    } else {
        auto newList = std::make_shared<std::vector<Listener>>(*it->second);
        newList->push_back(Listener{.token = token, .binding = std::move(binding)});
        it->second = std::move(newList);
    }
    return token;
}

template <typename EventType>
void EventBus::unsubscribe(EventToken token) {
    const auto type = std::type_index(typeid(EventType));
    auto it = _listeners.find(type);
    if (it == _listeners.end() || !it->second) {
        return;
    }
    for (auto& listener : *it->second) {
        if (listener.token == token && listener.binding) {
            listener.binding->active = false;
        }
    }
    auto newList = std::make_shared<std::vector<Listener>>();
    newList->reserve(it->second->size());
    for (const auto& listener : *it->second) {
        if (listener.token != token && listener.binding && listener.binding->active) {
            newList->push_back(listener);
        }
    }
    it->second = std::move(newList);
}

template <typename EventType>
void EventBus::publish(const EventType& event) {
    const auto type = std::type_index(typeid(EventType));
    std::shared_ptr<std::vector<Listener>> list;
    if (auto it = _listeners.find(type); it != _listeners.end()) {
        list = it->second;
    }
    if (!list) {
        return;
    }
    for (const auto& listener : *list) {
        auto binding = listener.binding;
        if (binding && binding->active) {
            binding->callback(&event);
        }
    }
}

}  // namespace ecs
