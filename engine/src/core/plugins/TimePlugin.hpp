/**
 * @file TimePlugin.hpp
 * @brief Implicit time simulation plugin managing Clock and Time resources.
 */

#pragma once

#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"

namespace engine {

/**
 * @brief Time management plugin providing fixed and variable simulation time resources.
 */
class TimePlugin final : public IPlugin {
  public:
    /** @brief Default constructor. */
    TimePlugin() = default;

    /** @brief Destructor. */
    ~TimePlugin() override = default;

    /** @brief Deleted copy constructor. */
    TimePlugin(const TimePlugin&) = delete;

    /** @brief Deleted copy assignment operator. */
    TimePlugin& operator=(const TimePlugin&) = delete;

    /** @brief Default move constructor. */
    TimePlugin(TimePlugin&&) noexcept = default;

    /** @brief Default move assignment operator. */
    TimePlugin& operator=(TimePlugin&&) noexcept = default;

    /**
     * @brief Gets plugin name.
     * @return String view "TimePlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "TimePlugin"; }

    /**
     * @brief Declares dependency on CorePlugin.
     * @param builder Dependency builder.
     */
    void build(PluginBuilder& builder) override;

    /**
     * @brief Registers the Time resource into the ECS Registry.
     * @param engine Reference to host Engine.
     */
    void init(Engine& engine) override;

    /**
     * @brief Synchronizes the Time resource before fixedUpdate systems execute.
     * @param engine Reference to host Engine.
     * @param time Current simulation step timing.
     */
    void fixedUpdate(Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Synchronizes the Time resource for variable-rate frame updates.
     * @param engine Reference to host Engine.
     * @param time Frame timing.
     */
    void update(Engine& engine, const ecs::time::Time& time) override;
};

}  // namespace engine
