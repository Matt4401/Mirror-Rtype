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
#include "CameraPlugin.hpp"
#include "ClientEvents.hpp"
#include "ServerLink.hpp"
#include "UiCanvas.hpp"
#include "components/CameraComponent.hpp"
#include "components/Renderable.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "core/services/IWindowService.hpp"
#include "core/services/InputCodes.hpp"
#include "ecs/Entity.hpp"
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

/** @brief Above the world and the scoreboard. */
constexpr int LAYER_OVERLAY = 200;
constexpr int LAYER_PANEL = 201;
constexpr int LAYER_TEXT = 202;

constexpr ecs::Rgba OVERLAY_COLOR{.r = 8, .g = 12, .b = 22, .a = 175};
constexpr ecs::Rgba BUTTON_COLOR{.r = 36, .g = 50, .b = 78, .a = 240};
constexpr ecs::Rgba HOVER_COLOR{.r = 60, .g = 98, .b = 168, .a = 250};
constexpr ecs::Rgba ROW_FULL_COLOR{.r = 34, .g = 36, .b = 44, .a = 220};
constexpr ecs::Rgba TEXT_COLOR{.r = 240, .g = 244, .b = 250, .a = 255};
constexpr ecs::Rgba DIM_COLOR{.r = 150, .g = 158, .b = 175, .a = 255};
constexpr ecs::Rgba NOTICE_COLOR{.r = 255, .g = 130, .b = 110, .a = 255};
constexpr ecs::Rgba GOLD_COLOR{.r = 255, .g = 210, .b = 60, .a = 255};
constexpr ecs::Rgba DEFEAT_COLOR{.r = 230, .g = 96, .b = 96, .a = 255};

constexpr float BUTTON_WIDTH = 460.0F;
constexpr float BUTTON_HEIGHT = 72.0F;
constexpr float BUTTON_GAP = 24.0F;
constexpr float LABEL_SQUARE = 4.0F;
constexpr float SMALL_SQUARE = 3.0F;
constexpr float TINY_SQUARE = 2.0F;
constexpr float TITLE_SQUARE = 16.0F;
constexpr float HEADING_SQUARE = 6.0F;
constexpr float RESULT_SQUARE = 12.0F;
constexpr float SCORE_SQUARE = 16.0F;
constexpr float WAIT_SQUARE = 5.0F;
constexpr float ROW_MAX_WIDTH = 880.0F;
constexpr float ROW_HEIGHT = 64.0F;
constexpr float ROW_GAP = 12.0F;
constexpr float ROW_PADDING = 24.0F;
constexpr float SIDE_MARGIN = 32.0F;
/** @brief A glyph is 7 squares high. */
constexpr float GLYPH_HEIGHT = 7.0F;
/** @brief Seconds a message stays. */
constexpr float NOTICE_DURATION = 3.0F;
/** @brief Longest wait for a `JoinResult`. */
constexpr float REQUEST_TIMEOUT = 5.0F;

ecs::Rgba opaque(const Rgb color) { return ecs::Rgba{.r = color.r, .g = color.g, .b = color.b, .a = 255}; }

/** @brief A clock value in seconds as "M:SS", with a "+" in overtime. */
std::string clockText(const std::uint16_t seconds, const bool overtime) {
    const std::string rest = std::to_string(seconds % 60);
    return (overtime ? "+" : "") + std::to_string(seconds / 60) + ":" + (rest.size() < 2 ? "0" : "") + rest;
}

/** @brief Top of a text of `square`-sized glyphs centered vertically in a rectangle. */
float centeredTop(const ScreenRect& rect, const float square) {
    return rect.y + ((rect.height - (GLYPH_HEIGHT * square)) / 2.0F);
}

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
}

void MenuPlugin::init(engine::Engine& engine) {
    _packets =
        engine.eventBus().subscribe<ecs::net::PacketReceived>([this, &engine](const ecs::net::PacketReceived& packet) {
            if (const auto message = protocol::decode(packet.data)) {
                onMessage(engine, *message);
            }
        });
    show(Scene::Menu);
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
    ecs::Registry& registry = engine.registry();
    const ecs::Entity camera = engine.getPlugin<CameraPlugin>().camera();
    auto& cameras = registry.getComponents<ecs::CameraComponent>();
    if (cameras.contains(camera)) {
        _canvas.place(registry, cameras.get(camera));
    }
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
    }
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

