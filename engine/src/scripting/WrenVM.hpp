/**
 * @file WrenVM.hpp
 * @brief High-level C++23 RAII wrapper and manager for the Wren Virtual Machine.
 */

#pragma once
#include <cstddef>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "scripting/detail/WrenApi.hpp"    // IWYU pragma: export
#include "scripting/detail/WrenTypes.hpp"  // IWYU pragma: export

namespace scripting {

class ForeignCall;

namespace detail {

inline std::string makeMethodSignature(std::string_view name, std::size_t arity) {
    std::string sig(name);
    sig += "(";
    for (std::size_t i = 0; i < arity; ++i) {
        if (i > 0) {
            sig += ",";
        }
        sig += "_";
    }
    sig += ")";
    return sig;
}

}  // namespace detail

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
     * @brief Registers a foreign method callback with Wren using a raw C function pointer.
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
     * @brief Registers a foreign method callback with Wren wrapped in an exception-safe noexcept trampoline.
     *
     * The callback Fn can accept `ForeignCall&` or `::WrenVM*`. Any thrown C++ exceptions are caught
     * and translated to `wrenAbortFiber`.
     *
     * @tparam Fn Compile-time function pointer to invoke.
     * @param module Wren module name.
     * @param className Wren class name.
     * @param isStatic True if static method.
     * @param signature Method signature.
     */
    template <auto Fn>
    void registerForeignMethod(std::string_view module, std::string_view className, bool isStatic,
                               std::string_view signature);

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
     * @brief High-level call API invoking a method on a receiver with variadic arguments.
     *
     * Ensures slots, writes the receiver into slot 0, writes args into slots 1+,
     * executes the method, and returns the interpret result.
     *
     * @tparam Receiver Receiver type (e.g. WrenHandle*, detail::UniqueWrenHandle, double).
     * @tparam Args Argument types.
     * @param method Method call handle.
     * @param receiver Target receiver object.
     * @param args Arguments to pass to the method.
     * @return Wren interpret result code.
     */
    template <typename Receiver, typename... Args>
    WrenInterpretResult call(WrenHandle* method, const Receiver& receiver, const Args&... args);

    /**
     * @brief High-level call API taking a RAII UniqueWrenHandle method.
     */
    template <typename Receiver, typename... Args>
    WrenInterpretResult call(const detail::UniqueWrenHandle& method, const Receiver& receiver, const Args&... args);

    /**
     * @brief Instantiates a Wren class with arguments and returns an owning handle to the instance.
     *
     * @tparam Args Argument types.
     * @param module Module containing the class.
     * @param className Name of the class to instantiate.
     * @param args Arguments forwarded to constructor.
     * @return Owning UniqueWrenHandle to the new instance, or null handle on failure.
     */
    template <typename... Args>
    [[nodiscard]] detail::UniqueWrenHandle newInstance(std::string_view module, std::string_view className,
                                                       const Args&... args);

    /**
     * @brief Instantiates a Wren class using a pre-cached constructor call handle.
     */
    template <typename... Args>
    [[nodiscard]] detail::UniqueWrenHandle newInstanceWithHandle(const detail::UniqueWrenHandle& newHandle,
                                                                 std::string_view module, std::string_view className,
                                                                 const Args&... args);

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
     */
    template <typename T>
    void setSlot(int slot, const T& value);

    /**
     * @brief Safely extracts a value from a Wren slot with type checking.
     * @tparam T Desired output type.
     * @param slot Slot index.
     * @return Extracted value, or std::nullopt if type mismatch or out of bounds.
     */
    template <typename T>
    [[nodiscard]] std::optional<T> getSlotChecked(int slot);

    /**
     * @brief Extracts a value of type T from a Wren VM slot, throwing if type does not match.
     */
    template <typename T>
    [[nodiscard]] T getSlot(int slot);

  private:
    /**
     * @brief Generic trampoline wrapping foreign methods in noexcept and try/catch.
     */
    template <auto Fn>
    static void foreignMethodTrampoline(::WrenVM* rawVm) noexcept;

    /**
     * @brief Wren VM callback binding foreign methods to C++ functions.
     */
    static WrenForeignMethodFn onBindForeignMethod(::WrenVM* vm, const char* module, const char* className,
                                                   bool isStatic, const char* signature) noexcept;

    /**
     * @brief Wren VM callback binding foreign class allocation and finalization functions.
     */
    static WrenForeignClassMethods onBindForeignClass(::WrenVM* vm, const char* module, const char* className) noexcept;

    /**
     * @brief Wren VM callback for module import resolution.
     */
    static WrenLoadModuleResult onLoadModule(::WrenVM* vm, const char* name) noexcept;

    /**
     * @brief Wren VM callback redirecting standard output from `System.print`.
     */
    static void onWrite(::WrenVM* vm, const char* text) noexcept;

    /**
     * @brief Wren VM callback receiving compile and runtime error diagnostics.
     */
    static void onError(::WrenVM* vm, WrenErrorType type, const char* module, int line, const char* message) noexcept;

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
