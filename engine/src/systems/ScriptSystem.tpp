#pragma once
#include "systems/ScriptSystem.hpp"

namespace systems {

template <typename EventType, typename ArgExtractor>
void ScriptSystem::register_event_forwarding(ecs::EventBus& bus, std::string const& methodName,
                                             ArgExtractor&& extractor) {
    std::string const signature = methodName + "(_)";
    auto callHandle = _vm.make_call_handle(signature);
    WrenHandle* rawHandle = callHandle.get();
    _cachedMethodHandles[signature] = std::move(callHandle);

    bus.subscribe<EventType>(
        [this, rawHandle, extractor = std::forward<ArgExtractor>(extractor)](const EventType& event) {
            auto [targetEntity, argEntity] = extractor(event);
            if (!_registry->template get_components<ecs::ScriptComponent>().contains(targetEntity)) {
                return;
            }
            auto& script = _registry->template get_components<ecs::ScriptComponent>().get(targetEntity);
            if (script.instanceHandle == nullptr) {
                return;
            }

            ::WrenVM* raw = _vm.raw();
            wrenEnsureSlots(raw, 2);
            wrenSetSlotHandle(raw, 0, static_cast<WrenHandle*>(script.instanceHandle));
            wrenSetSlotDouble(raw, 1, static_cast<double>(argEntity));
            wrenCall(raw, rawHandle);
        });
}

}  // namespace systems
