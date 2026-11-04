/**
 * @file RTypeGamePlugin.hpp
 * @brief Game logic plugin configuring R-Type demo scene, inputs, and gameplay systems.
 */

#pragma once

#include <string_view>

#include "components/Renderable.hpp"
#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/CommandBuffer.hpp"

namespace client {

/**
 * @brief Concrete game plugin for R-Type client demonstration.
 *
 * Loads textures, builds the demo scene (animated ship, bydos, parallax starfield),
 * and drives player movement, shooting, and bullet cleanup each frame.
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
     * @brief Declares dependencies on RenderPlugin, InputPlugin, PhysicsPlugin, AnimationPlugin.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Loads textures, configures action bindings, and spawns the initial demo scene.
     * @param engine Reference to host Engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Fixed simulation update: moves player and bullets, kills off-screen bullets.
     * @param engine Reference to host Engine.
     * @param time Fixed simulation timing information.
     */
    void fixedUpdate(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Frame update: checks player shooting trigger.
     * @param engine Reference to host Engine.
     * @param time Frame timing information.
     */
    void update(engine::Engine& engine, const ecs::time::Time& time) override;

  private:
    /** @brief Texture identifier for the player ship spritesheet. */
    ecs::TextureId _shipTexId = ecs::NO_TEXTURE;
    /** @brief Texture identifier for the Bydos enemy spritesheet. */
    ecs::TextureId _bydosTexId = ecs::NO_TEXTURE;
    /** @brief Texture identifier for the bullet sprite. */
    ecs::TextureId _bulletTexId = ecs::NO_TEXTURE;
    /** @brief Texture identifier for small star sprites. */
    ecs::TextureId _starSmallId = ecs::NO_TEXTURE;
    /** @brief Texture identifier for medium star sprites. */
    ecs::TextureId _starMediumId = ecs::NO_TEXTURE;
    /** @brief Texture identifier for large star sprites. */
    ecs::TextureId _starLargeId = ecs::NO_TEXTURE;
    /** @brief Deferred command buffer for safe prefab spawning during update loops. */
    ecs::CommandBuffer _commandBuffer;
};

}  // namespace client
