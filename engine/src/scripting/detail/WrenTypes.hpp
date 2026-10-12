#pragma once
#include <functional>
#include <memory>
#include <string>

extern "C" {
#include <wren.h>
}

namespace scripting::detail {

/**
 * @brief Composite lookup key identifying a registered Wren foreign method.
 */
struct ForeignMethodKey {
    /** @brief Wren module containing the class (e.g. "core/Engine"). */
    std::string module;
    /** @brief Wren class name owning the foreign method (e.g. "Engine"). */
    std::string className;
    /** @brief True if the method is static, false if it is an instance method. */
    bool isStatic = false;
    /** @brief Signature including parameter count (e.g. "wait(_)"). */
    std::string signature;

    bool operator==(const ForeignMethodKey& other) const noexcept {
        return isStatic == other.isStatic && module == other.module && className == other.className &&
               signature == other.signature;
    }
};

/**
 * @brief Custom hasher for ForeignMethodKey allowing use in std::unordered_map.
 */
struct ForeignMethodKeyHash {
    std::size_t operator()(const ForeignMethodKey& k) const noexcept {
        std::size_t const h1 = std::hash<std::string>{}(k.module);
        std::size_t const h2 = std::hash<std::string>{}(k.className);
        std::size_t const h3 = std::hash<std::string>{}(k.signature);
        std::size_t const h4 = std::hash<bool>{}(k.isStatic);
        return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
    }
};

/**
 * @brief Custom RAII deleter for WrenHandle pointers bound to a specific Wren VM.
 */
struct WrenHandleDeleter {
    /** @brief Pointer to the raw Wren VM that owns the handle. */
    ::WrenVM* vm = nullptr;

    /**
     * @brief Releases the handle via `wrenReleaseHandle` if both handle and vm are valid.
     * @param handle Raw handle to release.
     */
    void operator()(WrenHandle* handle) const noexcept {
        if (handle != nullptr && vm != nullptr) {
            wrenReleaseHandle(vm, handle);
        }
    }
};

/**
 * @brief Unique ownership RAII smart pointer for a WrenHandle.
 */
using UniqueWrenHandle = std::unique_ptr<WrenHandle, WrenHandleDeleter>;

}  // namespace scripting::detail
