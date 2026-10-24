/**
 * @file ScriptSystem.tpp
 * @brief Template implementation for ScriptSystem event forwarding subscriptions.
 */

#pragma once
#include <utility>

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
            auto itInst = _instanceHandles.find(targetEntity);
            if (itInst == _instanceHandles.end()) {
                return;
            }

            auto itMethod = _cachedMethodHandles.find(signature);
            if (itMethod == _cachedMethodHandles.end() || itMethod->second == nullptr) {
                return;
            }

            scripting::detail::UniqueWrenHandle argObj;
            if (_vm.hasVariable("core/Entity", "Entity")) {
                argObj = _vm.newInstanceWithHandle(_newHandle, "core/Entity", "Entity", static_cast<double>(argEntity));
            }

            if (argObj) {
                _vm.call(itMethod->second, itInst->second.get(), argObj.get());
            } else {
                _vm.call(itMethod->second, itInst->second.get(), static_cast<double>(argEntity));
            }
        });
}

}  // namespace systems
