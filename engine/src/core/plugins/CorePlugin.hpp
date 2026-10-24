/**
 * @file CorePlugin.hpp
 * @brief Implicit foundation plugin providing core ECS registry coordination.
 */

#pragma once

#include <string_view>

#include "core/IPlugin.hpp"

namespace engine {

/**
 * @brief Implicit core plugin automatically managed by Engine.
 *
 * Serves as the root dependency for all subsystems interacting with the ECS Registry.
 */
class CorePlugin final : public IPlugin {
  public:
    /** @brief Constructs CorePlugin. */
    CorePlugin() = default;

    /** @brief Destructor. */
    ~CorePlugin() override = default;

    /** @brief Deleted copy constructor. */
    CorePlugin(const CorePlugin&) = delete;

    /** @brief Deleted copy assignment operator. */
    CorePlugin& operator=(const CorePlugin&) = delete;

    /** @brief Default move constructor. */
    CorePlugin(CorePlugin&&) noexcept = default;

    /** @brief Default move assignment operator. */
    CorePlugin& operator=(CorePlugin&&) noexcept = default;

    /**
     * @brief Gets the human-readable plugin name.
     * @return String view "CorePlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "CorePlugin"; }

    /**
     * @brief Declares plugin dependencies (none for core).
     * @param builder Dependency builder.
     */
    void build(PluginBuilder& builder) override;

    /**
     * @brief Initializes core ECS listeners and base state.
     * @param engine Reference to host Engine.
     */
    void init(Engine& engine) override;

    /**
     * @brief Releases core resources.
     * @param engine Reference to host Engine.
     */
    void shutdown(Engine& engine) override;
};

}  // namespace engine
