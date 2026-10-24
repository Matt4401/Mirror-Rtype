/**
 * @file ForeignCall.hpp
 * @brief Type-safe and exception-safe wrapper for Wren foreign method invocations and slot marshaling.
 */

#pragma once
#include <optional>
#include <string_view>

#include "scripting/detail/WrenApi.hpp"  // IWYU pragma: export

namespace scripting {

class WrenVM;

/**
 * @brief Helper context object wrapping raw Wren VM slots in foreign callbacks.
 *
 * Provides type validation, integer bound checking, zero-copy string views,
 * exception-safe fiber abortion, and return slot helpers.
 */
class ForeignCall {
  public:
    /**
     * @brief Constructs a ForeignCall wrapping a raw Wren VM instance.
     * @param vm Raw Wren VM pointer.
     */
    explicit ForeignCall(::WrenVM* vm) noexcept : _vm(vm) {}

    /** @brief Returns the raw Wren VM pointer. */
    [[nodiscard]] ::WrenVM* raw() const noexcept { return _vm; }

    /** @brief Returns a pointer to the enclosing C++ WrenVM facade. */
    [[nodiscard]] WrenVM* vm() const noexcept;

    /**
     * @brief Retrieves the custom user context pointer registered with the WrenVM.
     * @tparam T Context type.
     * @return Pointer to user context or nullptr if missing.
     */
    template <typename T>
    [[nodiscard]] T* userData() const noexcept;

    /** @brief Returns the number of slots available in this foreign call. */
    [[nodiscard]] int slotCount() const noexcept { return wrenGetSlotCount(_vm); }

    /**
     * @brief Returns the Wren low-level type of the object stored in slot.
     * @param slot Slot index.
     */
    [[nodiscard]] WrenType slotType(int slot) const noexcept { return wrenGetSlotType(_vm, slot); }

    /**
     * @brief Aborts the current Wren fiber with an error message in slot 0.
     * @param message Descriptive error message.
     */
    void abort(std::string_view message) const noexcept {
        wrenEnsureSlots(_vm, 1);
        wrenSetSlotBytes(_vm, 0, message.data(), message.size());
        wrenAbortFiber(_vm, 0);
    }

    /**
     * @brief Extracts and type-checks an argument from a slot.
     *
     * Validates that the slot exists and matches the requested type T. Rejects NaN and
     * infinite values for arithmetic types, and checks integer bounds. If validation
     * fails, automatically aborts the fiber with an error and returns std::nullopt.
     *
     * @tparam T Desired output type.
     * @param slot Zero-based slot index (slot 0 is receiver, slot 1+ are arguments).
     * @return Extracted value, or std::nullopt on type mismatch or bounds failure.
     */
    template <typename T>
    [[nodiscard]] std::optional<T> arg(int slot) const noexcept;

    /**
     * @brief Extracts a foreign object pointer stored in a slot.
     *
     * Validates that the slot contains a foreign object. Aborts fiber on mismatch.
     *
     * @tparam T Foreign object struct/class type.
     * @param slot Slot index.
     * @return Pointer to foreign object, or nullptr if slot is not foreign.
     */
    template <typename T>
    [[nodiscard]] T* argForeign(int slot) const noexcept {
        if (slot >= slotCount() || wrenGetSlotType(_vm, slot) != WREN_TYPE_FOREIGN) {
            abort("Expected argument to be a foreign object");
            return nullptr;
        }
        return static_cast<T*>(wrenGetSlotForeign(_vm, slot));
    }

    /**
     * @brief Sets slot 0 to null.
     */
    void retNull() const noexcept { wrenSetSlotNull(_vm, 0); }

    /**
     * @brief Allocates a new empty list in slot 0.
     */
    void retList() const noexcept {
        wrenEnsureSlots(_vm, 1);
        wrenSetSlotNewList(_vm, 0);
    }

    /**
     * @brief Writes a return value into slot 0.
     * @tparam T Value type.
     * @param value Value to return.
     */
    template <typename T>
    void ret(const T& value) const noexcept;

    /**
     * @brief Allocates a new foreign object in slot 0.
     * @tparam T Foreign data struct type.
     * @param classSlot Slot containing the foreign class object (default 0 for constructors).
     * @return Pointer to newly allocated foreign memory.
     */
    template <typename T>
    T* retNewForeign(int classSlot = 0) const noexcept {
        return static_cast<T*>(wrenSetSlotNewForeign(_vm, 0, classSlot, sizeof(T)));
    }

  private:
    ::WrenVM* _vm = nullptr;
};

}  // namespace scripting

#include "scripting/ForeignCall.tpp"  // IWYU pragma: export
