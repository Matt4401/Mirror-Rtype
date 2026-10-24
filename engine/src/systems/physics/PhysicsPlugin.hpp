/**
 * @file PhysicsPlugin.hpp
 * @brief Physics simulation plugin driving Box2D rigid bodies and collider synchronization.
 */

#pragma once

#include <memory>
#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "systems/physics/PhysicsSystem.hpp"

namespace systems {

/**
 * @brief Plugin responsible for synchronizing ECS transforms with the Box2D physics world.
 */
class PhysicsPlugin final : public engine::IPlugin {
  public:
    /** @brief Constructs PhysicsPlugin. */
    PhysicsPlugin() = default;

    /** @brief Destructor. */
    ~PhysicsPlugin() override;

    /** @brief Deleted copy constructor. */
    PhysicsPlugin(const PhysicsPlugin&) = delete;

    /** @brief Deleted copy assignment operator. */
    PhysicsPlugin& operator=(const PhysicsPlugin&) = delete;

    /** @brief Move constructor. */
    PhysicsPlugin(PhysicsPlugin&&) noexcept;

    /** @brief Move assignment operator. */
    PhysicsPlugin& operator=(PhysicsPlugin&&) noexcept;

    /**
     * @brief Gets plugin name.
     * @return String view "PhysicsPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "PhysicsPlugin"; }

    /**
     * @brief Declares plugin dependencies.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Instantiates PhysicsSystem and registers IPhysicsService in Mediator.
     * @param engine Reference to host Engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Advances Box2D physics simulation during fixed timestep tick.
     * @param engine Reference to host Engine.
     * @param time Fixed simulation timing information.
     */
    void fixedUpdate(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Unregisters services and releases physics world.
     * @param engine Reference to host Engine.
     */
    void shutdown(engine::Engine& engine) override;

    /** @brief Gets pointer to the underlying PhysicsSystem, or nullptr if uninitialized. */
    [[nodiscard]] PhysicsSystem* physicsSystem() noexcept { return _physicsSystem.get(); }

  private:
    class PhysicsServiceImpl;

    std::unique_ptr<PhysicsSystem> _physicsSystem = nullptr;
    std::shared_ptr<PhysicsServiceImpl> _physicsService = nullptr;
};

}  // namespace systems
