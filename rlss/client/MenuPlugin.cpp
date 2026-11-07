#include "MenuPlugin.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "Arena.hpp"
#include "AssetsPlugin.hpp"
#include "CameraPlugin.hpp"
#include "ClientEvents.hpp"
#include "ServerLink.hpp"
#include "UiCanvas.hpp"
#include "components/Renderable.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "core/services/IScreenDrawService.hpp"
#include "core/services/IWindowService.hpp"
#include "core/services/InputCodes.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "net/ClientReplicationPlugin.hpp"
#include "net/INetworkManager.hpp"
#include "net/INetworkService.hpp"
#include "net/NetworkEvents.hpp"
#include "net/NetworkPlugin.hpp"
#include "net/ReplicationSystems.hpp"
#include "protocol/Protocol.hpp"
#include "scripting/ScriptPlugin.hpp"

namespace rlss::client {

namespace {

constexpr ecs::Rgba OVERLAY_COLOR{.r = 4, .g = 8, .b = 20, .a = 170};
constexpr ecs::Rgba BUTTON_COLOR{.r = 14, .g = 22, .b = 44, .a = 235};
constexpr ecs::Rgba HOVER_COLOR{.r = 34, .g = 88, .b = 196, .a = 245};
constexpr ecs::Rgba BUTTON_EDGE{.r = 255, .g = 255, .b = 255, .a = 34};
constexpr ecs::Rgba ROW_FULL_COLOR{.r = 26, .g = 28, .b = 38, .a = 215};
constexpr ecs::Rgba TEXT_COLOR{.r = 240, .g = 244, .b = 250, .a = 255};
constexpr ecs::Rgba DIM_COLOR{.r = 150, .g = 162, .b = 185, .a = 255};
constexpr ecs::Rgba NOTICE_COLOR{.r = 255, .g = 130, .b = 110, .a = 255};
constexpr ecs::Rgba GOLD_COLOR{.r = 255, .g = 206, .b = 64, .a = 255};
constexpr ecs::Rgba DEFEAT_COLOR{.r = 230, .g = 96, .b = 96, .a = 255};
constexpr ecs::Rgba PANEL_COLOR{.r = 6, .g = 10, .b = 24, .a = 200};
constexpr ecs::Rgba INVISIBLE{.r = 0, .g = 0, .b = 0, .a = 0};

constexpr float BUTTON_WIDTH = 460.0F;
constexpr float BUTTON_HEIGHT = 72.0F;
constexpr float BUTTON_GAP = 22.0F;
/** @brief Team-colored strip on the left of the buttons, in pixels. */
constexpr float BUTTON_ACCENT = 6.0F;
// Heights of the capital letters, in pixels.
constexpr float LABEL_CAP = 24.0F;
constexpr float SMALL_CAP = 18.0F;
constexpr float TINY_CAP = 14.0F;
constexpr float TITLE_CAP = 120.0F;
constexpr float HEADING_CAP = 40.0F;
constexpr float RESULT_CAP = 90.0F;
constexpr float SCORE_CAP = 120.0F;
constexpr float WAIT_CAP = 34.0F;
constexpr float FIELD_CAP = 36.0F;
constexpr float ROW_MAX_WIDTH = 880.0F;
constexpr float ROW_HEIGHT = 64.0F;
constexpr float ROW_GAP = 12.0F;
constexpr float ROW_PADDING = 24.0F;
constexpr float SIDE_MARGIN = 32.0F;
/** @brief Seconds a message stays. */
constexpr float NOTICE_DURATION = 3.0F;
/** @brief Longest wait for a `JoinResult`. */
constexpr float REQUEST_TIMEOUT = 5.0F;
/** @brief Backspace held: first repeat after this long, then one character every `ERASE_REPEAT`. */
constexpr float ERASE_FIRST_REPEAT = 0.45F;
constexpr float ERASE_REPEAT = 0.05F;
/** @brief The cursor of the name entry blinks: shown, then hidden, this long each. */
constexpr float BLINK_TIME = 0.5F;
constexpr ecs::Rgba FIELD_COLOR{.r = 8, .g = 14, .b = 30, .a = 240};

/** @brief A typed character as a name character (letters made upper-case), or nothing if names cannot hold it. */
std::optional<char> nameCharacter(char32_t typed) {
    if (typed >= U'a' && typed <= U'z') {
        typed = typed - U'a' + U'A';
    }
    if (typed > U'\x7F') {
        return std::nullopt;  // accents and other scripts: names are plain ASCII
    }
    const auto character = static_cast<char>(typed);
    return protocol::isNameCharacter(character) ? std::optional<char>(character) : std::nullopt;
}

ecs::Rgba opaque(const Rgb color) { return ecs::Rgba{.r = color.r, .g = color.g, .b = color.b, .a = 255}; }

/** @brief A clock value in seconds as "M:SS", with a "+" in overtime. */
std::string clockText(const std::uint16_t seconds, const bool overtime) {
    const std::string rest = std::to_string(seconds % 60);
    return (overtime ? "+" : "") + std::to_string(seconds / 60) + ":" + (rest.size() < 2 ? "0" : "") + rest;
}

/** @brief Top of capital letters `capHeight` high centered vertically in a rectangle. */
float centeredTop(const ScreenRect& rect, const float capHeight) { return rect.y + ((rect.height - capHeight) / 2.0F); }

/** @brief Why a request was refused. */
std::string_view refusal(const protocol::JoinStatus status) {
    switch (status) {
        case protocol::JoinStatus::Full:
            return "PARTIE PLEINE";
        case protocol::JoinStatus::NotFound:
            return "PARTIE TERMINEE";
        case protocol::JoinStatus::ServerFull:
            return "SERVEUR PLEIN";
        case protocol::JoinStatus::Joined:
            break;
    }
    return {};
}

}  // namespace

MenuPlugin::MenuPlugin(std::shared_ptr<ServerLink> link) : _link(std::move(link)) {}

void MenuPlugin::build(engine::PluginBuilder& builder) {
    builder.require<ecs::net::NetworkPlugin>();
    builder.require<ecs::net::ClientReplicationPlugin>();
    // The camera script runs in the ScriptPlugin update: the menus are placed after it, with the final camera.
    builder.require<scripting::ScriptPlugin>();
    builder.require<CameraPlugin>();
    builder.require<AssetsPlugin>();
}

void MenuPlugin::init(engine::Engine& engine) {
    const GameAssets& assets = engine.registry().getResource<GameAssets>();
    _typeface = Typeface(engine.service<engine::IScreenDrawService>(), assets.font);
    _band = assets.hudBand;
    _packets =
        engine.eventBus().subscribe<ecs::net::PacketReceived>([this, &engine](const ecs::net::PacketReceived& packet) {
            if (const auto message = protocol::decode(packet.data)) {
                onMessage(engine, *message);
            }
        });
    show(Scene::Name);
}

void MenuPlugin::onMessage(engine::Engine& engine, const protocol::Message& message) {
    if (const auto* list = std::get_if<protocol::MatchList>(&message)) {
        if (*list != _matches) {
            _matches = *list;
            _dirty = _dirty || _scene == Scene::Menu || _scene == Scene::Browser;
        }
    } else if (const auto* result = std::get_if<protocol::JoinResult>(&message)) {
        onJoinResult(engine, *result);
    } else if (const auto* welcome = std::get_if<protocol::Welcome>(&message)) {
        if (_scene == Scene::Playing) {
            _team = welcome->team;
        }
    } else if (const auto* status = std::get_if<protocol::MatchStatus>(&message)) {
        if (_scene == Scene::Playing && status->phase == protocol::MatchPhase::Ended) {
            _final = *status;
            show(Scene::Ended);
        }
    }
}

void MenuPlugin::onJoinResult(engine::Engine& engine, const protocol::JoinResult& result) {
    if (result.status != protocol::JoinStatus::Joined) {
        if (_scene == Scene::Joining) {
            show(_requestedFrom);
            notify(std::string(refusal(result.status)));
        }
        return;
    }
    // Joined, even if the wait timed out: the server put the player in the match, follow it.
    if (_scene == Scene::Playing || _scene == Scene::Ended) {
        return;
    }
    _team.reset();
    _link->setMatch(result.matchId);
    show(Scene::Playing);
    engine.eventBus().publish(MatchJoined{.matchId = result.matchId});
}

void MenuPlugin::update(engine::Engine& engine, const ecs::time::Time& time) {
    if (_noticeTime > 0.0F) {
        _noticeTime -= time.deltaTime;
        if (_noticeTime <= 0.0F) {
            _notice.clear();
            _dirty = true;
        }
    }
    if (_scene == Scene::Joining) {
        _waitTime -= time.deltaTime;
        if (_waitTime <= 0.0F) {
            show(_requestedFrom);
            notify("PAS DE REPONSE DU SERVEUR");
        }
    }

    if (_scene == Scene::Name) {
        type(engine, time.deltaTime);
    }
    const auto& window = engine.service<engine::IWindowService>();
    if (_scene == Scene::Browser) {
        const float wheel = window.mouseWheelDelta();
        if (wheel > 0.0F && _scroll > 0) {
            --_scroll;
            _dirty = true;
        } else if (wheel < 0.0F && _scroll + 1 < _matches.matches.size()) {
            ++_scroll;
            _dirty = true;
        }
    }
    if (_dirty) {
        rebuild(engine);
    }

    const engine::MousePosition mouse = window.mousePosition();
    hover(engine, mouse.x, mouse.y);
    const bool down = window.isMouseButtonDown(engine::MouseButton::Left);
    if (down && !_mouseWasDown) {
        const auto clicked = std::ranges::find_if(
            _buttons, [&mouse](const Button& button) { return button.rect.contains(mouse.x, mouse.y); });
        if (clicked != _buttons.end()) {
            const Button button = *clicked;  // the click may rebuild the screen
            onClick(engine, button);
        }
    }
    _mouseWasDown = down;

    if (_dirty) {
        rebuild(engine);
    }
    if (_cursor) {
        _blink += time.deltaTime;
        const bool shown = static_cast<int>(_blink / BLINK_TIME) % 2 == 0;
        _canvas.setColor(*_cursor, shown ? TEXT_COLOR : INVISIBLE);
    }
    // Drawn every frame, after the name tags and the HUD (plugin order): the menus stay on top.
    _canvas.draw(_typeface);
}

void MenuPlugin::onClick(engine::Engine& engine, const Button& button) {
    switch (button.action) {
        case Action::Create:
            request(engine, protocol::encode(protocol::CreateMatch{}));
            break;
        case Action::Browse:
            _scroll = 0;
            show(Scene::Browser);
            break;
        case Action::Back:
            show(Scene::Menu);
            break;
        case Action::Join:
            request(engine, protocol::encode(protocol::JoinMatch{.matchId = button.matchId}));
            break;
        case Action::Leave:
            leaveMatch(engine);
            break;
        case Action::ConfirmName:
            confirmName(engine);
            break;
        case Action::Rename:
            _typed = _name;
            show(Scene::Name);
            break;
    }
}

void MenuPlugin::type(engine::Engine& engine, const float dt) {
    auto& window = engine.service<engine::IWindowService>();
    bool changed = false;
    for (char32_t typed = window.nextTypedCharacter(); typed != U'\0'; typed = window.nextTypedCharacter()) {
        const auto character = nameCharacter(typed);
        const bool doubleSpace = character == ' ' && (_typed.empty() || _typed.back() == ' ');
        if (character && !doubleSpace && _typed.size() < protocol::MAX_NAME_LENGTH) {
            _typed.push_back(*character);
            changed = true;
        }
    }

    // Backspace erases on the press, then repeats while held.
    if (window.isKeyDown(engine::Key::Backspace)) {
        const bool pressed = _eraseDelay < 0.0F;
        _eraseDelay -= dt;
        if (pressed || _eraseDelay <= 0.0F) {
            _eraseDelay = pressed ? ERASE_FIRST_REPEAT : ERASE_REPEAT;
            if (!_typed.empty()) {
                _typed.pop_back();
                changed = true;
            }
        }
    } else {
        _eraseDelay = -1.0F;
    }
    if (changed) {
        _dirty = true;
    }

    const bool enter = window.isKeyDown(engine::Key::Enter) || window.isKeyDown(engine::Key::KeypadEnter);
    if (enter && !_enterWasDown) {
        confirmName(engine);
    }
    _enterWasDown = enter;
}

void MenuPlugin::confirmName(engine::Engine& engine) {
    while (!_typed.empty() && _typed.back() == ' ') {
        _typed.pop_back();
    }
    if (!protocol::isValidName(_typed)) {
        notify("ENTRE UN PSEUDO");
        return;
    }
    _name = _typed;
    engine.service<ecs::net::INetworkService>().send(
        ecs::net::SERVER_PEER, protocol::encode(protocol::SetName{.name = _name}), ecs::net::Channel::Reliable);
    show(Scene::Menu);
}

void MenuPlugin::request(engine::Engine& engine, const protocol::Bytes& request) {
    engine.service<ecs::net::INetworkService>().send(ecs::net::SERVER_PEER, request, ecs::net::Channel::Reliable);
    _requestedFrom = _scene;
    _waitTime = REQUEST_TIMEOUT;
    show(Scene::Joining);
}

void MenuPlugin::leaveMatch(engine::Engine& engine) {
    engine.service<ecs::net::INetworkService>().send(ecs::net::SERVER_PEER, protocol::encode(protocol::LeaveMatch{}),
                                                     ecs::net::Channel::Reliable);
    _link->setMatch(std::nullopt);
    // Remove every entity of the match (the views remove their parts on `ReplicatedEntityDespawned`), and forget its
    // last tick: the next match counts its steps from 0.
    ecs::Registry& registry = engine.registry();
    ecs::net::despawnMissing(registry, engine.eventBus(), {});
    registry.getResource<ecs::net::SnapshotState>() = ecs::net::SnapshotState{};
    engine.eventBus().publish(MatchLeft{});
    show(Scene::Menu);
}

void MenuPlugin::show(const Scene scene) {
    _scene = scene;
    _dirty = true;
}

void MenuPlugin::notify(std::string text) {
    _notice = std::move(text);
    _noticeTime = NOTICE_DURATION;
    _dirty = true;
}

void MenuPlugin::hover(engine::Engine& /*engine*/, const float mouseX, const float mouseY) {
    for (const Button& button : _buttons) {
        _canvas.setColor(button.background, button.rect.contains(mouseX, mouseY) ? HOVER_COLOR : BUTTON_COLOR);
    }
}

void MenuPlugin::rebuild(engine::Engine& engine) {
    _dirty = false;
    _canvas.clear();
    _buttons.clear();
    _cursor.reset();
    const auto& window = engine.service<engine::IWindowService>();
    const auto width = static_cast<float>(window.width());
    const auto height = static_cast<float>(window.height());
    switch (_scene) {
        case Scene::Name:
            buildName(width, height);
            break;
        case Scene::Menu:
            buildMenu(width, height);
            break;
        case Scene::Browser:
            buildBrowser(width, height);
            break;
        case Scene::Joining:
            buildJoining(width, height);
            break;
        case Scene::Ended:
            buildEnded(width, height);
            break;
        case Scene::Playing:
            break;
    }
}

void MenuPlugin::buildOverlay(const float width, const float height) {
    _canvas.rect(ScreenRect{.x = 0.0F, .y = 0.0F, .width = width, .height = height}, OVERLAY_COLOR);
}

void MenuPlugin::buildTitle(const float middle, const float capTop, const float capHeight) {
    // A soft glow behind, the name, and a bar under it: blue on the left, orange on the right.
    const float glowWidth = capHeight * 9.0F;
    const float glowHeight = capHeight * 2.4F;
    _canvas.image(_band,
                  ScreenRect{
                      .x = middle - (glowWidth / 2.0F),
                      .y = capTop + (capHeight / 2.0F) - (glowHeight / 2.0F),
                      .width = glowWidth,
                      .height = glowHeight,
                  },
                  ecs::Rgba{.r = 40, .g = 110, .b = 255, .a = 70});
    const float shadow = capHeight * 0.05F;
    _canvas.text(protocol::GAME_TITLE, middle + shadow, capTop + shadow, capHeight,
                 ecs::Rgba{.r = 0, .g = 0, .b = 0, .a = 140});
    _canvas.text(protocol::GAME_TITLE, middle, capTop, capHeight, TEXT_COLOR);
    const float barWidth = _typeface.width(protocol::GAME_TITLE, capHeight) * 0.55F;
    const float barTop = capTop + capHeight + (capHeight * 0.16F);
    const float barHeight = std::max(4.0F, capHeight * 0.06F);
    _canvas.rect(ScreenRect{.x = middle - barWidth, .y = barTop, .width = barWidth - 3.0F, .height = barHeight},
                 opaque(BLUE_TEAM));
    _canvas.rect(ScreenRect{.x = middle + 3.0F, .y = barTop, .width = barWidth - 3.0F, .height = barHeight},
                 opaque(ORANGE_TEAM));
}

void MenuPlugin::addButton(const ScreenRect& rect, const std::string_view label, const Action action,
                           const std::uint16_t matchId) {
    const UiHandle background = _canvas.rect(rect, BUTTON_COLOR);
    _canvas.outline(rect, 1.0F, BUTTON_EDGE);
    _canvas.rect(ScreenRect{.x = rect.x, .y = rect.y, .width = BUTTON_ACCENT, .height = rect.height},
                 opaque(BLUE_TEAM));
    if (!label.empty()) {
        _canvas.text(label, rect.x + (rect.width / 2.0F) + (BUTTON_ACCENT / 2.0F), centeredTop(rect, LABEL_CAP),
                     LABEL_CAP, TEXT_COLOR);
    }
    _buttons.push_back(Button{.rect = rect, .background = background, .action = action, .matchId = matchId});
}

void MenuPlugin::buildNotice(const float width, const float top) {
    if (!_notice.empty()) {
        _canvas.text(_notice, width / 2.0F, top, SMALL_CAP, NOTICE_COLOR);
    }
}

void MenuPlugin::buildName(const float width, const float height) {
    constexpr float TITLE_TOP = 0.13F;
    constexpr float HEADING_TOP = 0.38F;
    constexpr float FIELD_TOP = 0.45F;
    constexpr float HINT_TOP = 0.585F;
    constexpr float BUTTON_TOP = 0.66F;
    constexpr float NOTICE_TOP = 0.8F;
    constexpr float FIELD_PADDING = 22.0F;
    constexpr float CURSOR_WIDTH = 4.0F;
    const float middle = width / 2.0F;
    buildOverlay(width, height);
    buildTitle(middle, height * TITLE_TOP, TITLE_CAP);
    _canvas.text("CHOISIS TON PSEUDO", middle, height * HEADING_TOP, SMALL_CAP, DIM_COLOR);

    // The field fits the longest name; the text is centered, the cursor right after it.
    const float fieldWidth =
        std::min(width - (2.0F * SIDE_MARGIN),
                 _typeface.width(std::string(protocol::MAX_NAME_LENGTH, 'W'), FIELD_CAP) + (2.0F * FIELD_PADDING));
    const ScreenRect field{
        .x = middle - (fieldWidth / 2.0F),
        .y = height * FIELD_TOP,
        .width = fieldWidth,
        .height = FIELD_CAP + (2.0F * FIELD_PADDING),
    };
    _canvas.rect(field, FIELD_COLOR);
    _canvas.outline(field, 2.0F, ecs::Rgba{.r = 60, .g = 120, .b = 255, .a = 200});
    const float textTop = centeredTop(field, FIELD_CAP);
    const float textWidth = _typeface.width(_typed, FIELD_CAP);
    _canvas.text(_typed, middle, textTop, FIELD_CAP, TEXT_COLOR);
    const float cursorX = middle + (textWidth / 2.0F) + (_typed.empty() ? -CURSOR_WIDTH / 2.0F : CURSOR_WIDTH);
    _cursor =
        _canvas.rect(ScreenRect{.x = cursorX, .y = textTop, .width = CURSOR_WIDTH, .height = FIELD_CAP}, TEXT_COLOR);
    _blink = 0.0F;

    _canvas.text(std::to_string(protocol::MAX_NAME_LENGTH) + " CARACTERES MAX : LETTRES, CHIFFRES, ESPACE, - .", middle,
                 height * HINT_TOP, TINY_CAP, DIM_COLOR);
    addButton(
        ScreenRect{
            .x = middle - (BUTTON_WIDTH / 2.0F),
            .y = height * BUTTON_TOP,
            .width = BUTTON_WIDTH,
            .height = BUTTON_HEIGHT,
        },
        "VALIDER", Action::ConfirmName);
    buildNotice(width, height * NOTICE_TOP);
}

void MenuPlugin::buildMenu(const float width, const float height) {
    constexpr float TITLE_TOP = 0.13F;
    constexpr float TAGLINE_TOP = 0.34F;
    constexpr float BUTTONS_TOP = 0.45F;
    constexpr float NOTICE_TOP = 0.76F;
    constexpr float ONLINE_TOP = 0.9F;
    const float middle = width / 2.0F;
    buildOverlay(width, height);
    buildTitle(middle, height * TITLE_TOP, TITLE_CAP);
    _canvas.text("MATCHS DE 3 MINUTES  -  2 CONTRE 2", middle, height * TAGLINE_TOP, SMALL_CAP, DIM_COLOR);
    const float left = middle - (BUTTON_WIDTH / 2.0F);
    const float top = height * BUTTONS_TOP;
    addButton(ScreenRect{.x = left, .y = top, .width = BUTTON_WIDTH, .height = BUTTON_HEIGHT}, "NOUVELLE PARTIE",
              Action::Create);
    addButton(
        ScreenRect{
            .x = left,
            .y = top + BUTTON_HEIGHT + BUTTON_GAP,
            .width = BUTTON_WIDTH,
            .height = BUTTON_HEIGHT,
        },
        "REJOINDRE", Action::Browse);
    buildNotice(width, height * NOTICE_TOP);
    const std::uint16_t online = _matches.online;
    _canvas.text(std::to_string(online) + (online > 1 ? " JOUEURS EN LIGNE" : " JOUEUR EN LIGNE"), middle,
                 height * ONLINE_TOP, SMALL_CAP, DIM_COLOR);

    // The player's name in the corner: a click changes it.
    constexpr float NAME_PADDING = 20.0F;
    constexpr float NAME_HEIGHT = 48.0F;
    const std::string label = "PSEUDO : " + _name;
    addButton(
        ScreenRect{
            .x = SIDE_MARGIN,
            .y = SIDE_MARGIN,
            .width = _typeface.width(label, LABEL_CAP * 0.75F) + (2.0F * NAME_PADDING) + BUTTON_ACCENT,
            .height = NAME_HEIGHT,
        },
        "", Action::Rename);
    _canvas.text(label, SIDE_MARGIN + BUTTON_ACCENT + NAME_PADDING,
                 SIDE_MARGIN + ((NAME_HEIGHT - (LABEL_CAP * 0.75F)) / 2.0F), LABEL_CAP * 0.75F, TEXT_COLOR,
                 Align::Left);
}

void MenuPlugin::buildBrowser(const float width, const float height) {
    constexpr float HEADING_TOP = 0.08F;
    constexpr float LIST_TOP = 0.22F;
    constexpr float LIST_BOTTOM = 0.8F;
    constexpr float HINT_TOP = 0.815F;
    constexpr float NOTICE_TOP = 0.85F;
    constexpr float BUTTONS_TOP = 0.88F;
    constexpr float EMPTY_TOP = 0.45F;
    const float middle = width / 2.0F;
    buildOverlay(width, height);
    _canvas.text("PARTIES EN COURS", middle, height * HEADING_TOP, HEADING_CAP, TEXT_COLOR);

    const std::vector<protocol::MatchInfo>& matches = _matches.matches;
    const float rowWidth = std::min(ROW_MAX_WIDTH, width - (2.0F * SIDE_MARGIN));
    const float listTop = height * LIST_TOP;
    const auto visible = static_cast<std::size_t>(
        std::max(1.0F, ((height * (LIST_BOTTOM - LIST_TOP)) + ROW_GAP) / (ROW_HEIGHT + ROW_GAP)));
    _scroll = std::min(_scroll, matches.size() > visible ? matches.size() - visible : 0);
    if (matches.empty()) {
        _canvas.text("AUCUNE PARTIE EN COURS", middle, height * EMPTY_TOP, LABEL_CAP, DIM_COLOR);
    }
    for (std::size_t i = _scroll; i < matches.size() && i < _scroll + visible; ++i) {
        const float top = listTop + (static_cast<float>(i - _scroll) * (ROW_HEIGHT + ROW_GAP));
        buildMatchRow(matches.at(i),
                      ScreenRect{.x = middle - (rowWidth / 2.0F), .y = top, .width = rowWidth, .height = ROW_HEIGHT});
    }
    if (matches.size() > visible) {
        const std::string hint = std::to_string(_scroll + 1) + "-" +
                                 std::to_string(std::min(matches.size(), _scroll + visible)) + " / " +
                                 std::to_string(matches.size()) + "  MOLETTE POUR DEFILER";
        _canvas.text(hint, middle, height * HINT_TOP, TINY_CAP, DIM_COLOR);
    }
    buildNotice(width, height * NOTICE_TOP);

    const float buttonWidth = std::min(BUTTON_WIDTH, (width - (2.0F * SIDE_MARGIN) - BUTTON_GAP) / 2.0F);
    const float top = height * BUTTONS_TOP;
    addButton(
        ScreenRect{
            .x = middle - (BUTTON_GAP / 2.0F) - buttonWidth,
            .y = top,
            .width = buttonWidth,
            .height = BUTTON_HEIGHT * 0.8F,
        },
        "RETOUR", Action::Back);
    addButton(
        ScreenRect{
            .x = middle + (BUTTON_GAP / 2.0F),
            .y = top,
            .width = buttonWidth,
            .height = BUTTON_HEIGHT * 0.8F,
        },
        "NOUVELLE PARTIE", Action::Create);
}

void MenuPlugin::buildMatchRow(const protocol::MatchInfo& match, const ScreenRect& row) {
    const bool full = match.bluePlayers + match.orangePlayers >= protocol::MAX_PLAYERS_PER_MATCH;
    if (full) {
        _canvas.rect(row, ROW_FULL_COLOR);
    } else {
        addButton(row, "", Action::Join, match.id);
    }
    const ecs::Rgba text = full ? DIM_COLOR : TEXT_COLOR;
    const float top = centeredTop(row, LABEL_CAP);
    _canvas.text("PARTIE " + std::to_string(match.id), row.x + ROW_PADDING, top, LABEL_CAP, text, Align::Left);

    // Score in the team colors: "2 - 1".
    constexpr float SCORE_AT = 0.47F;
    const float scoreX = row.x + (row.width * SCORE_AT);
    const float dash = _typeface.width(" - ", LABEL_CAP) / 2.0F;
    _canvas.text("-", scoreX, top, LABEL_CAP, text);
    _canvas.text(std::to_string(match.blueScore), scoreX - dash, top, LABEL_CAP, opaque(BLUE_TEAM), Align::Right);
    _canvas.text(std::to_string(match.orangeScore), scoreX + dash, top, LABEL_CAP, opaque(ORANGE_TEAM), Align::Left);

    constexpr float CLOCK_AT = 0.68F;
    _canvas.text(clockText(match.clock, match.overtime), row.x + (row.width * CLOCK_AT), top, LABEL_CAP,
                 match.overtime ? GOLD_COLOR : text);
    const std::string players = full ? std::string("PLEIN")
                                     : std::to_string(match.bluePlayers + match.orangePlayers) + "/" +
                                           std::to_string(static_cast<int>(protocol::MAX_PLAYERS_PER_MATCH));
    _canvas.text(players, row.x + row.width - ROW_PADDING, top, LABEL_CAP, text, Align::Right);
}

void MenuPlugin::buildJoining(const float width, const float height) {
    buildOverlay(width, height);
    _canvas.text("CONNEXION...", width / 2.0F, (height - WAIT_CAP) / 2.0F, WAIT_CAP, TEXT_COLOR);
}

void MenuPlugin::buildEnded(const float width, const float height) {
    constexpr float RESULT_TOP = 0.14F;
    constexpr float PANEL_TOP = 0.33F;
    constexpr float SCORE_TOP = 0.38F;
    constexpr float BUTTON_TOP = 0.74F;
    /** @brief Space between the dash and each score, in pixels. */
    constexpr float SCORE_SPACING = 48.0F;
    constexpr float PANEL_WIDTH = 640.0F;
    const float middle = width / 2.0F;
    buildOverlay(width, height);

    std::string result = "MATCH TERMINE";
    ecs::Rgba resultColor = TEXT_COLOR;
    if (_team && _final.blueScore != _final.orangeScore) {
        const bool blueWon = _final.blueScore > _final.orangeScore;
        const bool won = blueWon == (*_team == protocol::Team::Blue);
        result = won ? "VICTOIRE" : "DEFAITE";
        resultColor = won ? GOLD_COLOR : DEFEAT_COLOR;
    }
    const float glowWidth = RESULT_CAP * 10.0F;
    const float glowHeight = RESULT_CAP * 2.2F;
    _canvas.image(_band,
                  ScreenRect{
                      .x = middle - (glowWidth / 2.0F),
                      .y = (height * RESULT_TOP) + (RESULT_CAP / 2.0F) - (glowHeight / 2.0F),
                      .width = glowWidth,
                      .height = glowHeight,
                  },
                  ecs::Rgba{.r = resultColor.r, .g = resultColor.g, .b = resultColor.b, .a = 60});
    _canvas.text(result, middle, height * RESULT_TOP, RESULT_CAP, resultColor);

    // The final score on a panel, each half in its team color.
    const float scoreTop = height * SCORE_TOP;
    const float panelWidth = std::min(PANEL_WIDTH, width - (2.0F * SIDE_MARGIN));
    const float panelTop = height * PANEL_TOP;
    const float panelHeight = ((scoreTop - panelTop) * 2.0F) + SCORE_CAP + (SMALL_CAP * 3.0F);
    _canvas.rect(
        ScreenRect{.x = middle - (panelWidth / 2.0F), .y = panelTop, .width = panelWidth, .height = panelHeight},
        PANEL_COLOR);
    _canvas.rect(
        ScreenRect{.x = middle - (panelWidth / 2.0F), .y = panelTop, .width = panelWidth / 2.0F, .height = 6.0F},
        opaque(BLUE_TEAM));
    _canvas.rect(ScreenRect{.x = middle, .y = panelTop, .width = panelWidth / 2.0F, .height = 6.0F},
                 opaque(ORANGE_TEAM));
    _canvas.text("-", middle, scoreTop, SCORE_CAP, DIM_COLOR);
    const float halfDash = _typeface.width("-", SCORE_CAP) / 2.0F;
    const float blueRight = middle - halfDash - SCORE_SPACING;
    const float orangeLeft = middle + halfDash + SCORE_SPACING;
    _canvas.text(std::to_string(_final.blueScore), blueRight, scoreTop, SCORE_CAP, opaque(BLUE_TEAM), Align::Right);
    _canvas.text(std::to_string(_final.orangeScore), orangeLeft, scoreTop, SCORE_CAP, opaque(ORANGE_TEAM), Align::Left);
    const float labelTop = scoreTop + SCORE_CAP + SMALL_CAP;
    _canvas.text("BLEU", blueRight, labelTop, SMALL_CAP, opaque(BLUE_TEAM), Align::Right);
    _canvas.text("ORANGE", orangeLeft, labelTop, SMALL_CAP, opaque(ORANGE_TEAM), Align::Left);
    if (_final.overtime) {
        _canvas.text("APRES PROLONGATION", middle, panelTop + panelHeight + SMALL_CAP, SMALL_CAP, GOLD_COLOR);
    }

    addButton(
        ScreenRect{
            .x = middle - (BUTTON_WIDTH / 2.0F),
            .y = height * BUTTON_TOP,
            .width = BUTTON_WIDTH,
            .height = BUTTON_HEIGHT,
        },
        "RETOUR AU MENU", Action::Leave);
}

void MenuPlugin::shutdown(engine::Engine& engine) {
    if (_packets) {
        engine.eventBus().unsubscribe<ecs::net::PacketReceived>(*_packets);
        _packets.reset();
    }
    _canvas.clear();
    _buttons.clear();
}

}  // namespace rlss::client