void MenuPlugin::hover(engine::Engine& engine, const float mouseX, const float mouseY) {
    auto& renderables = engine.registry().getComponents<ecs::Renderable>();
    for (const Button& button : _buttons) {
        if (renderables.contains(button.background)) {
            renderables.get(button.background).tint = button.rect.contains(mouseX, mouseY) ? HOVER_COLOR : BUTTON_COLOR;
        }
    }
}

void MenuPlugin::rebuild(engine::Engine& engine) {
    _dirty = false;
    ecs::Registry& registry = engine.registry();
    _canvas.clear(registry);
    _buttons.clear();
    const auto& window = engine.service<engine::IWindowService>();
    const auto width = static_cast<float>(window.width());
    const auto height = static_cast<float>(window.height());
    switch (_scene) {
        case Scene::Menu:
            buildMenu(registry, width, height);
            break;
        case Scene::Browser:
            buildBrowser(registry, width, height);
            break;
        case Scene::Joining:
            buildJoining(registry, width, height);
            break;
        case Scene::Ended:
            buildEnded(registry, width, height);
            break;
        case Scene::Playing:
            break;
    }
}

void MenuPlugin::buildOverlay(ecs::Registry& registry, const float width, const float height) {
    _canvas.rect(registry, ScreenRect{.x = 0.0F, .y = 0.0F, .width = width, .height = height}, OVERLAY_COLOR,
                 LAYER_OVERLAY);
}

void MenuPlugin::addButton(ecs::Registry& registry, const ScreenRect& rect, const std::string_view label,
                           const Action action, const std::uint16_t matchId) {
    const ecs::Entity background = _canvas.rect(registry, rect, BUTTON_COLOR, LAYER_PANEL);
    if (!label.empty()) {
        _canvas.text(registry, label, rect.x + (rect.width / 2.0F), centeredTop(rect, LABEL_SQUARE), LABEL_SQUARE,
                     TEXT_COLOR, LAYER_TEXT);
    }
    _buttons.push_back(Button{.rect = rect, .background = background, .action = action, .matchId = matchId});
}

void MenuPlugin::buildNotice(ecs::Registry& registry, const float width, const float top) {
    if (!_notice.empty()) {
        _canvas.text(registry, _notice, width / 2.0F, top, SMALL_SQUARE, NOTICE_COLOR, LAYER_TEXT);
    }
}

void MenuPlugin::buildMenu(ecs::Registry& registry, const float width, const float height) {
    constexpr float TITLE_TOP = 0.13F;
    constexpr float TAGLINE_TOP = 0.33F;
    constexpr float BUTTONS_TOP = 0.45F;
    constexpr float NOTICE_TOP = 0.76F;
    constexpr float ONLINE_TOP = 0.9F;
    const float middle = width / 2.0F;
    buildOverlay(registry, width, height);
    _canvas.text(registry, protocol::GAME_TITLE, middle, height * TITLE_TOP, TITLE_SQUARE, TEXT_COLOR, LAYER_TEXT);
    _canvas.text(registry, "MATCHS DE 3 MINUTES - 2 CONTRE 2", middle, height * TAGLINE_TOP, SMALL_SQUARE, DIM_COLOR,
                 LAYER_TEXT);
    const float left = middle - (BUTTON_WIDTH / 2.0F);
    const float top = height * BUTTONS_TOP;
    addButton(registry, ScreenRect{.x = left, .y = top, .width = BUTTON_WIDTH, .height = BUTTON_HEIGHT},
              "NOUVELLE PARTIE", Action::Create);
    addButton(registry,
              ScreenRect{
                  .x = left,
                  .y = top + BUTTON_HEIGHT + BUTTON_GAP,
                  .width = BUTTON_WIDTH,
                  .height = BUTTON_HEIGHT,
              },
              "REJOINDRE", Action::Browse);
    buildNotice(registry, width, height * NOTICE_TOP);
    const std::uint16_t online = _matches.online;
    _canvas.text(registry, std::to_string(online) + (online > 1 ? " JOUEURS EN LIGNE" : " JOUEUR EN LIGNE"), middle,
                 height * ONLINE_TOP, SMALL_SQUARE, DIM_COLOR, LAYER_TEXT);
}

