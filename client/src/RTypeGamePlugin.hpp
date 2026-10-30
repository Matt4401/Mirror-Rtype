/**
 * @file RTypeGamePlugin.hpp
 * @brief Game logic plugin configuring R-Type demo scene, inputs, and gameplay systems.
 */

#pragma once

#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"

namespace client {

/**
 * @brief Concrete game plugin for R-Type client demonstration.
 *
 * Encapsulates game scene setup, player movement, projectile updates, and weapon firing.
 */
class RTypeGamePlugin final : public engine::IPlugin {
  public:
    /** @brief Constructs RTypeGamePlugin. */
    RTypeGamePlugin() = default;

    /** @brief Destructor. */
    ~RTypeGamePlugin() override = default;

    /** @brief Deleted copy constructor. */
    RTypeGamePlugin(const RTypeGamePlugin&) = delete;

    /** @brief Deleted copy assignment operator. */
    RTypeGamePlugin& operator=(const RTypeGamePlugin&) = delete;

    /** @brief Deleted move constructor. */
    RTypeGamePlugin(RTypeGamePlugin&&) = delete;

    /** @brief Deleted move assignment operator. */
    RTypeGamePlugin& operator=(RTypeGamePlugin&&) = delete;

    /**
     * @brief Gets plugin name.
     * @return String view "RTypeGamePlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "RTypeGamePlugin"; }

    /**
     * @brief Declares dependencies on RenderPlugin, InputPlugin, PhysicsPlugin.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Configures action bindings and spawns the initial demo scene.
     * @param engine Reference to host Engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Fixed simulation update moving player, bullets, and obstacles.
     * @param engine Reference to host Engine.
     * @param time Fixed simulation timing information.
     */
    void fixedUpdate(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Frame update checking player shooting triggers.
     * @param engine Reference to host Engine.
     * @param time Frame timing information.
     */
    void update(engine::Engine& engine, const ecs::time::Time& time) override;
};

}  // namespace client
