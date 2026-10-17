/**
 * @file WrenVM.hpp
 * @brief High-level C++23 RAII wrapper and manager for the Wren Virtual Machine.
 */

#pragma once
#include <expected>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

extern "C" {
#include <wren.h>
}

#include "scripting/detail/WrenTypes.hpp"

namespace scripting {

/**
 * @brief High-level C++23 RAII facade and manager for the Wren Virtual Machine.
 *
 * Wraps Wren's C API into a type-safe, ergonomic interface featuring:
 * - RAII handle management via `UniqueWrenHandle`.
 * - Modern C++23 template slot marshaling (`setSlot<T>`, `getSlot<T>`).
 * - Strong error handling with `std::expected<void, std::string>`.
 * - Foreign method and class registry maps.
 * - Integration with engine memory and module loading subsystems.
 */
class WrenVM {
  public:
    /**
     * @brief Signature for module loading callbacks.
     * Given a module name, returns the Wren source code as a string.
     */
    using ModuleLoader = std::function<std::string(std::string_view moduleName)>;

    /**
     * @brief Signature for text output interception (e.g. System.print).
     */
    using WriteHandler = std::function<void(std::string_view text)>;

    /**
     * @brief Signature for error notifications (compile errors, runtime errors, stack traces).
     */
    using ErrorHandler =
        std::function<void(WrenErrorType type, std::string_view module, int line, std::string_view message)>;

    /**
     * @brief Constructs a new WrenVM instance and initializes internal configuration.
     */
    WrenVM();

    /**
     * @brief Destroys the WrenVM instance and releases all associated VM resources.
     */
    ~WrenVM();

    /** @brief Deleted copy constructor. */
    WrenVM(const WrenVM&) = delete;
    /** @brief Deleted copy assignment operator. */
    WrenVM& operator=(const WrenVM&) = delete;

    /**
     * @brief Move constructor transferring VM ownership, handles, and callbacks.
     * @param other Rvalue reference to the source WrenVM.
     */
    WrenVM(WrenVM&& other) noexcept;

    /**
     * @brief Move assignment operator transferring VM ownership, handles, and callbacks.
     * @param other Rvalue reference to the source WrenVM.
     * @return Reference to this instance.
     */
    WrenVM& operator=(WrenVM&& other) noexcept;

    /**
     * @brief Sets the custom module loader used when Wren encounters an `import` statement.
     * @param loader Callable returning module source code given a module name.
     */
    void setModuleLoader(ModuleLoader loader);

    /**
     * @brief Retrieves the active module loader callback.
     */
    [[nodiscard]] const ModuleLoader& getModuleLoader() const noexcept;

    /**
     * @brief Recreates the internal Wren virtual machine while preserving foreign bindings and callbacks.
     */
    void reset();

    /**
     * @brief Sets the text output handler intercepting `System.print`.
     * @param handler Callable taking output text.
     */
    void setWriteHandler(WriteHandler handler);

    /**
     * @brief Sets the error handler invoked during compile or runtime errors.
     * @param handler Callable receiving error type, module, line, and message.
     */
    void setErrorHandler(ErrorHandler handler);

    /**
     * @brief Registers a foreign method callback with Wren.
     *
     * @param module Wren module name containing the class.
     * @param className Wren class name owning the foreign method.
     * @param isStatic True if the method is static, false for instance methods.
     * @param signature Wren method signature (e.g. "wait(_)" or "x=(_)").
     * @param fn C callback function pointer matching WrenForeignMethodFn.
     */
    void registerForeignMethod(std::string_view module, std::string_view className, bool isStatic,
                               std::string_view signature, WrenForeignMethodFn fn);

    /**
     * @brief Registers foreign class allocation and finalization callbacks.
     *
     * @param module Wren module name.
     * @param className Wren class name.
     * @param methods Struct containing `allocate` and `finalize` function pointers.
     */
    void registerForeignClass(std::string_view module, std::string_view className, WrenForeignClassMethods methods);

