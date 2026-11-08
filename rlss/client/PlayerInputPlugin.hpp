/**
 * @file PlayerInputPlugin.hpp
 * @brief Client plugin sending the local player's controls to the server.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "events/EventBus.hpp"

namespace ecs::time {
struct Time;
}

namespace rlss::client {

/**
 * @brief Creates the local player's `input::InputState` (sampled by `input::InputPlugin`) and publishes it as an
 * `ecs::net::LocalInput` every fixed step while the player is in a match (between `MatchJoined` and `MatchLeft`);
 * `ClientReplicationPlugin` sends it to the server.
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

    /**
     * @brief Unsubscribes from the match events.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

  private:
    /** @brief Sequence of the last input sent; keeps growing from one match to the next. */
    std::uint32_t _sequence = 0;
    /** @brief True while the player is in a match. */
    bool _inMatch = false;
    /** @brief Subscription to `MatchJoined`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _joined;
    /** @brief Subscription to `MatchLeft`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _left;
};

}  // namespace rlss::client
