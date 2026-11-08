/**
 * @file SceneContext.tpp
 * @brief Template implementations for SceneContext.
 */

#pragma once

#include <utility>

#include "scene/SceneContext.hpp"  // NOLINT(misc-header-include-cycle)

namespace scene {

template <typename EventType>
void SceneContext::subscribe(ecs::MoveOnlyFunction<void(const EventType&)> callback) {
    ecs::EventBus& eventBus = bus();
    const ecs::EventBus::EventToken token = eventBus.subscribe<EventType>(std::move(callback));
    _subscriptions.push_back(Subscription{.token = token, .unsubscribe = &unsubscribeFrom<EventType>});
}

template <typename EventType>
void SceneContext::unsubscribeFrom(ecs::EventBus& bus, ecs::EventBus::EventToken token) {
    bus.unsubscribe<EventType>(token);
}

}  // namespace scene
