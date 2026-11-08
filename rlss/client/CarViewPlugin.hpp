/**
 * @file CarViewPlugin.hpp
 * @brief Client plugin giving each replicated car its colored parts, placed by the `CarView` Wren script.
 */

#pragma once

#include <optional>
#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "events/EventBus.hpp"

namespace rlss::client {

/**
 * @brief Builds the look of the cars sent by the server.
 *
 * When a car appears (`ecs::net::ReplicatedEntitySpawned` of kind `KIND_CAR`), spawns its parts, textured with
 * `GameAssets` (body tinted with the player color, untinted glass and trims, wheels, headlight glow, boost flame, boost
 * gauge, white flip reset flash) and attaches the `CarView` script,
 * which sizes and places them every frame from the car `Transform` (interpolated by `ClientReplicationPlugin`). When it
 * disappears, removes the parts and the script.
 */
class CarViewPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Gets plugin name.
     * @return String view "CarViewPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "CarViewPlugin"; }

    /**
     * @brief Declares the dependencies on the assets, script and client replication plugins.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Registers the Wren bindings and subscribes to the replicated entity events.
     * @param engine Host engine.
     * @throws std::runtime_error If Wren refuses the bindings.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Unsubscribes from the events.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

  private:
    /** @brief Subscription to `ReplicatedEntitySpawned`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _spawned;
    /** @brief Subscription to `ReplicatedEntityDespawned`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _despawned;
};

}  // namespace rlss::client
