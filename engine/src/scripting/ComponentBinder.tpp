#pragma once
#include "scripting/ComponentBinder.hpp"
#include "scripting/detail/PropertyAccessor.hpp"

namespace scripting {

template <typename ComponentType>
ComponentBinder<ComponentType>::ComponentBinder(WrenVM& vm, std::string moduleName, std::string className)
    : ComponentBinderBase(vm, std::move(moduleName), std::move(className)) {}

template <typename ComponentType>
template <auto MemberPtr>
ComponentBinder<ComponentType>& ComponentBinder<ComponentType>::property(const std::string& propName) {
    register_property_methods(propName, &detail::PropertyAccessor<ComponentType, MemberPtr>::getter,
                              &detail::PropertyAccessor<ComponentType, MemberPtr>::setter);
    return *this;
}

}  // namespace scripting
