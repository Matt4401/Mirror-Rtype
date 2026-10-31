/**
 * @file ForeignCall.tpp
 * @brief Template method implementations for ForeignCall argument and return marshaling.
 */

#pragma once
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

#include "scripting/ForeignCall.hpp"
#include "scripting/WrenVM.hpp"

namespace scripting {

inline WrenVM* ForeignCall::vm() const noexcept { return static_cast<WrenVM*>(wrenGetUserData(_vm)); }

template <typename T>
T* ForeignCall::userData() const noexcept {
    auto* wrapper = vm();
    if (wrapper == nullptr) {
        return nullptr;
    }
    return static_cast<T*>(wrapper->getUserContext());
}

template <typename T>
std::optional<T> ForeignCall::arg(int slot) const noexcept {
    using DecayedT = std::decay_t<T>;

    if (slot >= slotCount()) {
        abort("Slot index out of range");
        return std::nullopt;
    }

    if constexpr (std::is_same_v<DecayedT, bool>) {
        if (wrenGetSlotType(_vm, slot) != WREN_TYPE_BOOL) {
            abort("Expected argument to be a boolean");
            return std::nullopt;
        }
        return wrenGetSlotBool(_vm, slot);
    } else if constexpr (std::is_floating_point_v<DecayedT>) {
        if (wrenGetSlotType(_vm, slot) != WREN_TYPE_NUM) {
            abort("Expected argument to be a number");
            return std::nullopt;
        }
        double const val = wrenGetSlotDouble(_vm, slot);
        if (std::isnan(val) || std::isinf(val)) {
            abort("Number argument is NaN or infinite");
            return std::nullopt;
        }
        if (std::abs(val) > static_cast<double>(std::numeric_limits<DecayedT>::max())) {
            abort("Number argument out of floating-point bounds");
            return std::nullopt;
        }
        return static_cast<T>(val);
    } else if constexpr (std::is_integral_v<DecayedT>) {
        if (wrenGetSlotType(_vm, slot) != WREN_TYPE_NUM) {
            abort("Expected argument to be a number");
            return std::nullopt;
        }
        double const val = wrenGetSlotDouble(_vm, slot);
        if (std::isnan(val) || std::isinf(val)) {
            abort("Number argument is NaN or infinite");
            return std::nullopt;
        }
        if (std::trunc(val) != val) {
            abort("Expected argument to be an integer");
            return std::nullopt;
        }
        // The upper limit is exclusive: max() may round up to 2^digits once converted to double.
        if (val < static_cast<double>(std::numeric_limits<DecayedT>::lowest()) ||
            val >= std::ldexp(1.0, std::numeric_limits<DecayedT>::digits)) {
            abort("Number argument out of integer bounds");
            return std::nullopt;
        }
        return static_cast<T>(val);
    } else if constexpr (std::is_same_v<DecayedT, std::string_view>) {
        if (wrenGetSlotType(_vm, slot) != WREN_TYPE_STRING) {
            abort("Expected argument to be a string");
            return std::nullopt;
        }
        int len = 0;
        const char* str = wrenGetSlotBytes(_vm, slot, &len);
        return std::string_view(str, static_cast<std::size_t>(len));
    } else if constexpr (std::is_same_v<DecayedT, std::string>) {
        if (wrenGetSlotType(_vm, slot) != WREN_TYPE_STRING) {
            abort("Expected argument to be a string");
            return std::nullopt;
        }
        int len = 0;
        const char* str = wrenGetSlotBytes(_vm, slot, &len);
        return std::string(str, static_cast<std::size_t>(len));
    } else if constexpr (std::is_same_v<DecayedT, detail::UniqueWrenHandle>) {
        if (wrenGetSlotType(_vm, slot) == WREN_TYPE_NULL) {
            abort("Expected non-null handle argument");
            return std::nullopt;
        }
        return detail::UniqueWrenHandle(wrenGetSlotHandle(_vm, slot), detail::WrenHandleDeleter{.vm = _vm});
    } else if constexpr (std::is_same_v<DecayedT, WrenHandle*>) {
        return wrenGetSlotHandle(_vm, slot);
    } else {
        static_assert(detail::always_false_v<DecayedT>, "Unsupported type for ForeignCall::arg");
        return std::nullopt;
    }
}

template <typename T>
void ForeignCall::ret(const T& value) const noexcept {
    using DecayedT = std::decay_t<T>;

    if constexpr (std::is_same_v<DecayedT, std::nullptr_t>) {
        (void)value;
        wrenSetSlotNull(_vm, 0);
    } else if constexpr (std::is_same_v<DecayedT, bool>) {
        wrenSetSlotBool(_vm, 0, value);
    } else if constexpr (std::is_arithmetic_v<DecayedT>) {
        wrenSetSlotDouble(_vm, 0, static_cast<double>(value));
    } else if constexpr (std::is_convertible_v<DecayedT, std::string_view>) {
        std::string_view const sv = value;
        wrenSetSlotBytes(_vm, 0, sv.data(), sv.size());
    } else if constexpr (std::is_same_v<DecayedT, WrenHandle*>) {
        wrenSetSlotHandle(_vm, 0, value);
    } else if constexpr (std::is_same_v<DecayedT, detail::UniqueWrenHandle>) {
        wrenSetSlotHandle(_vm, 0, value.get());
    } else {
        static_assert(detail::always_false_v<DecayedT>, "Unsupported type for ForeignCall::ret");
    }
}

}  // namespace scripting
