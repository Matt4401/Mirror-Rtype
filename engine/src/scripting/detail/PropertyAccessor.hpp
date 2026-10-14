#pragma once
#include <string>
#include <type_traits>

extern "C" {
#include <wren.h>
}

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "scripting/WrenVM.hpp"

namespace scripting::detail {

/**
 * @brief Primary template declaration for the ECS component property accessor.
 *
 * Specializations of this struct extract member pointer metadata at compile time
 * and provide static FFI callbacks compatible with Wren's Foreign Method interface.
 *
 * @tparam ComponentType The C++ ECS component aggregate/struct.
 * @tparam MemberPtr Non-Type Template Parameter (NTTP) pointing to the struct member.
 */
template <typename ComponentType, auto MemberPtr>
struct PropertyAccessor;

/**
 * @brief Specialization for pointer-to-member fields (`FieldType ComponentType::*`).
 *
 * Provides static `getter` and `setter` functions matching `WrenForeignMethodFn`.
 * Automatically deduces field types (arithmetic, boolean, string) and converts
 * them to/from Wren VM slots using C++23 `if constexpr`.
 *
 * @tparam ComponentType The ECS component struct type.
 * @tparam FieldType The primitive or aggregate field type of the member.
 * @tparam MemberPtr The compile-time pointer-to-member.
 */
template <typename ComponentType, typename FieldType, FieldType ComponentType::* MemberPtr>
struct PropertyAccessor<ComponentType, MemberPtr> {
    /**
     * @brief Wren foreign method getter.
     *
     * Reads the component instance from the ECS Registry associated with the entity
     * stored in foreign slot 0, and writes the field value to Wren slot 0.
     *
     * @param rawVm Raw pointer to the underlying Wren VM instance.
     */
    static void getter(::WrenVM* rawVm) {
        auto* vmWrapper = static_cast<WrenVM*>(wrenGetUserData(rawVm));
        if (vmWrapper == nullptr || vmWrapper->getRegistry() == nullptr) {
            wrenSetSlotNull(rawVm, 0);
            return;
        }
        auto& registry = *static_cast<ecs::Registry*>(vmWrapper->getRegistry());

        auto* entityPtr = static_cast<ecs::Entity*>(wrenGetSlotForeign(rawVm, 0));
        if (entityPtr == nullptr) {
            wrenSetSlotNull(rawVm, 0);
            return;
        }
        ecs::Entity const e = *entityPtr;
        if (!registry.template getComponents<ComponentType>().contains(e)) {
            wrenSetSlotNull(rawVm, 0);
            return;
        }
        auto const& comp = registry.template getComponents<ComponentType>().get(e);

        if constexpr (std::is_arithmetic_v<FieldType> && !std::is_same_v<FieldType, bool>) {
            wrenSetSlotDouble(rawVm, 0, static_cast<double>(comp.*MemberPtr));
        } else if constexpr (std::is_same_v<FieldType, bool>) {
            wrenSetSlotBool(rawVm, 0, comp.*MemberPtr);
        } else if constexpr (std::is_same_v<FieldType, std::string>) {
            wrenSetSlotBytes(rawVm, 0, (comp.*MemberPtr).data(), (comp.*MemberPtr).size());
        }
    }

    /**
     * @brief Wren foreign method setter.
     *
     * Reads the new value from Wren slot 1 and mutates the component instance
     * belonging to the entity stored in foreign slot 0 within the ECS Registry.
     *
     * @param rawVm Raw pointer to the underlying Wren VM instance.
     */
    static void setter(::WrenVM* rawVm) {
        auto* vmWrapper = static_cast<WrenVM*>(wrenGetUserData(rawVm));
        if (vmWrapper == nullptr || vmWrapper->getRegistry() == nullptr) {
            return;
        }
        auto& registry = *static_cast<ecs::Registry*>(vmWrapper->getRegistry());

        auto* entityPtr = static_cast<ecs::Entity*>(wrenGetSlotForeign(rawVm, 0));
        if (entityPtr == nullptr) {
            return;
        }
        ecs::Entity const e = *entityPtr;
        if (!registry.template getComponents<ComponentType>().contains(e)) {
            return;
        }
        auto& comp = registry.template getComponents<ComponentType>().get(e);

        if constexpr (std::is_arithmetic_v<FieldType> && !std::is_same_v<FieldType, bool>) {
            comp.*MemberPtr = static_cast<FieldType>(wrenGetSlotDouble(rawVm, 1));
        } else if constexpr (std::is_same_v<FieldType, bool>) {
            comp.*MemberPtr = wrenGetSlotBool(rawVm, 1);
        } else if constexpr (std::is_same_v<FieldType, std::string>) {
            int len = 0;
            const char* s = wrenGetSlotBytes(rawVm, 1, &len);
            comp.*MemberPtr = std::string(s, static_cast<std::size_t>(len));
        }
    }
};

}  // namespace scripting::detail
