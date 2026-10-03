#pragma once
#include <cstdint>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace ecs {
    class EventBus {
    public:
        using EventToken = uint32_t;

        EventBus() = default;
        ~EventBus() = default;

        EventBus(const EventBus&) = delete;
        EventBus& operator=(const EventBus&) = delete;
        EventBus(EventBus&&) = delete;
        EventBus& operator=(EventBus&&) = delete;

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

        template <typename EventType>
        void unsubscribe(EventToken token) {
            const auto type = std::type_index(typeid(EventType));
            if (auto it = _listeners.find(type); it != _listeners.end()) {
                std::erase_if(it->second, [token](const Listener& listener) {
                    return listener.token == token;
                });
            }
        }

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
        EventToken _nextToken = 1;
    };
}
