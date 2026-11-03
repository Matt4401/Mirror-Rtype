/**
 * @file PluginBuilder.hpp
 * @brief Configuration builder used to declare type-safe plugin dependencies.
 */

#pragma once

#include <string>
#include <typeindex>
#include <vector>

namespace engine {

/**
 * @brief Dependency metadata registered for a single required or optional dependency.
 */
struct PluginDependency {
    /** @brief Type index identifying the target plugin. */
    std::type_index typeId;
    /** @brief Human-readable name of the target plugin type for error diagnostics. */
    std::string name;
};

/**
 * @brief Fluent configuration builder passed to IPlugin::build.
 */
class PluginBuilder {
  public:
    /** @brief Default constructor. */
    PluginBuilder() = default;

    /** @brief Default destructor. */
    ~PluginBuilder() = default;

    /** @brief Default copy constructor. */
    PluginBuilder(const PluginBuilder&) = default;

    /** @brief Default copy assignment operator. */
    PluginBuilder& operator=(const PluginBuilder&) = default;

    /** @brief Default move constructor. */
    PluginBuilder(PluginBuilder&&) noexcept = default;

    /** @brief Default move assignment operator. */
    PluginBuilder& operator=(PluginBuilder&&) noexcept = default;

    /**
     * @brief Declares a mandatory dependency on another plugin type.
     *
     * If the dependency is missing at engine initialization, initialization fails.
     *
     * @tparam PluginType The dependency plugin class.
     * @return Reference to this builder for method chaining.
     */
    template <typename PluginType>
    PluginBuilder& require();

    /**
     * @brief Declares an optional dependency on another plugin type.
     *
     * If present, the engine guarantees this dependency is initialized before this plugin.
     * If absent, no error occurs.
     *
     * @tparam PluginType The dependency plugin class.
     * @return Reference to this builder for method chaining.
     */
    template <typename PluginType>
    PluginBuilder& optionally();

    /**
     * @brief Declares that this plugin provides a service once initialized.
     *
     * Plugins requiring this service are initialized after this plugin.
     *
     * @tparam Service The service interface provided by this plugin.
     * @return Reference to this builder for method chaining.
     */
    template <typename Service>
    PluginBuilder& provides();

    /**
     * @brief Declares a mandatory dependency on a service, whichever plugin provides it.
     *
     * If no plugin provides the service at engine initialization, initialization fails.
     *
     * @tparam Service The service interface required by this plugin.
     * @return Reference to this builder for method chaining.
     */
    template <typename Service>
    PluginBuilder& requireService();

    /**
     * @brief Retrieves the list of required plugin dependencies.
     * @return Const reference to required dependencies list.
     */
    [[nodiscard]] const std::vector<PluginDependency>& requiredDependencies() const noexcept { return _required; }

    /**
     * @brief Retrieves the list of optional plugin dependencies.
     * @return Const reference to optional dependencies list.
     */
    [[nodiscard]] const std::vector<PluginDependency>& optionalDependencies() const noexcept { return _optional; }

    /**
     * @brief Retrieves the list of services provided by the plugin.
     * @return Const reference to provided services list.
     */
    [[nodiscard]] const std::vector<PluginDependency>& providedServices() const noexcept { return _providedServices; }

    /**
     * @brief Retrieves the list of services required by the plugin.
     * @return Const reference to required services list.
     */
    [[nodiscard]] const std::vector<PluginDependency>& requiredServices() const noexcept { return _requiredServices; }

  private:
    /** @brief List of mandatory plugin dependencies. */
    std::vector<PluginDependency> _required;
    /** @brief List of optional plugin dependencies. */
    std::vector<PluginDependency> _optional;
    /** @brief List of services provided by the plugin. */
    std::vector<PluginDependency> _providedServices;
    /** @brief List of services required by the plugin. */
    std::vector<PluginDependency> _requiredServices;
};

}  // namespace engine

#include "core/PluginBuilder.tpp"  // IWYU pragma: export
