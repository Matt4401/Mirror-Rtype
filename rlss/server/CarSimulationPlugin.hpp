/**
 * @file CarSimulationPlugin.hpp
 * @brief Server plugin hosting the car simulation: arena, Wren bindings, player inputs.
 */

#pragma once

#include <optional>
#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "events/EventBus.hpp"

namespace rlss::server {

/**
 * @brief Prepares the world the `Car` Wren scripts simulate, and feeds them the player inputs.
 *
 * - Exposes the RLSS components to Wren (`rlss::game::installScriptBindings`).
 * - Spawns the arena entity (`ecs::Tag` "arena" + `rlss::game::ArenaShape`), read by `Arena.fromWorld()`.
 * - On each `ecs::net::InputApplied` (one input per car and per fixed step, from `ServerReplicationPlugin`), writes
 *   the input into the car's `input::InputState`, which the scripts read with `Input.getAxis` / `Input.isDown`.
 *
 * The physics itself is in `rlss/scripts/Car.wren`.
 */
class CarSimulationPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Gets plugin name.
     * @return String view "CarSimulationPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "CarSimulationPlugin"; }

    /**
     * @brief Declares the dependencies on `scripting::ScriptPlugin` and `ecs::net::ServerReplicationPlugin`.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Registers the Wren bindings, spawns the arena and subscribes to the applied inputs.
     * @param engine Host engine.
     * @throws std::runtime_error If Wren refuses the bindings.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Unsubscribes from the applied inputs.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

  private:
    /** @brief Arena entity. */
    ecs::Entity _arena = 0;
    /** @brief Subscription to `InputApplied`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _inputs;
};

}  // namespace rlss::server
