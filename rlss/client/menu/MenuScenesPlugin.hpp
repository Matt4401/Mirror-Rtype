/**
 * @file MenuScenesPlugin.hpp
 * @brief The RLSS menus on the engine scene system: name, menu, match browser, match, end of match, settings.
 */

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ServerLink.hpp"
#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "events/EventBus.hpp"
#include "protocol/Protocol.hpp"

namespace ecs::time {
struct Time;
}

namespace rlss::client {

/** @brief Seconds a notice stays on screen. */
inline constexpr float NOTICE_DURATION = 3.0F;

/** @brief Registry resource: what the menus show, kept across scenes. */
struct MenuState {
    /** @brief Name of the player, once confirmed. */
    std::string name;
    /** @brief Last list of matches. */
    protocol::MatchList matches;
    /** @brief Team of the player in the current match, once the server said it. */
    std::optional<protocol::Team> team;
    /** @brief Final status of the last match. */
    protocol::MatchStatus final;
    /** @brief Message shown at the bottom of the menus (refused join...), or empty. */
    std::string notice;
    /** @brief Seconds left before the notice disappears. */
    float noticeTime = 0.0F;
    /** @brief A create or join request waits for its answer: no other one is sent meanwhile. */
    bool requestPending = false;
    /** @brief Answers still to come for requests the player cancelled; they are dropped. */
    int cancelledRequests = 0;
    /** @brief In a match since its `JoinResult`, even before `MatchScene` is entered. */
    bool inMatch = false;
    /** @brief The end of match screen is open or about to be. */
    bool endedShown = false;

    /**
     * @brief Shows a message for `NOTICE_DURATION` seconds.
     * @param text Message.
     */
    void notify(std::string text) {
        notice = std::move(text);
        noticeTime = NOTICE_DURATION;
    }
};

/** @brief A new list of matches arrived (`MenuState::matches`). */
struct MatchListUpdated {};

/**
 * @brief Registers the RLSS scenes and runs what spans them: the server messages and the match flow.
 *
 * - Server messages: `MatchList` updates the menus, `JoinResult` enters a match (or pops the waiting screen with the
 *   reason), `Welcome` gives the team, the `Ended` phase of `MatchStatus` opens the end of match screen.
 * - Entering a match sets `ServerLink::setMatch` and publishes `MatchJoined`; leaving it sends `LeaveMatch`, resets
 *   the client replication and publishes `MatchLeft`. The HUD, camera, input and sound plugins only follow these
 *   events.
 * - The player name and the volumes are saved settings: a saved name skips the name screen.
 */
class MenuScenesPlugin final : public engine::IPlugin {
  public:
    /** @brief Longest wait for a `JoinResult`. */
    static constexpr float REQUEST_TIMEOUT = 5.0F;

    /**
     * @brief Creates the plugin.
     * @param link Link to the server, told which match the player is in.
     */
    explicit MenuScenesPlugin(std::shared_ptr<ServerLink> link) : _link(std::move(link)) {}

    /**
     * @brief Gets plugin name.
     * @return String view "MenuScenesPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "MenuScenesPlugin"; }

    /**
     * @brief Declares the plugins the menus use.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Declares the settings, registers the scenes, and opens the name screen or the menu.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Expires the notice.
     * @param engine Host engine.
     * @param time Frame time.
     */
    void update(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Unsubscribes from the network.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

    /**
     * @brief Sends a create or join request and shows the waiting screen; does nothing while one is pending.
     * @param engine Host engine.
     * @param request Encoded `CreateMatch` or `JoinMatch`.
     */
    static void request(engine::Engine& engine, const protocol::Bytes& request);

    /**
     * @brief Confirms the name typed by the player: sends it, saves it, opens the menu.
     * @param engine Host engine.
     * @param typed Name typed.
     * @return `false` (with a notice) if the name is not valid.
     */
    static bool confirmName(engine::Engine& engine, std::string typed);

    /**
     * @brief Leaves the match and goes back to the menu.
     * @param engine Host engine.
     */
    void leaveMatch(engine::Engine& engine);

    /**
     * @brief Gives up waiting for the pending request: its answer will be dropped, and a match it joined is left.
     * @param engine Host engine.
     */
    static void cancelRequest(engine::Engine& engine);

  private:
    /**
     * @brief Handles a message of the server.
     * @param engine Host engine.
     * @param message The message.
     */
    void onMessage(engine::Engine& engine, const protocol::Message& message);

    /**
     * @brief Handles the answer to a create or join request.
     * @param engine Host engine.
     * @param result The answer.
     */
    void onJoinResult(engine::Engine& engine, const protocol::JoinResult& result);

    /** @brief Link to the server. */
    std::shared_ptr<ServerLink> _link;
    /** @brief Subscription to the packets. */
    std::optional<ecs::EventBus::EventToken> _packets;
};

}  // namespace rlss::client
