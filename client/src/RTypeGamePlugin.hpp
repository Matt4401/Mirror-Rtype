/**
 * @file RTypeGamePlugin.hpp
 * @brief Game logic plugin configuring R-Type demo scene, inputs, and gameplay systems.
 */

#pragma once

#include <cstddef>
#include <memory>
#include <string_view>

#include "collision/CollisionRouter.hpp"
#include "components/Renderable.hpp"
#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/CommandBuffer.hpp"
#include "gameplay/ShotHitRule.hpp"

namespace client {

/**
 * @brief Concrete game plugin for R-Type client demonstration.
 *
 * Loads textures, builds the demo scene (animated ship, parallax starfield), sends waves of Bydos enemies,
 * and drives player movement, shooting, and off-screen cleanup each frame. Player shots destroy the enemies they
 * touch: physics contacts go through `CollisionRouter`, and `ShotHitRule` applies the hits.
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
     * @brief Unsubscribes the collision router and rules before the engine tears down its event bus.
     * @param engine Reference to host Engine.
     */
    void shutdown(engine::Engine& engine) override;

    /**
     * @brief Fixed simulation update: moves the player, spawns enemy waves, kills off-screen bullets and enemies.
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
    /** @brief Turns physics contacts into R-Type collision events. */
    std::unique_ptr<rtype::server::collision::CollisionRouter> _collisionRouter;
    /** @brief Destroys player shots and the enemies they hit. */
    std::unique_ptr<gameplay::ShotHitRule> _shotHitRule;
    /** @brief Seconds left before the next enemy spawns. */
    float _enemySpawnTimer = 0.0F;
    /** @brief Number of enemies spawned so far, used to pick the next spawn height. */
    std::size_t _enemiesSpawned = 0;
};

}  // namespace client
