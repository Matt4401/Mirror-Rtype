#pragma once
#include <expected>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "scripting/detail/WrenTypes.hpp"

namespace scripting {

/**
 * @brief High-level C++23 RAII facade and manager for the Wren Virtual Machine.
 *
 * Wraps Wren's C API into a type-safe, ergonomic interface featuring:
 * - RAII handle management via `UniqueWrenHandle`.
 * - Modern C++23 template slot marshaling (`set_slot<T>`, `get_slot<T>`).
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

    WrenVM();
    ~WrenVM();

    WrenVM(const WrenVM&) = delete;
    WrenVM& operator=(const WrenVM&) = delete;

    WrenVM(WrenVM&& other) noexcept;
    WrenVM& operator=(WrenVM&& other) noexcept;

    /**
     * @brief Sets the custom module loader used when Wren encounters an `import` statement.
     * @param loader Callable returning module source code given a module name.
     */
    void set_module_loader(ModuleLoader loader);

    /**
     * @brief Sets the text output handler intercepting `System.print`.
     * @param handler Callable taking output text.
     */
    void set_write_handler(WriteHandler handler);

    /**
     * @brief Sets the error handler invoked during compile or runtime errors.
     * @param handler Callable receiving error type, module, line, and message.
     */
    void set_error_handler(ErrorHandler handler);

    /**
     * @brief Registers a foreign method callback with Wren.
     *
     * @param module Wren module name containing the class.
     * @param className Wren class name owning the foreign method.
     * @param isStatic True if the method is static, false for instance methods.
     * @param signature Wren method signature (e.g. "wait(_)" or "x=(_)").
     * @param fn C callback function pointer matching WrenForeignMethodFn.
     */
    void register_foreign_method(std::string_view module, std::string_view className, bool isStatic,
                                 std::string_view signature, WrenForeignMethodFn fn);

    /**
     * @brief Registers foreign class allocation and finalization callbacks.
     *
     * @param module Wren module name.
     * @param className Wren class name.
     * @param methods Struct containing `allocate` and `finalize` function pointers.
     */
    void register_foreign_class(std::string_view module, std::string_view className, WrenForeignClassMethods methods);

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
    [[nodiscard]] detail::UniqueWrenHandle make_call_handle(std::string_view signature);

    /**
     * @brief Manually releases a raw Wren handle.
     * @param handle Raw handle pointer to release.
     */
    void release_handle(WrenHandle* handle);

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
    [[nodiscard]] bool has_module(std::string_view module);

    /**
     * @brief Checks if a top-level variable exists in a given module.
     * @param module Module name.
     * @param variableName Variable / class identifier.
     * @return True if the variable exists.
     */
    bool has_variable(std::string_view module, std::string_view variableName);

    /**
     * @brief Loads a module variable into a slot.
     * @param module Module name.
     * @param variableName Variable identifier.
     * @param slot Target slot index.
     */
    void get_variable(std::string_view module, std::string_view variableName, int slot);

    /**
     * @brief Triggers a garbage collection cycle in the Wren VM.
     */
    void collect_garbage();

    /**
     * @brief Sets the ECS Registry pointer for component binding access.
     * @param registry Opaque pointer to the ecs::Registry.
     */
    void set_registry(void* registry) noexcept;

    /**
     * @brief Retrieves the registered ECS Registry pointer.
     * @return Opaque pointer to the ecs::Registry.
     */
    [[nodiscard]] void* get_registry() const noexcept;

    /**
     * @brief Sets a custom user context pointer (e.g. ScriptSystem*).
     * @param ctx Opaque pointer to the user context.
     */
    void set_user_context(void* ctx) noexcept;

    /**
     * @brief Retrieves the custom user context pointer.
     * @return Opaque pointer to the user context.
     */
    [[nodiscard]] void* get_user_context() const noexcept;

    /**
     * @brief Provides direct access to the raw Wren C VM.
     * @return Raw ::WrenVM pointer.
     */
    [[nodiscard]] ::WrenVM* raw() noexcept;

    /**
     * @brief Ensures at least `numSlots` are available in the slot array.
     * @param numSlots Minimum slot count needed.
     */
    void ensure_slots(int numSlots);

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
    void set_slot(int slot, const T& value);

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
    [[nodiscard]] T get_slot(int slot);

  private:
    static WrenForeignMethodFn on_bind_foreign_method(::WrenVM* vm, const char* module, const char* className,
                                                      bool isStatic, const char* signature);

    static WrenForeignClassMethods on_bind_foreign_class(::WrenVM* vm, const char* module, const char* className);

    static WrenLoadModuleResult on_load_module(::WrenVM* vm, const char* name);
    static void on_write(::WrenVM* vm, const char* text);
    static void on_error(::WrenVM* vm, WrenErrorType type, const char* module, int line, const char* message);

    ::WrenVM* _vm = nullptr;
    void* _registry = nullptr;
    void* _userContext = nullptr;
    ModuleLoader _moduleLoader;
    WriteHandler _writeHandler;
    ErrorHandler _errorHandler;
    std::string _lastError;

    std::unordered_map<std::string, std::string> _loadedSources;
    std::unordered_map<detail::ForeignMethodKey, WrenForeignMethodFn, detail::ForeignMethodKeyHash> _foreignMethods;
    std::unordered_map<std::string, WrenForeignClassMethods> _foreignClasses;
};

}  // namespace scripting

#include "WrenVM.tpp"
