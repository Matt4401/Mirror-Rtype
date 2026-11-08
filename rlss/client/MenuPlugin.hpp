/**
 * @file MenuPlugin.hpp
 * @brief Client plugin for the menus: main menu, list of the matches being played, end of match screen.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ServerLink.hpp"
#include "UiCanvas.hpp"
#include "components/Renderable.hpp"
#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "events/EventBus.hpp"
#include "protocol/Protocol.hpp"

namespace ecs::time {
struct Time;
}

namespace rlss::client {

/** @brief What the player sees. */
enum class Scene : std::uint8_t {
    /** @brief Name entry, first thing shown once connected: the player types the name shown above their car. */
    Name,
    /** @brief Main menu: new match, join a match. */
    Menu,
    /** @brief List of the matches being played. */
    Browser,
    /** @brief Waiting for the server to answer a create / join request. */
    Joining,
    /** @brief In a match: no menu, the scoreboard shows the score. */
    Playing,
    /** @brief The match is over: final score, back to the menu. */
    Ended,
};

/**
 * @brief Runs the menus, drawn in screen space over the arena with flat rectangles and the block font (`UiCanvas`),
 * and clicked with the mouse.
 *
 * - Name: the player types their name (letters are made upper-case, only `protocol::isNameCharacter` ones are kept, at
 *   most `protocol::MAX_NAME_LENGTH`); Enter or "VALIDER" sends it (`SetName`) and opens the menu.
 * - Menu: "NOUVELLE PARTIE" sends `CreateMatch`, "REJOINDRE" opens the list of matches, the name in the corner opens
 *   the name entry again.
 * - Browser: the matches of the last `MatchList` (teams, score, clock), updated as the server sends new lists; a click
 *   on a match that is not full sends `JoinMatch`; the mouse wheel scrolls a long list.
 * - Joining: until the `JoinResult`. Joined, the player is in the match: `MatchJoined` is published and the
 *   `ServerLink` lets the snapshots of this match through. Refused, the previous screen comes back with the reason.
 * - Playing: nothing drawn. When a `MatchStatus` says the match is over, the end screen shows the final score and
 *   whether the player's team (from `Welcome`) won.
 * - Ended: "RETOUR AU MENU" sends `LeaveMatch`, removes the entities of the match and publishes `MatchLeft`.
 */
class MenuPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Creates the plugin.
     * @param link Connection to the server, told which match's snapshots to let through.
     */
    explicit MenuPlugin(std::shared_ptr<ServerLink> link);

    /**
     * @brief Gets plugin name.
     * @return String view "MenuPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "MenuPlugin"; }

    /**
     * @brief Declares the dependencies: the network and the replication, and the camera (and its script), which must
     * be final before the menus are placed on screen.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Subscribes to the received packets and shows the main menu.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Handles the mouse, rebuilds the screen if it changed, and places it through the camera.
     * @param engine Host engine.
     * @param time Frame time.
     */
    void update(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Unsubscribes from the packets and removes the menus.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

    /**
     * @brief Current screen.
     * @return The scene.
     */
    [[nodiscard]] Scene scene() const noexcept { return _scene; }

  private:
    /** @brief What a button does. */
    enum class Action : std::uint8_t {
        /** @brief Sends `CreateMatch`. */
        Create,
        /** @brief Opens the list of matches. */
        Browse,
        /** @brief Back to the main menu (from the list). */
        Back,
        /** @brief Sends `JoinMatch` for the button's match. */
        Join,
        /** @brief Leaves the match that ended. */
        Leave,
        /** @brief Sends the typed name and opens the menu. */
        ConfirmName,
        /** @brief Opens the name entry. */
        Rename,
    };

    /** @brief A clickable rectangle. */
    struct Button {
        /** @brief Place on screen. */
        ScreenRect rect;
        /** @brief Background element, whose color shows the hover. */
        UiHandle background = 0;
        /** @brief What a click does. */
        Action action = Action::Create;
        /** @brief Match to join, for `Action::Join`. */
        std::uint16_t matchId = 0;
    };

    /**
     * @brief Reacts to a message from the server.
     * @param engine Host engine.
     * @param message Decoded message.
     */
    void onMessage(engine::Engine& engine, const protocol::Message& message);
    /**
     * @brief Handles the answer to a create / join request.
     * @param engine Host engine.
     * @param result Answer.
     */
    void onJoinResult(engine::Engine& engine, const protocol::JoinResult& result);
    /**
     * @brief Runs a button's action.
     * @param engine Host engine.
     * @param button Clicked button.
     */
    void onClick(engine::Engine& engine, const Button& button);
    /**
     * @brief Sends a request and waits for its answer.
     * @param engine Host engine.
     * @param request Encoded `CreateMatch` / `JoinMatch`.
     */
    void request(engine::Engine& engine, const protocol::Bytes& request);
    /**
     * @brief Leaves the match: tells the server, removes the match's entities, publishes `MatchLeft`.
     * @param engine Host engine.
     */
    void leaveMatch(engine::Engine& engine);
    /**
     * @brief Switches to another screen.
     * @param scene New screen.
     */
    void show(Scene scene);
    /**
     * @brief Shows a short message under the menus.
     * @param text Message.
     */
    void notify(std::string text);

    /**
     * @brief Replaces the drawn screen by the current one.
     * @param engine Host engine.
     */
    void rebuild(engine::Engine& engine);
    /**
     * @brief Reads the keyboard into the typed name; Enter confirms it.
     * @param engine Host engine.
     * @param dt Frame time, in seconds (Backspace repeats while held).
     */
    void type(engine::Engine& engine, float dt);
    /**
     * @brief Sends the typed name if it is valid, and opens the menu; shows why otherwise.
     * @param engine Host engine.
     */
    void confirmName(engine::Engine& engine);
    /** @brief Draws the name entry. */
    void buildName(float width, float height);
    /** @brief Draws the main menu. */
    void buildMenu(float width, float height);
    /** @brief Draws the list of matches. */
    void buildBrowser(float width, float height);
    /** @brief Draws one match of the list. */
    void buildMatchRow(const protocol::MatchInfo& match, const ScreenRect& row);
    /** @brief Draws the waiting screen. */
    void buildJoining(float width, float height);
    /** @brief Draws the end of match screen. */
    void buildEnded(float width, float height);
    /** @brief Draws the dark veil over the arena. */
    void buildOverlay(float width, float height);
    /** @brief Draws the game title, centered at `capTop`, with its two team-colored bars. */
    void buildTitle(float middle, float capTop, float capHeight);
    /** @brief Draws a button with a centered label and registers it. */
    void addButton(const ScreenRect& rect, std::string_view label, Action action, std::uint16_t matchId = 0);
    /** @brief Draws the current message, if any, centered at `top`. */
    void buildNotice(float width, float top);

    /**
     * @brief Tints the button under the mouse.
     * @param engine Host engine.
     * @param mouseX Mouse, in pixels.
     * @param mouseY Mouse, in pixels.
     */
    void hover(engine::Engine& engine, float mouseX, float mouseY);

    /** @brief Connection to the server. */
    std::shared_ptr<ServerLink> _link;
    /** @brief Screen-space elements of the current screen. */
    UiCanvas _canvas;
    /** @brief Font of the menus and the service they are drawn with. */
    Typeface _typeface;
    /** @brief Soft band texture, behind the title. */
    ecs::TextureId _band = ecs::NO_TEXTURE;
    /** @brief Buttons of the current screen. */
    std::vector<Button> _buttons;
    /** @brief Current screen. */
    Scene _scene = Scene::Name;
    /** @brief Name sent to the server, empty until the first one. */
    std::string _name;
    /** @brief Name being typed on the name entry. */
    std::string _typed;
    /** @brief Blinking cursor of the name entry, none on the other screens. */
    std::optional<UiHandle> _cursor;
    /** @brief Time on the name entry, for the cursor blink. */
    float _blink = 0.0F;
    /** @brief Seconds before Backspace erases again while held; negative when it is not held. */
    float _eraseDelay = -1.0F;
    /** @brief Enter state of the previous frame, to confirm on the press only. */
    bool _enterWasDown = false;
    /** @brief Screen to go back to if the server refuses a request. */
    Scene _requestedFrom = Scene::Menu;
    /** @brief True when the drawn screen no longer matches the state. */
    bool _dirty = true;
    /** @brief Last match list received. */
    protocol::MatchList _matches;
    /** @brief First match shown in the list (scrolling). */
    std::size_t _scroll = 0;
    /** @brief Message shown under the menus, if any. */
    std::string _notice;
    /** @brief Seconds before the message disappears. */
    float _noticeTime = 0.0F;
    /** @brief Seconds left to wait for a `JoinResult`. */
    float _waitTime = 0.0F;
    /** @brief Team of the local player in the match, from `Welcome`. */
    std::optional<protocol::Team> _team;
    /** @brief Final score, once the match is over. */
    protocol::MatchStatus _final;
    /** @brief Mouse button state of the previous frame, to click on the press only. */
    bool _mouseWasDown = false;
    /** @brief Subscription to `PacketReceived`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _packets;
};

}  // namespace rlss::client
