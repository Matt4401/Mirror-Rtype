/**
 * @file RTypeGamePlugin.hpp
 * @brief Game logic plugin configuring R-Type demo scene, inputs, and gameplay systems.
 */

#pragma once

#include <optional>
#include <string_view>

#include "components/Renderable.hpp"
#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/CommandBuffer.hpp"
#include "events/EventBus.hpp"
#include "net/ReplicationEvents.hpp"

namespace client {

/**
 * @brief What the R-Type client shows: textures, input bindings, starfield and the sprites of replicated entities.
 *
 * The game itself runs on the server: ships come from its snapshots through the network plugins.
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
     * @brief Loads textures, configures action bindings, and spawns the starfield, the camera and the local input.
     * @param engine Reference to host Engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Fixed simulation update: kills off-screen bullets.
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

    /**
     * @brief Unsubscribes from the replication events.
     * @param engine Reference to host Engine.
     */
    void shutdown(engine::Engine& engine) override;

  private:
    /**
     * @brief Gives a new replicated entity the sprite of its kind; a kind this client does not know stays invisible.
     * @param engine Reference to host Engine.
     * @param event The new entity.
     */
    void onEntitySpawned(engine::Engine& engine, const ecs::net::ReplicatedEntitySpawned& event) const;

    /** @brief Subscription to `ReplicatedEntitySpawned`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _spawned;
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
