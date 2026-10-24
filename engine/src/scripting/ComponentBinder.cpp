/**
 * @file ComponentBinder.cpp
 * @brief Implementation of ComponentBinderBase foreign class registration and script generation.
 */

#include "scripting/ComponentBinder.hpp"

#include <exception>
#include <sstream>
#include <string>
#include <utility>

#include "ecs/Entity.hpp"
#include "scripting/ForeignCall.hpp"
#include "scripting/WrenVM.hpp"

namespace scripting {

ComponentBinderBase::ComponentBinderBase(WrenVM& vm, std::string moduleName, std::string className)
    : _vm(&vm), _module(std::move(moduleName)), _className(std::move(className)) {
    _vm->registerForeignClass(
        _module, _className,
        WrenForeignClassMethods{.allocate =
                                    [](::WrenVM* rawVm) noexcept {
                                        try {
                                            ForeignCall const call(rawVm);
                                            auto entityId = call.arg<ecs::Entity>(1);
                                            if (!entityId.has_value()) {
                                                return;
                                            }
                                            auto* ptr = call.retNewForeign<ecs::Entity>(0);
                                            if (ptr != nullptr) {
                                                *ptr = *entityId;
                                            }
                                        } catch (const std::exception& e) {
                                            ForeignCall const call(rawVm);
                                            call.abort(std::string("Exception in foreign class allocation: ") +
                                                       e.what());
                                        } catch (...) {
                                            ForeignCall const call(rawVm);
                                            call.abort("Unknown exception in foreign class allocation");
                                        }
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
