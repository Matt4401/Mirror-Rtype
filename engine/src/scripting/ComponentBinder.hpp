#pragma once
#include <string>
#include <utility>
#include <vector>

extern "C" {
#include <wren.h>
}

#include "scripting/WrenVM.hpp"
#include "scripting/detail/PropertyAccessor.hpp"

namespace scripting {

/**
 * @brief Non-template base class for ComponentBinder managing foreign class registration and code generation.
 *
 * Encapsulates the VM reference, module name, class name, and registered properties,
 * moving string formatting and non-template FFI mechanics into ComponentBinder.cpp.
 */
class ComponentBinderBase {
  public:
    /**
     * @brief Constructs a ComponentBinderBase and registers the foreign class with Wren.
     *
     * Registers the foreign class allocator that stores the entity ID inside Wren's foreign slot.
     *
     * @param vm The WrenVM wrapper to bind the component into.
     * @param moduleName The Wren module name (e.g. "core" or "game").
     * @param className The name of the class exposed to Wren (e.g. "Position").
     */
    ComponentBinderBase(WrenVM& vm, std::string moduleName, std::string className);
    virtual ~ComponentBinderBase() = default;

    ComponentBinderBase(const ComponentBinderBase&) = delete;
    ComponentBinderBase& operator=(const ComponentBinderBase&) = delete;
    ComponentBinderBase(ComponentBinderBase&&) noexcept = default;
    ComponentBinderBase& operator=(ComponentBinderBase&&) noexcept = default;

    /**
     * @brief Generates the corresponding Wren script class definition.
     *
     * Produces a string declaring the foreign class constructor and foreign getters/setters.
     *
     * @return Generated Wren source code string.
     */
    [[nodiscard]] std::string generate_wren_class() const;

    /**
     * @brief Retrieves the Wren module name containing this component class.
     * @return Reference to the module name string.
     */
    [[nodiscard]] const std::string& get_module() const noexcept;

    /**
     * @brief Retrieves the Wren class name for this component.
     * @return Reference to the class name string.
     */
    [[nodiscard]] const std::string& get_class_name() const noexcept;

  protected:
    /**
     * @brief Registers getter and setter foreign methods in the VM and appends the property name.
     *
     * @param propName The property name as accessed in Wren.
     * @param getter Function pointer to the getter FFI callback.
     * @param setter Function pointer to the setter FFI callback.
     */
    void register_property_methods(const std::string& propName, WrenForeignMethodFn getter, WrenForeignMethodFn setter);

  private:
    WrenVM* _vm = nullptr;
    std::string _module;
    std::string _className;
    std::vector<std::string> _properties;
};

/**
 * @brief Type-safe fluent builder for binding C++ ECS components to Wren scripts.
 *
 * Uses Non-Type Template Parameters (NTTP) pointer-to-members to eliminate macros,
 * providing zero-overhead, type-safe getters and setters in Wren.
 *
 * Example usage:
 * @code
 * ComponentBinder<Position> binder(vm, "core", "Position");
 * binder.property<&Position::x>("x")
 *       .property<&Position::y>("y");
 * vm.interpret("core", binder.generate_wren_class());
 * @endcode
 *
 * @tparam ComponentType The C++ ECS component aggregate type.
 */
template <typename ComponentType>
class ComponentBinder : public ComponentBinderBase {
  public:
    /**
     * @brief Constructs a ComponentBinder for the specified component type.
     *
     * @param vm The WrenVM wrapper.
     * @param moduleName Module name where the class lives in Wren.
     * @param className Name of the class exposed to Wren.
     */
    ComponentBinder(WrenVM& vm, std::string moduleName, std::string className)
        : ComponentBinderBase(vm, std::move(moduleName), std::move(className)) {}

    /**
     * @brief Binds a struct member field as a Wren property with getter and setter.
     *
     * @tparam MemberPtr Compile-time pointer-to-member (e.g. `&Position::x`).
     * @param propName The property name visible to Wren scripts.
     * @return Reference to this binder for fluent chaining.
     */
    template <auto MemberPtr>
    ComponentBinder& property(const std::string& propName) {
        register_property_methods(propName, &detail::PropertyAccessor<ComponentType, MemberPtr>::getter,
                                  &detail::PropertyAccessor<ComponentType, MemberPtr>::setter);
        return *this;
    }
};

}  // namespace scripting
