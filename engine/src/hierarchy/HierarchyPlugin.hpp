/**
 * @file HierarchyPlugin.hpp
 * @brief Engine plugin maintaining parent/child links and propagating transforms each frame.
 */

#pragma once

#include <memory>
#include <string_view>

#include "core/IPlugin.hpp"
#include "hierarchy/HierarchyHooks.hpp"

namespace hierarchy {

/**
 * @brief Plugin enabling entity hierarchies in an Engine.
 *
 * - `init`: installs `HierarchyHooks` (cascade destruction, link repair) on the engine registry.
 * - `lateUpdate`: runs `propagateTransforms`, after every plugin's `update`, so render, audio and networking read the
 *   final world transforms of children for the frame.
 *
 * Propagation owns the world `ecs::Transform` of children: move a child through its `LocalTransform`. A child with a
 * physics body should be kinematic; its propagated position is pushed to the physics world on the next fixed step.
 * Headless: depends only on `engine_core`.
 */
class HierarchyPlugin final : public engine::IPlugin {
  public:
    /** @brief Constructs HierarchyPlugin. */
    HierarchyPlugin();

    /** @brief Destructor. */
    ~HierarchyPlugin() override;

    /** @brief Deleted copy constructor. */
    HierarchyPlugin(const HierarchyPlugin&) = delete;
    /** @brief Deleted copy assignment operator. */
    HierarchyPlugin& operator=(const HierarchyPlugin&) = delete;
    /** @brief Deleted move constructor. */
    HierarchyPlugin(HierarchyPlugin&&) = delete;
    /** @brief Deleted move assignment operator. */
    HierarchyPlugin& operator=(HierarchyPlugin&&) = delete;

    /**
     * @brief Gets plugin name.
     * @return String view "HierarchyPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "HierarchyPlugin"; }

    /**
     * @brief Installs the hierarchy hooks on the engine registry.
     * @param engine Reference to host Engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Propagates world transforms down every hierarchy.
     * @param engine Reference to host Engine.
     * @param time Frame timing information (unused).
     */
    void lateUpdate(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Removes the hierarchy hooks.
     * @param engine Reference to host Engine.
     */
    void shutdown(engine::Engine& engine) override;

  private:
    /** @brief Registry listeners, alive between `init` and `shutdown`. */
    std::unique_ptr<HierarchyHooks> _hooks;
};

}  // namespace hierarchy
