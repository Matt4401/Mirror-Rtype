/**
 * @file RTypeGamePlugin.hpp
 * @brief Game logic plugin configuring R-Type demo scene, inputs, and gameplay systems.
 */

#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "components/Renderable.hpp"
#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "events/EventBus.hpp"
#include "looks/BossLooks.hpp"
#include "net/ReplicationEvents.hpp"
#include "render/texture/TextureStore.hpp"

namespace client {

/**
 * @brief What the R-Type client shows: textures, input bindings, starfield and the sprites of replicated entities.
 *
 * The game itself runs on the server: ships, enemies and bullets come from its snapshots through the network plugins.
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
     * @brief Declares dependencies on RenderPlugin, InputPlugin, AnimationPlugin.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Loads textures, configures action bindings, and spawns the starfield, the camera and the local input.
     * @param engine Reference to host Engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Makes the ships that just respawned blink.
     * @param engine Reference to host Engine.
     * @param time Frame time.
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

    /**
     * @brief Reads `client/config/bosses.toml` and loads the textures it names.
     *
     * A missing file keeps every boss a box; a texture that does not load turns its look back into a box. Every problem
     * is logged, none stops the client.
     *
     * @param textures Texture store of the render plugin.
     */
    void loadBossLooks(render::TextureStore& textures);

    /**
     * @brief Draws a boss or a boss part with its configured look, or as a box without one.
     * @param registry Client registry.
     * @param entity Replicated entity.
     * @param look Configured look, or null.
     * @param box Size of the box without a configured size, in pixels: width then height.
     * @param color Color of the box.
     * @param layer Draw layer.
     */
    void applyLook(ecs::Registry& registry, ecs::Entity entity, const looks::Look* look, std::pair<float, float> box,
                   ecs::Rgba color, int layer) const;

    /** @brief Looks of the boss variants and of the boss parts. */
    looks::BossLooks _bossLooks;
    /** @brief Textures of the boss looks, by path. */
    std::map<std::string, ecs::TextureId, std::less<>> _lookTextures;

    /** @brief Seconds since the plugin started, the clock of the blinks. */
    float _elapsed = 0.0F;
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
};

}  // namespace client
