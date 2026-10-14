#include "scripting/ComponentBinder.hpp"

#include <sstream>
#include <string>
#include <utility>

extern "C" {
#include <wren.h>
}

#include "ecs/Entity.hpp"
#include "scripting/WrenVM.hpp"

namespace scripting {

ComponentBinderBase::ComponentBinderBase(WrenVM& vm, std::string moduleName, std::string className)
    : _vm(&vm), _module(std::move(moduleName)), _className(std::move(className)) {
    _vm->registerForeignClass(
        _module, _className,
        WrenForeignClassMethods{.allocate =
                                    [](::WrenVM* rawVm) {
                                        auto* ptr = static_cast<ecs::Entity*>(
                                            wrenSetSlotNewForeign(rawVm, 0, 0, sizeof(ecs::Entity)));
                                        *ptr = static_cast<ecs::Entity>(wrenGetSlotDouble(rawVm, 1));
                                    },
                                .finalize = nullptr});
}

void ComponentBinderBase::registerPropertyMethods(const std::string& propName, WrenForeignMethodFn getter,
                                                  WrenForeignMethodFn setter) {
    _vm->registerForeignMethod(_module, _className, false, propName, getter);
    _vm->registerForeignMethod(_module, _className, false, propName + "=(_)", setter);
    _properties.push_back(propName);
}

std::string ComponentBinderBase::generateWrenClass() const {
    std::ostringstream ss;
    ss << "foreign class " << _className << " {\n";
    ss << "    construct new(entityId) {}\n";
    for (const auto& prop : _properties) {
        ss << "    foreign " << prop << "\n";
        ss << "    foreign " << prop << "=(val)\n";
    }
    ss << "}\n";
    return ss.str();
}

const std::string& ComponentBinderBase::getModule() const noexcept { return _module; }

const std::string& ComponentBinderBase::getClassName() const noexcept { return _className; }

}  // namespace scripting
