/**
 * @file WrenVM.tpp
 * @brief Template method implementations for WrenVM slot marshaling and foreign trampolines.
 */

#pragma once
#include <cmath>
#include <cstdint>
#include <exception>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

#include "scripting/ForeignCall.hpp"
#include "scripting/WrenVM.hpp"

namespace scripting {

template <auto Fn>
void WrenVM::foreignMethodTrampoline(::WrenVM* rawVm) noexcept {
    try {
        ForeignCall call(rawVm);
        if constexpr (std::is_invocable_v<decltype(Fn), ForeignCall&>) {
            Fn(call);
        } else if constexpr (std::is_invocable_v<decltype(Fn), ::WrenVM*>) {
            Fn(rawVm);
        } else {
            static_assert(detail::always_false_v<decltype(Fn)>, "Foreign method must accept ForeignCall& or ::WrenVM*");
        }
    } catch (const std::exception& e) {
        ForeignCall call(rawVm);
        call.abort(std::string("C++ exception: ") + e.what());
    } catch (...) {
        ForeignCall call(rawVm);
        call.abort("Unknown C++ exception");
    }
}

template <auto Fn>
void WrenVM::registerForeignMethod(std::string_view module, std::string_view className, bool isStatic,
                                   std::string_view signature) {
    registerForeignMethod(module, className, isStatic, signature, &foreignMethodTrampoline<Fn>);
}

template <typename Receiver, typename... Args>
WrenInterpretResult WrenVM::call(WrenHandle* method, const Receiver& receiver, const Args&... args) {
    constexpr int totalSlots = 1 + sizeof...(Args);
    ensureSlots(totalSlots);
    setSlot(0, receiver);
    int slot = 1;
    ([&]() { setSlot(slot++, args); }(), ...);
    return call(method);
}

template <typename Receiver, typename... Args>
WrenInterpretResult WrenVM::call(const detail::UniqueWrenHandle& method, const Receiver& receiver,
                                 const Args&... args) {
    return call(method.get(), receiver, args...);
}

template <typename... Args>
detail::UniqueWrenHandle WrenVM::newInstance(std::string_view module, std::string_view className, const Args&... args) {
    if (!hasVariable(module, className)) {
        return detail::UniqueWrenHandle(nullptr, detail::WrenHandleDeleter{.vm = raw()});
    }
    constexpr int totalSlots = 1 + sizeof...(Args);
    ensureSlots(totalSlots);
    getVariable(module, className, 0);
    int slot = 1;
    ([&]() { setSlot(slot++, args); }(), ...);

    auto newHandle = makeCallHandle(detail::makeMethodSignature("new", sizeof...(Args)));
    if (call(newHandle.get()) != WREN_RESULT_SUCCESS) {
        return detail::UniqueWrenHandle(nullptr, detail::WrenHandleDeleter{.vm = raw()});
    }
    return getSlot<detail::UniqueWrenHandle>(0);
}

template <typename... Args>
detail::UniqueWrenHandle WrenVM::newInstanceWithHandle(const detail::UniqueWrenHandle& newHandle,
                                                       std::string_view module, std::string_view className,
                                                       const Args&... args) {
    if (!hasVariable(module, className)) {
        return detail::UniqueWrenHandle(nullptr, detail::WrenHandleDeleter{.vm = raw()});
    }
    constexpr int totalSlots = 1 + sizeof...(Args);
    ensureSlots(totalSlots);
    getVariable(module, className, 0);
    int slot = 1;
    ([&]() { setSlot(slot++, args); }(), ...);

    if (call(newHandle.get()) != WREN_RESULT_SUCCESS) {
        return detail::UniqueWrenHandle(nullptr, detail::WrenHandleDeleter{.vm = raw()});
    }
    return getSlot<detail::UniqueWrenHandle>(0);
}

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
    } else {
        static_assert(detail::always_false_v<DecayedT>, "Unsupported slot type for WrenVM::setSlot");
    }
}

template <typename T>
std::optional<T> WrenVM::getSlotChecked(int slot) {
    using DecayedT = std::decay_t<T>;

    if (slot < 0 || slot >= wrenGetSlotCount(raw())) {
        return std::nullopt;
    }

    if constexpr (std::is_same_v<DecayedT, bool>) {
        if (wrenGetSlotType(raw(), slot) != WREN_TYPE_BOOL) {
            return std::nullopt;
        }
        return wrenGetSlotBool(raw(), slot);
    } else if constexpr (std::is_floating_point_v<DecayedT>) {
        if (wrenGetSlotType(raw(), slot) != WREN_TYPE_NUM) {
            return std::nullopt;
        }
        double const d = wrenGetSlotDouble(raw(), slot);
        if (std::isnan(d) || std::isinf(d)) {
            return std::nullopt;
        }
        return static_cast<T>(d);
    } else if constexpr (std::is_integral_v<DecayedT>) {
        if (wrenGetSlotType(raw(), slot) != WREN_TYPE_NUM) {
            return std::nullopt;
        }
        double const d = wrenGetSlotDouble(raw(), slot);
        if (std::isnan(d) || std::isinf(d)) {
            return std::nullopt;
        }
        if (d < static_cast<double>(std::numeric_limits<DecayedT>::lowest()) ||
            d > static_cast<double>(std::numeric_limits<DecayedT>::max())) {
            return std::nullopt;
        }
        return static_cast<T>(d);
    } else if constexpr (std::is_same_v<DecayedT, std::string_view>) {
        if (wrenGetSlotType(raw(), slot) != WREN_TYPE_STRING) {
            return std::nullopt;
        }
        int length = 0;
        const char* str = wrenGetSlotBytes(raw(), slot, &length);
        return std::string_view(str, static_cast<std::size_t>(length));
    } else if constexpr (std::is_same_v<DecayedT, std::string>) {
        if (wrenGetSlotType(raw(), slot) != WREN_TYPE_STRING) {
            return std::nullopt;
        }
        int length = 0;
        const char* str = wrenGetSlotBytes(raw(), slot, &length);
        return std::string(str, static_cast<std::size_t>(length));
    } else if constexpr (std::is_same_v<DecayedT, WrenHandle*>) {
        return wrenGetSlotHandle(raw(), slot);
    } else if constexpr (std::is_same_v<DecayedT, detail::UniqueWrenHandle>) {
        return detail::UniqueWrenHandle(wrenGetSlotHandle(raw(), slot), detail::WrenHandleDeleter{.vm = raw()});
    } else {
        static_assert(detail::always_false_v<DecayedT>, "Unsupported slot type for WrenVM::getSlotChecked");
        return std::nullopt;
    }
}

template <typename T>
T WrenVM::getSlot(int slot) {
    auto opt = getSlotChecked<T>(slot);
    if (!opt.has_value()) {
        throw std::runtime_error("Wren slot type mismatch or out of bounds for requested type");
    }
    return std::move(*opt);
}

}  // namespace scripting
