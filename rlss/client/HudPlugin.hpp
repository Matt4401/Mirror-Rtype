/**
 * @file ScoreboardPlugin.hpp
 * @brief Client plugin showing the score, the match clock, the kickoff countdown and "GOAL", drawn with small squares.
 */

#pragma once

#include <optional>
#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "events/EventBus.hpp"
#include "game/Components.hpp"

namespace rlss::client {

/**
 * @brief Shows the match status sent by the server, until the engine can render text.
 *
 * Spawns the scoreboard entity (`rlss::game::MatchState` + the `ScoreboardView` script) and a grid of squares for each
 * display: blue score (top, left of center), orange score (top, right of center), match clock (top, center), kickoff
 * countdown and "GOAL" banner (middle). Each `MatchStatus` message of the joined match updates the `MatchState`; the
 * script lights the squares. Between `MatchLeft` and `MatchJoined` (the menus), nothing is shown.
 */
class ScoreboardPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Gets plugin name.
     * @return String view "ScoreboardPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "ScoreboardPlugin"; }

    /**
     * @brief Declares the dependencies on the car view (which registers the Wren bindings) and the network.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Spawns the scoreboard and subscribes to the received packets.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Unsubscribes from the received packets.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

  private:
    /**
     * @brief Replaces the state shown.
     * @param engine Host engine.
     * @param state New state.
     */
    void setState(engine::Engine& engine, const game::MatchState& state) const;

    /** @brief Entity holding the `MatchState` and the script. */
    ecs::Entity _scoreboard = 0;
    /** @brief True between `MatchJoined` and `MatchLeft`: the statuses received are shown. */
    bool _inMatch = false;
    /** @brief Subscription to `PacketReceived`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _packets;
    /** @brief Subscription to `MatchJoined`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _joined;
    /** @brief Subscription to `MatchLeft`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _left;
};

}  // namespace rlss::client
