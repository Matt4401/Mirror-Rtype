/**
 * @file PropertyAccessor.hpp
 * @brief Compile-time NTTP property accessor and Wren FFI marshaler for ECS components.
 */

#pragma once
#include <cstddef>
#include <exception>
#include <string>
#include <string_view>
#include <type_traits>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "scripting/ForeignCall.hpp"
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
    static void getter(::WrenVM* rawVm) noexcept {
        try {
            ForeignCall call(rawVm);
            auto* vmWrapper = call.vm();
            if (vmWrapper == nullptr || vmWrapper->getRegistry() == nullptr) {
                call.retNull();
                return;
            }
            auto& registry = *static_cast<ecs::Registry*>(vmWrapper->getRegistry());

            auto* entityPtr = call.argForeign<ecs::Entity>(0);
            if (entityPtr == nullptr) {
                call.retNull();
                return;
            }
            ecs::Entity const e = *entityPtr;
            if (!registry.template getComponents<ComponentType>().contains(e)) {
                call.retNull();
                return;
            }
            auto const& comp = registry.template getComponents<ComponentType>().get(e);
            call.ret(comp.*MemberPtr);
        } catch (const std::exception& e) {
            ForeignCall call(rawVm);
            call.abort(std::string("Exception in getter: ") + e.what());
        } catch (...) {
            ForeignCall call(rawVm);
            call.abort("Unknown exception in getter");
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
    static void setter(::WrenVM* rawVm) noexcept {
        try {
            ForeignCall call(rawVm);
            auto* vmWrapper = call.vm();
            if (vmWrapper == nullptr || vmWrapper->getRegistry() == nullptr) {
                return;
            }
            auto& registry = *static_cast<ecs::Registry*>(vmWrapper->getRegistry());

            auto* entityPtr = call.argForeign<ecs::Entity>(0);
            if (entityPtr == nullptr) {
                return;
            }
            ecs::Entity const e = *entityPtr;
            if (!registry.template getComponents<ComponentType>().contains(e)) {
                return;
            }
            auto& comp = registry.template getComponents<ComponentType>().get(e);

            if constexpr (std::is_same_v<FieldType, std::string>) {
                auto val = call.arg<std::string_view>(1);
                if (!val.has_value()) {
                    return;
                }
                comp.*MemberPtr = std::string(*val);
            } else {
                auto val = call.arg<FieldType>(1);
                if (!val.has_value()) {
                    return;
                }
                comp.*MemberPtr = *val;
            }
        } catch (const std::exception& e) {
            ForeignCall call(rawVm);
            call.abort(std::string("Exception in setter: ") + e.what());
        } catch (...) {
            ForeignCall call(rawVm);
            call.abort("Unknown exception in setter");
        }
    }
};

}  // namespace scripting::detail
