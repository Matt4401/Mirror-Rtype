/**
 * @file CameraPlugin.hpp
 * @brief Client plugin owning the camera, driven by the `CameraDirector` Wren script.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "events/EventBus.hpp"

namespace ecs::time {
struct Time;
}

namespace rlss::client {

/**
 * @brief Spawns the camera entity (`ecs::CameraComponent`, tag "main_camera", `rlss::game::CameraRig`, `CameraDirector`
 * script) and keeps its `CameraRig` up to date: the local car (from the server's `Welcome` and the network id map), the
 * ball (from the replication events), the match phase (from `MatchStatus`), the window size and the arena bounds.
 * The framing itself (aim, zoom, easing, bounds) is in `rlss/scripts/CameraDirector.wren`.
 */
class CameraPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Gets plugin name.
     * @return String view "CameraPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "CameraPlugin"; }

    /**
     * @brief Declares the dependencies on the render, network, replication plugins and the car view (bindings).
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Spawns the camera and subscribes to the network and replication events.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Resolves the local car from its network id.
     * @param engine Host engine.
     * @param time Frame time.
     */
    void update(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Unsubscribes from the events.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

  private:
    /** @brief Camera entity. */
    ecs::Entity _camera = 0;
    /** @brief Network id of the local car, once the server said it. */
    std::optional<std::uint32_t> _localCar;
    /** @brief Subscription to `PacketReceived`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _packets;
    /** @brief Subscription to `ReplicatedEntitySpawned`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _spawned;
    /** @brief Subscription to `ReplicatedEntityDespawned`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _despawned;
};

}  // namespace rlss::client