    /**
     * @brief Interprets and executes Wren source code within a given module scope.
     *
     * @param module Module name (e.g. "main" or "gameplay").
     * @param source Wren source code string.
     * @return `std::expected<void, std::string>` containing success or a formatted error message.
     */
    std::expected<void, std::string> interpret(std::string_view module, std::string_view source);

    /**
     * @brief Creates a call handle for repeatedly invoking a method with minimal overhead.
     *
     * @param signature Method signature (e.g. "update(_)" or "onSpawn()").
     * @return RAII `detail::UniqueWrenHandle` automatically released on destruction.
     */
    [[nodiscard]] detail::UniqueWrenHandle makeCallHandle(std::string_view signature);

    /**
     * @brief Manually releases a raw Wren handle.
     * @param handle Raw handle pointer to release.
     */
    void releaseHandle(WrenHandle* handle);

    /**
     * @brief Invokes a method using a raw handle.
     * @param method Raw Wren handle to invoke.
     * @return Result status (success, runtime error).
     */
    WrenInterpretResult call(WrenHandle* method);

    /**
     * @brief Invokes a method using a RAII UniqueWrenHandle.
     * @param method Owning handle reference.
     * @return Result status.
     */
    WrenInterpretResult call(const detail::UniqueWrenHandle& method);

    /**
     * @brief Checks if a module has been loaded/imported into the VM.
     * @param module Module name.
     * @return True if the module is loaded.
     */
    [[nodiscard]] bool hasModule(std::string_view module);

    /**
     * @brief Checks if a top-level variable exists in a given module.
     * @param module Module name.
     * @param variableName Variable / class identifier.
     * @return True if the variable exists.
     */
    bool hasVariable(std::string_view module, std::string_view variableName);

    /**
     * @brief Loads a module variable into a slot.
     * @param module Module name.
     * @param variableName Variable identifier.
     * @param slot Target slot index.
     */
    void getVariable(std::string_view module, std::string_view variableName, int slot);

    /**
     * @brief Triggers a garbage collection cycle in the Wren VM.
     */
    void collectGarbage();

    /**
     * @brief Sets the ECS Registry pointer for component binding access.
     * @param registry Opaque pointer to the ecs::Registry.
     */
    void setRegistry(void* registry) noexcept;

    /**
     * @brief Retrieves the registered ECS Registry pointer.
     * @return Opaque pointer to the ecs::Registry.
     */
    [[nodiscard]] void* getRegistry() const noexcept;

    /**
     * @brief Sets a custom user context pointer (e.g. ScriptSystem*).
     * @param ctx Opaque pointer to the user context.
     */
    void setUserContext(void* ctx) noexcept;

    /**
     * @brief Retrieves the custom user context pointer.
     * @return Opaque pointer to the user context.
     */
    [[nodiscard]] void* getUserContext() const noexcept;

    /**
     * @brief Provides direct access to the raw Wren C VM.
     * @return Raw ::WrenVM pointer.
     */
    [[nodiscard]] ::WrenVM* raw() noexcept;

    /**
     * @brief Ensures at least `numSlots` are available in the slot array.
     * @param numSlots Minimum slot count needed.
     */
    void ensureSlots(int numSlots);

    /**
     * @brief Sets a value into a Wren VM slot with compile-time type deduction.
     *
     * Supported types:
     * - `std::nullptr_t` -> wrenSetSlotNull
     * - `bool` -> wrenSetSlotBool
     * - Arithmetic types (int, float, double, enums) -> wrenSetSlotDouble
     * - String types (`std::string_view`, `std::string`, `const char*`) -> wrenSetSlotBytes
     * - `WrenHandle*` / `detail::UniqueWrenHandle` -> wrenSetSlotHandle
     *
     * @tparam T Value type.
     * @param slot Target slot index.
     * @param value The value to place into the slot.
     */
    template <typename T>
    void setSlot(int slot, const T& value);