void MenuPlugin::buildBrowser(ecs::Registry& registry, const float width, const float height) {
    constexpr float HEADING_TOP = 0.07F;
    constexpr float LIST_TOP = 0.22F;
    constexpr float LIST_BOTTOM = 0.8F;
    constexpr float HINT_TOP = 0.815F;
    constexpr float NOTICE_TOP = 0.85F;
    constexpr float BUTTONS_TOP = 0.88F;
    constexpr float EMPTY_TOP = 0.45F;
    const float middle = width / 2.0F;
    buildOverlay(registry, width, height);
    _canvas.text(registry, "PARTIES EN COURS", middle, height * HEADING_TOP, HEADING_SQUARE, TEXT_COLOR, LAYER_TEXT);

    const std::vector<protocol::MatchInfo>& matches = _matches.matches;
    const float rowWidth = std::min(ROW_MAX_WIDTH, width - (2.0F * SIDE_MARGIN));
    const float listTop = height * LIST_TOP;
    const auto visible = static_cast<std::size_t>(
        std::max(1.0F, ((height * (LIST_BOTTOM - LIST_TOP)) + ROW_GAP) / (ROW_HEIGHT + ROW_GAP)));
    _scroll = std::min(_scroll, matches.size() > visible ? matches.size() - visible : 0);
    if (matches.empty()) {
        _canvas.text(registry, "AUCUNE PARTIE EN COURS", middle, height * EMPTY_TOP, LABEL_SQUARE, DIM_COLOR,
                     LAYER_TEXT);
    }
    for (std::size_t i = _scroll; i < matches.size() && i < _scroll + visible; ++i) {
        const float top = listTop + (static_cast<float>(i - _scroll) * (ROW_HEIGHT + ROW_GAP));
        buildMatchRow(registry, matches.at(i),
                      ScreenRect{.x = middle - (rowWidth / 2.0F), .y = top, .width = rowWidth, .height = ROW_HEIGHT});
    }
    if (matches.size() > visible) {
        const std::string hint = std::to_string(_scroll + 1) + "-" +
                                 std::to_string(std::min(matches.size(), _scroll + visible)) + " / " +
                                 std::to_string(matches.size()) + "  MOLETTE POUR DEFILER";
        _canvas.text(registry, hint, middle, height * HINT_TOP, TINY_SQUARE, DIM_COLOR, LAYER_TEXT);
    }
    buildNotice(registry, width, height * NOTICE_TOP);

    const float buttonWidth = std::min(BUTTON_WIDTH, (width - (2.0F * SIDE_MARGIN) - BUTTON_GAP) / 2.0F);
    const float top = height * BUTTONS_TOP;
    addButton(registry,
              ScreenRect{
                  .x = middle - (BUTTON_GAP / 2.0F) - buttonWidth,
                  .y = top,
                  .width = buttonWidth,
                  .height = BUTTON_HEIGHT * 0.8F,
              },
              "RETOUR", Action::Back);
    addButton(registry,
              ScreenRect{
                  .x = middle + (BUTTON_GAP / 2.0F),
                  .y = top,
                  .width = buttonWidth,
                  .height = BUTTON_HEIGHT * 0.8F,
              },
              "NOUVELLE PARTIE", Action::Create);
}

void MenuPlugin::buildMatchRow(ecs::Registry& registry, const protocol::MatchInfo& match, const ScreenRect& row) {
    const bool full = match.bluePlayers + match.orangePlayers >= protocol::MAX_PLAYERS_PER_MATCH;
    if (full) {
        _canvas.rect(registry, row, ROW_FULL_COLOR, LAYER_PANEL);
    } else {
        addButton(registry, row, "", Action::Join, match.id);
    }
    const ecs::Rgba text = full ? DIM_COLOR : TEXT_COLOR;
    const float top = centeredTop(row, LABEL_SQUARE);
    _canvas.text(registry, "PARTIE " + std::to_string(match.id), row.x + ROW_PADDING, top, LABEL_SQUARE, text,
                 LAYER_TEXT, Align::Left);

    // Score in the team colors: "2 - 1".
    constexpr float SCORE_AT = 0.47F;
    const float scoreX = row.x + (row.width * SCORE_AT);
    const float dash = UiCanvas::textWidth("-", LABEL_SQUARE);
    _canvas.text(registry, "-", scoreX, top, LABEL_SQUARE, text, LAYER_TEXT);
    _canvas.text(registry, std::to_string(match.blueScore), scoreX - dash, top, LABEL_SQUARE, opaque(BLUE_TEAM),
                 LAYER_TEXT, Align::Right);
    _canvas.text(registry, std::to_string(match.orangeScore), scoreX + dash, top, LABEL_SQUARE, opaque(ORANGE_TEAM),
                 LAYER_TEXT, Align::Left);

    constexpr float CLOCK_AT = 0.68F;
    _canvas.text(registry, clockText(match.clock, match.overtime), row.x + (row.width * CLOCK_AT), top, LABEL_SQUARE,
                 match.overtime ? GOLD_COLOR : text, LAYER_TEXT);
    const std::string players = full ? std::string("PLEIN")
                                     : std::to_string(match.bluePlayers + match.orangePlayers) + "/" +
                                           std::to_string(static_cast<int>(protocol::MAX_PLAYERS_PER_MATCH));
    _canvas.text(registry, players, row.x + row.width - ROW_PADDING, top, LABEL_SQUARE, text, LAYER_TEXT, Align::Right);
}

