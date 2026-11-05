/**
 * @file ScriptSystem.tpp
 * @brief Template implementation for ScriptSystem event forwarding subscriptions.
 */

#pragma once
#include <string>
#include <utility>

#include "ecs/Entity.hpp"
#include "systems/script/ScriptSystem.hpp"

namespace systems {

template <typename EventType, typename ArgExtractor>
void ScriptSystem::registerEventForwarding(ecs::EventBus& bus, std::string const& methodName,
                                           ArgExtractor&& extractor) {
    std::string const signature = methodName + "(_)";
    if (!_cachedMethodHandles.contains(signature)) {
        _cachedMethodHandles[signature] = _vm.makeCallHandle(signature);
    }

    bus.subscribe<EventType>(
        [this, signature, extractor = std::forward<ArgExtractor>(extractor)](const EventType& event) {
            auto [targetEntity, argEntity] = extractor(event);
            invokeEntityMethod(targetEntity, signature, static_cast<ecs::Entity>(argEntity));
        });
}

}  // namespace systems