    /**
     * @brief Extracts a value of type T from a Wren VM slot.
     *
     * Supported types:
     * - `bool` -> wrenGetSlotBool
     * - Arithmetic types (int, float, double, etc.) -> wrenGetSlotDouble cast to T
     * - `std::string_view` -> wrenGetSlotBytes
     * - `std::string` -> std::string from wrenGetSlotBytes
     * - `WrenHandle*` -> wrenGetSlotHandle
     * - `detail::UniqueWrenHandle` -> RAII handle from wrenGetSlotHandle
     *
     * @tparam T Desired output type.
     * @param slot Slot index to read from.
     * @return Extracted value.
     */
    template <typename T>
    [[nodiscard]] T getSlot(int slot);

  private:
    /**
     * @brief Wren VM callback binding foreign methods to C++ functions.
     * @param vm Raw Wren VM pointer.
     * @param module Module name.
     * @param className Class name.
     * @param isStatic True if static method.
     * @param signature Method signature.
     * @return Function pointer to the foreign method implementation, or nullptr.
     */
    static WrenForeignMethodFn onBindForeignMethod(::WrenVM* vm, const char* module, const char* className,
                                                   bool isStatic, const char* signature);

    /**
     * @brief Wren VM callback binding foreign class allocation and finalization functions.
     * @param vm Raw Wren VM pointer.
     * @param module Module name.
     * @param className Class name.
     * @return Struct containing allocate and finalize callbacks.
     */
    static WrenForeignClassMethods onBindForeignClass(::WrenVM* vm, const char* module, const char* className);

    /**
     * @brief Wren VM callback for module import resolution.
     * @param vm Raw Wren VM pointer.
     * @param name Module name to import.
     * @return Struct containing source code or load error status.
     */
    static WrenLoadModuleResult onLoadModule(::WrenVM* vm, const char* name);

    /**
     * @brief Wren VM callback redirecting standard output from `System.print`.
     * @param vm Raw Wren VM pointer.
     * @param text Printed text string.
     */
    static void onWrite(::WrenVM* vm, const char* text);

    /**
     * @brief Wren VM callback receiving compile and runtime error diagnostics.
     * @param vm Raw Wren VM pointer.
     * @param type Error category (compile error, runtime error, stack trace).
     * @param module Module where the error occurred.
     * @param line Line number of the error.
     * @param message Diagnostic error message.
     */
    static void onError(::WrenVM* vm, WrenErrorType type, const char* module, int line, const char* message);

    /** @brief Underlying raw pointer to the Wren VM C instance. */
    ::WrenVM* _vm = nullptr;
    /** @brief Opaque pointer to the ECS Registry for component lookups. */
    void* _registry = nullptr;
    /** @brief Opaque pointer to user context (e.g. ScriptSystem). */
    void* _userContext = nullptr;
    /** @brief Registered module loader callback. */
    ModuleLoader _moduleLoader;
    /** @brief Registered standard output callback. */
    WriteHandler _writeHandler;
    /** @brief Registered error diagnostic callback. */
    ErrorHandler _errorHandler;
    /** @brief Diagnostic error message from the last interpretation error. */
    std::string _lastError;

    /** @brief Cache of loaded module source strings for inspection. */
    std::unordered_map<std::string, std::string> _loadedSources;
    /** @brief Registry of foreign method callbacks indexed by composite key. */
    std::unordered_map<detail::ForeignMethodKey, WrenForeignMethodFn, detail::ForeignMethodKeyHash> _foreignMethods;
    /** @brief Registry of foreign class lifecycle callback tables indexed by "module:class". */
    std::unordered_map<std::string, WrenForeignClassMethods> _foreignClasses;
};

}  // namespace scripting

#include "WrenVM.tpp"  // IWYU pragma: export