void MenuPlugin::buildJoining(ecs::Registry& registry, const float width, const float height) {
    buildOverlay(registry, width, height);
    _canvas.text(registry, "CONNEXION...", width / 2.0F, (height - (GLYPH_HEIGHT * WAIT_SQUARE)) / 2.0F, WAIT_SQUARE,
                 TEXT_COLOR, LAYER_TEXT);
}

void MenuPlugin::buildEnded(ecs::Registry& registry, const float width, const float height) {
    constexpr float RESULT_TOP = 0.14F;
    constexpr float SCORE_TOP = 0.38F;
    constexpr float BUTTON_TOP = 0.74F;
    /** @brief Space between the dash and each score, in pixels. */
    constexpr float SCORE_SPACING = 40.0F;
    const float middle = width / 2.0F;
    buildOverlay(registry, width, height);

    std::string result = "MATCH TERMINE";
    ecs::Rgba resultColor = TEXT_COLOR;
    if (_team && _final.blueScore != _final.orangeScore) {
        const bool blueWon = _final.blueScore > _final.orangeScore;
        const bool won = blueWon == (*_team == protocol::Team::Blue);
        result = won ? "VICTOIRE" : "DEFAITE";
        resultColor = won ? GOLD_COLOR : DEFEAT_COLOR;
    }
    _canvas.text(registry, result, middle, height * RESULT_TOP, RESULT_SQUARE, resultColor, LAYER_TEXT);

    const float scoreTop = height * SCORE_TOP;
    _canvas.text(registry, "-", middle, scoreTop, SCORE_SQUARE, TEXT_COLOR, LAYER_TEXT);
    const float halfDash = UiCanvas::textWidth("-", SCORE_SQUARE) / 2.0F;
    const float blueRight = middle - halfDash - SCORE_SPACING;
    const float orangeLeft = middle + halfDash + SCORE_SPACING;
    _canvas.text(registry, std::to_string(_final.blueScore), blueRight, scoreTop, SCORE_SQUARE, opaque(BLUE_TEAM),
                 LAYER_TEXT, Align::Right);
    _canvas.text(registry, std::to_string(_final.orangeScore), orangeLeft, scoreTop, SCORE_SQUARE, opaque(ORANGE_TEAM),
                 LAYER_TEXT, Align::Left);
    const float labelTop = scoreTop + (GLYPH_HEIGHT * SCORE_SQUARE) + (4.0F * SMALL_SQUARE);
    _canvas.text(registry, "BLEU", blueRight, labelTop, SMALL_SQUARE, opaque(BLUE_TEAM), LAYER_TEXT, Align::Right);
    _canvas.text(registry, "ORANGE", orangeLeft, labelTop, SMALL_SQUARE, opaque(ORANGE_TEAM), LAYER_TEXT, Align::Left);
    if (_final.overtime) {
        _canvas.text(registry, "APRES PROLONGATION", middle, labelTop + (GLYPH_HEIGHT * SMALL_SQUARE * 2.0F),
                     SMALL_SQUARE, DIM_COLOR, LAYER_TEXT);
    }

    addButton(registry,
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
    _canvas.clear(engine.registry());
    _buttons.clear();
}

}  // namespace rlss::client
