/**
 * @file BallViewPlugin.hpp
 * @brief Client plugin drawing the ball, placed by the `BallView` Wren script.
 */

#pragma once

#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "events/EventBus.hpp"

namespace rlss::client {

/**
 * @brief Builds the look of the ball sent by the server.
 *
 * When the ball appears (`ecs::net::ReplicatedEntitySpawned` of kind `KIND_BALL`), spawns its pieces, each tagged
 * "ball_part" with a `BallPart`: the panels (a texture that turns with the ball), its lighting (a texture that does
 * not turn, so the ball keeps looking lit from above), and one soft colored halo per shot color (red, purple, gold),
 * shown while the ball glows after that shot. Attaches the `BallView` script, which places them every frame from the
 * ball `Transform`. When it disappears, removes them.
 */
class BallViewPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Gets plugin name.
     * @return String view "BallViewPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "BallViewPlugin"; }

    /**
     * @brief Declares the dependencies on the car view (which registers the Wren bindings) and the replication.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Subscribes to the replicated entity events.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Unsubscribes from the events.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

  private:
    /** @brief Pieces of each ball on screen. */
    std::unordered_map<ecs::Entity, std::vector<ecs::Entity>> _pieces;
    /** @brief Subscription to `ReplicatedEntitySpawned`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _spawned;
    /** @brief Subscription to `ReplicatedEntityDespawned`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _despawned;
};

}  // namespace rlss::client
