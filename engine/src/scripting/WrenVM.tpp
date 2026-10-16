/**
 * @file WrenVM.tpp
 * @brief Template method implementations for WrenVM slot marshaling.
 */

#pragma once
#include <type_traits>

#include "scripting/WrenVM.hpp"

namespace scripting {

template <typename T>
void WrenVM::setSlot(int slot, const T& value) {
    using DecayedT = std::decay_t<T>;
    if constexpr (std::is_same_v<DecayedT, std::nullptr_t>) {
        (void)value;
        wrenSetSlotNull(raw(), slot);
    } else if constexpr (std::is_same_v<DecayedT, bool>) {
        wrenSetSlotBool(raw(), slot, value);
    } else if constexpr (std::is_arithmetic_v<DecayedT>) {
        wrenSetSlotDouble(raw(), slot, static_cast<double>(value));
    } else if constexpr (std::is_convertible_v<DecayedT, std::string_view>) {
        std::string_view const sv = value;
        wrenSetSlotBytes(raw(), slot, sv.data(), sv.size());
    } else if constexpr (std::is_same_v<DecayedT, WrenHandle*>) {
        wrenSetSlotHandle(raw(), slot, value);
    } else if constexpr (std::is_same_v<DecayedT, detail::UniqueWrenHandle>) {
        wrenSetSlotHandle(raw(), slot, value.get());
    }
}

template <typename T>
T WrenVM::getSlot(int slot) {
    using DecayedT = std::decay_t<T>;
    if constexpr (std::is_same_v<DecayedT, bool>) {
        return wrenGetSlotBool(raw(), slot);
    } else if constexpr (std::is_arithmetic_v<DecayedT>) {
        return static_cast<T>(wrenGetSlotDouble(raw(), slot));
    } else if constexpr (std::is_same_v<DecayedT, std::string_view>) {
        int length = 0;
        const char* str = wrenGetSlotBytes(raw(), slot, &length);
        return std::string_view(str, static_cast<std::size_t>(length));
    } else if constexpr (std::is_same_v<DecayedT, std::string>) {
        int length = 0;
        const char* str = wrenGetSlotBytes(raw(), slot, &length);
        return std::string(str, static_cast<std::size_t>(length));
    } else if constexpr (std::is_same_v<DecayedT, WrenHandle*>) {
        return wrenGetSlotHandle(raw(), slot);
    } else if constexpr (std::is_same_v<DecayedT, detail::UniqueWrenHandle>) {
        return detail::UniqueWrenHandle(wrenGetSlotHandle(raw(), slot), detail::WrenHandleDeleter{.vm = raw()});
    }
}

}  // namespace scripting
