/**
 * @file PlayerInputPlugin.hpp
 * @brief Client plugin sending the local player's controls to the server.
 */

#pragma once

#include <cstdint>
#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "input/ActionMapping.hpp"

namespace ecs::time {
struct Time;
}

namespace rlss::client {

/**
 * @brief Default controls: steer with A/D, Q/D or the arrows (or the left stick), jump with Space, W, Z or Up (or the
 * bottom face button).
 * @return The mapping, for `input::InputPlugin`.
 */
[[nodiscard]] input::ActionMapping makeControls();

/**
 * @brief Creates the local player's `input::InputState` (sampled by `input::InputPlugin`) and publishes it as an
 * `ecs::net::LocalInput` every fixed step; `ClientReplicationPlugin` sends it to the server.
 */
class PlayerInputPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Gets plugin name.
     * @return String view "PlayerInputPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "PlayerInputPlugin"; }

    /**
     * @brief Declares the dependencies on `input::InputPlugin` and `ecs::net::ClientReplicationPlugin`.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Spawns the local player's input state.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Publishes this step's input.
     * @param engine Host engine.
     * @param time Fixed step timing.
     */
    void fixedUpdate(engine::Engine& engine, const ecs::time::Time& time) override;

  private:
    /** @brief Sequence of the last input sent. */
    std::uint32_t _sequence = 0;
};

}  // namespace rlss::client
