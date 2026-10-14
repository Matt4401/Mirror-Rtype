#pragma once

namespace systems {

template <typename EventType, typename ArgExtractor>
void ScriptSystem::registerEventForwarding(ecs::EventBus& bus, std::string const& methodName,
                                           ArgExtractor&& extractor) {
    std::string const signature = methodName + "(_)";
    auto callHandle = _vm.makeCallHandle(signature);
    WrenHandle* rawHandle = callHandle.get();
    _cachedMethodHandles[signature] = std::move(callHandle);

    bus.subscribe<EventType>(
        [this, rawHandle, extractor = std::forward<ArgExtractor>(extractor)](const EventType& event) {
            auto [targetEntity, argEntity] = extractor(event);
            if (!_registry->template getComponents<ecs::ScriptComponent>().contains(targetEntity)) {
                return;
            }
            auto& script = _registry->template getComponents<ecs::ScriptComponent>().get(targetEntity);
            if (script.instanceHandle == nullptr) {
                return;
            }

            ::WrenVM* raw = _vm.raw();
            WrenHandle* argHandle = nullptr;
            if (_vm.hasVariable("core/Entity", "Entity")) {
                wrenEnsureSlots(raw, 2);
                _vm.getVariable("core/Entity", "Entity", 0);
                wrenSetSlotDouble(raw, 1, static_cast<double>(argEntity));
                if (wrenCall(raw, _newHandle.get()) == WREN_RESULT_SUCCESS) {
                    argHandle = wrenGetSlotHandle(raw, 0);
                }
            }

            wrenEnsureSlots(raw, 2);
            wrenSetSlotHandle(raw, 0, static_cast<WrenHandle*>(script.instanceHandle));
            if (argHandle != nullptr) {
                wrenSetSlotHandle(raw, 1, argHandle);
                wrenReleaseHandle(raw, argHandle);
            } else {
                wrenSetSlotDouble(raw, 1, static_cast<double>(argEntity));
            }
            wrenCall(raw, rawHandle);
        });
}

}  // namespace systems
