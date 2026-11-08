#include "menu/MenuScenes.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "Arena.hpp"
#include "AssetsPlugin.hpp"
#include "Controls.hpp"
#include "components/Renderable.hpp"
#include "config/Settings.hpp"
#include "core/Engine.hpp"
#include "core/services/IScreenDrawService.hpp"
#include "core/services/IWindowService.hpp"
#include "core/services/InputCodes.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "events/UiEvents.hpp"
#include "hierarchy/Hierarchy.hpp"
#include "input/KeyNames.hpp"
#include "input/KeyboardLayout.hpp"
#include "menu/MenuKit.hpp"
#include "menu/MenuScenesPlugin.hpp"
#include "protocol/Protocol.hpp"
#include "scene/SceneContext.hpp"
#include "scene/SceneManager.hpp"
#include "scene/SceneTypes.hpp"
#include "ui/components/UiAnchor.hpp"
#include "ui/components/UiButton.hpp"
#include "ui/components/UiFocusable.hpp"
#include "ui/components/UiImage.hpp"
#include "ui/components/UiNavigation.hpp"
#include "ui/components/UiRect.hpp"
#include "ui/components/UiScrollList.hpp"
#include "ui/components/UiSlider.hpp"
#include "ui/components/UiTextInput.hpp"

namespace rlss::client {

namespace {

constexpr float TITLE_CAP = 120.0F;
constexpr float HEADING_CAP = 40.0F;
constexpr float FIELD_CAP = 36.0F;
constexpr float WAIT_CAP = 34.0F;
constexpr float RESULT_CAP = 90.0F;
constexpr float SCORE_CAP = 120.0F;
constexpr float SIDE_MARGIN = 32.0F;
constexpr float ROW_MAX_WIDTH = 880.0F;
constexpr float ROW_HEIGHT = 64.0F;
constexpr float ROW_GAP = 12.0F;
constexpr float ROW_PADDING = 24.0F;
/** @brief Characters a name may hold, typed letters being made capitals. */
constexpr std::string_view NAME_CHARACTERS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -.";
/** @brief Prefix of the action of a match row; the match id follows. */
constexpr std::string_view JOIN_PREFIX = "browser.join.";
/** @brief Prefix of the action of a control button; the control id follows. */
constexpr std::string_view CONTROL_PREFIX = "controls.key.";
/** @brief Top of the first control button, as a fraction of the screen height. */
constexpr float CONTROLS_TOP = 0.2F;
/** @brief Distance between two control buttons, as a fraction of the screen height. */
constexpr float CONTROLS_STEP = 0.085F;
/** @brief Height of a button on the controls screen, smaller than a menu button so the six rows fit. */
constexpr float CONTROL_BUTTON_HEIGHT = menu::BUTTON_HEIGHT * 0.7F;

/**
 * @brief Gets the saved settings, if the program has them.
 * @param context Scene context.
 * @return The settings, or `nullptr`.
 */
config::Settings* settingsOf(scene::SceneContext& context) {
    auto service = context.engine().mediator().getService<config::Settings>();
    return service ? &service->get() : nullptr;
}

/**
 * @brief Tells whether a key may be given to a control: Escape stays the back key.
 * @param key Key.
 * @return Whether it can be bound.
 */
bool isBindable(engine::Key key) noexcept { return key != engine::Key::Escape; }

/**
 * @brief Gets the layout of the player's keyboard, detected once.
 * @return The layout.
 */
input::KeyboardLayout layout() {
    static const input::KeyboardLayout detected = input::detectKeyboardLayout();
    return detected;
}

/**
 * @brief Gets the state shared by the menus.
 * @param context Scene context.
 * @return The state.
 */
MenuState& stateOf(scene::SceneContext& context) { return context.registry().getResource<MenuState>(); }

/**
 * @brief Gets the menus plugin.
 * @param context Scene context.
 * @return The plugin.
 */
MenuScenesPlugin& menusOf(scene::SceneContext& context) { return context.engine().getPlugin<MenuScenesPlugin>(); }

/**
 * @brief A clock value in seconds as "M:SS", with a "+" in overtime.
 * @param seconds Clock.
 * @param overtime Overtime.
 * @return Text.
 */
std::string clockText(std::uint16_t seconds, bool overtime) {
    const std::string rest = std::to_string(seconds % 60);
    return (overtime ? "+" : "") + std::to_string(seconds / 60) + ":" + (rest.size() < 2 ? "0" : "") + rest;
}

/**
 * @brief Spawns the notice line, kept up to date with `MenuState::notice` by a scene system.
 * @param context Scene owning it.
 * @param anchorY Vertical anchor.
 */
void notice(scene::SceneContext& context, float anchorY) {
    const ecs::Entity line =
        menu::text(context, ecs::NULL_ENTITY, 0.5F, anchorY, stateOf(context).notice, menu::SMALL_CAP, menu::NOTICE);
    context.addSystem(scene::SceneStage::Update, [&context, line](engine::Engine& /*engine*/, const ecs::time::Time&) {
        menu::setText(context.registry(), line, stateOf(context).notice);
    });
}

/**
 * @brief Text centred vertically in a row, at a fraction of its width.
 * @param context Scene owning it.
 * @param row The row.
 * @param anchorX Fraction of the row width.
 * @param text Text.
 * @param color Colour.
 * @param align Alignment.
 * @param offsetX Offset in pixels.
 */
void rowText(scene::SceneContext& context, ecs::Entity row, float anchorX, std::string text, ecs::Rgba color,
             menu::Align align, float offsetX = 0.0F) {
    const ecs::Entity entity =
        menu::text(context, row, anchorX, 0.5F, std::move(text), menu::LABEL_CAP, color, align, offsetX);
    context.registry().getComponents<ui::UiAnchor>().get(entity).offsetY -= menu::LABEL_CAP / 2.0F;
}

/**
 * @brief Anchor of a button placed `pixels` below `anchorY`, for buttons stacked at a fixed gap.
 * @param context Scene context.
 * @param anchorY Top of the first button.
 * @param index Rank of the button.
 * @return Vertical anchor.
 */
float stacked(scene::SceneContext& context, float anchorY, int index) {
    return anchorY +
           ((static_cast<float>(index) * (menu::BUTTON_HEIGHT + menu::BUTTON_GAP)) / menu::screenHeight(context));
}

}  // namespace

void NameScene::onEnter(scene::SceneContext& context) {
    ecs::Registry& registry = context.registry();
    (void)menu::overlay(context);
    menu::title(context, 0.13F, TITLE_CAP);
    (void)menu::text(context, ecs::NULL_ENTITY, 0.5F, 0.38F, "CHOISIS TON PSEUDO", menu::SMALL_CAP, menu::DIM);

    const float fieldWidth =
        std::min(menu::screenWidth(context) - (2.0F * SIDE_MARGIN),
                 menu::textWidth(context, std::string(protocol::MAX_NAME_LENGTH, 'W'), FIELD_CAP) + 44.0F);
    _field = context.spawn();
    registry.addComponent(_field, ui::UiAnchor{.anchorX = 0.5F, .anchorY = 0.45F, .pivotX = 0.5F});
    registry.addComponent(
        _field, ui::UiTextInput{.text = stateOf(context).name,
                                .action = "name.confirm",
                                .allowed = std::string(NAME_CHARACTERS),
                                .uppercase = true,
                                .maxLength = protocol::MAX_NAME_LENGTH,
                                .width = fieldWidth,
                                .height = FIELD_CAP + 44.0F,
                                .fontSize = FIELD_CAP / (700.0F / 1205.0F),
                                .fontId = registry.hasResource<GameAssets>() ? registry.getResource<GameAssets>().font
                                                                             : engine::NO_FONT,
                                .background = menu::FIELD,
                                .border = {.r = 60, .g = 120, .b = 255, .a = 200}});
    registry.addComponent(_field, ui::UiFocusable{.order = 0});
    (void)menu::text(context, ecs::NULL_ENTITY, 0.5F, 0.585F,
                     std::to_string(protocol::MAX_NAME_LENGTH) + " CARACTERES MAX : LETTRES, CHIFFRES, ESPACE, - .",
                     menu::TINY_CAP, menu::DIM);
    (void)menu::button(context, ecs::NULL_ENTITY, 0.5F, 0.66F, menu::BUTTON_WIDTH, menu::BUTTON_HEIGHT, "VALIDER",
                       "name.confirm", 1);
    notice(context, 0.8F);
    menu::focus(registry, _field);

    const auto confirm = [this, &context] {
        MenuScenesPlugin::confirmName(context.engine(),
                                      context.registry().getComponents<ui::UiTextInput>().get(_field).text);
    };
    menu::onClick(context, [confirm](std::string_view action) {
        if (action == "name.confirm") {
            confirm();
        }
    });
    context.subscribe<ui::UiTextSubmittedEvent>([&context, confirm](const ui::UiTextSubmittedEvent& event) {
        if (event.action == "name.confirm" && context.manager().top() == context.id()) {
            confirm();
        }
    });
    menu::onBack(context, [&context] {
        if (!stateOf(context).name.empty()) {
            context.manager().switchTo<MenuScene>();  // renaming: keep the old name
        } else {
            context.engine().stop();
        }
    });
}

void MenuScene::onEnter(scene::SceneContext& context) {
    (void)menu::overlay(context);
    menu::title(context, 0.13F, TITLE_CAP);
    (void)menu::text(context, ecs::NULL_ENTITY, 0.5F, 0.34F, "MATCHS DE 3 MINUTES  -  2 CONTRE 2", menu::SMALL_CAP,
                     menu::DIM);
    _first = menu::button(context, ecs::NULL_ENTITY, 0.5F, stacked(context, 0.45F, 0), menu::BUTTON_WIDTH,
                          menu::BUTTON_HEIGHT, "NOUVELLE PARTIE", "menu.create", 0);
    (void)menu::button(context, ecs::NULL_ENTITY, 0.5F, stacked(context, 0.45F, 1), menu::BUTTON_WIDTH,
                       menu::BUTTON_HEIGHT, "REJOINDRE", "menu.browse", 1);
    (void)menu::button(context, ecs::NULL_ENTITY, 0.5F, stacked(context, 0.45F, 2), menu::BUTTON_WIDTH,
                       menu::BUTTON_HEIGHT * 0.8F, "PARAMETRES", "menu.settings", 2);
    notice(context, 0.84F);

    const std::string label = "PSEUDO : " + stateOf(context).name;
    const float nameWidth = menu::textWidth(context, label, menu::LABEL_CAP) + 46.0F;
    (void)menu::button(context, ecs::NULL_ENTITY, (SIDE_MARGIN + (nameWidth / 2.0F)) / menu::screenWidth(context),
                       SIDE_MARGIN / menu::screenHeight(context), nameWidth, 48.0F, label, "menu.rename", 3);

    const ecs::Entity online = menu::text(context, ecs::NULL_ENTITY, 0.5F, 0.92F, "", menu::SMALL_CAP, menu::DIM);
    context.addSystem(scene::SceneStage::Update,
                      [&context, online](engine::Engine& /*engine*/, const ecs::time::Time&) {
                          const std::uint16_t count = stateOf(context).matches.online;
                          menu::setText(context.registry(), online,
                                        std::to_string(count) + (count > 1 ? " JOUEURS EN LIGNE" : " JOUEUR EN LIGNE"));
                      });
    menu::focus(context.registry(), _first);

    menu::onClick(context, [&context](std::string_view action) {
        if (action == "menu.create") {
            MenuScenesPlugin::request(context.engine(), protocol::encode(protocol::CreateMatch{}));
        } else if (action == "menu.browse") {
            context.manager().switchTo<BrowserScene>();
        } else if (action == "menu.settings") {
            context.manager().push<SettingsScene>();
        } else if (action == "menu.rename") {
            context.manager().switchTo<NameScene>();
        }
    });
    menu::onBack(context, [&context] { context.engine().stop(); });
}

void MenuScene::onResume(scene::SceneContext& context) { menu::focus(context.registry(), _first); }

void BrowserScene::onEnter(scene::SceneContext& context) {
    ecs::Registry& registry = context.registry();
    _rows.clear();
    const float width = menu::screenWidth(context);
    const float height = menu::screenHeight(context);
    (void)menu::overlay(context);
    (void)menu::text(context, ecs::NULL_ENTITY, 0.5F, 0.08F, "PARTIES EN COURS", HEADING_CAP, menu::TEXT);

    _rowWidth = std::min(ROW_MAX_WIDTH, width - (2.0F * SIDE_MARGIN));
    const auto visible =
        static_cast<std::size_t>(std::max(1.0F, ((height * 0.58F) + ROW_GAP) / (ROW_HEIGHT + ROW_GAP)));
    _list = context.spawn();
    registry.addComponent(_list, ui::UiAnchor{.anchorX = 0.5F, .anchorY = 0.22F, .pivotX = 0.5F});
    registry.addComponent(_list, ui::UiRect{.width = _rowWidth,
                                            .height = static_cast<float>(visible) * (ROW_HEIGHT + ROW_GAP),
                                            .fillColor = {.r = 0, .g = 0, .b = 0, .a = 0}});
    registry.addComponent(_list, ui::UiScrollList{.visibleCount = visible, .itemHeight = ROW_HEIGHT + ROW_GAP});
    _empty = menu::text(context, ecs::NULL_ENTITY, 0.5F, 0.45F, "", menu::LABEL_CAP, menu::DIM);
    _hint = menu::text(context, ecs::NULL_ENTITY, 0.5F, 0.815F, "", menu::TINY_CAP, menu::DIM);
    notice(context, 0.85F);

    const float buttonWidth = std::min(menu::BUTTON_WIDTH, (width - (2.0F * SIDE_MARGIN) - menu::BUTTON_GAP) / 2.0F);
    const float shift = ((menu::BUTTON_GAP / 2.0F) + (buttonWidth / 2.0F)) / width;
    _back = menu::button(context, ecs::NULL_ENTITY, 0.5F - shift, 0.88F, buttonWidth, menu::BUTTON_HEIGHT * 0.8F,
                         "RETOUR", "browser.back", 1000);
    (void)menu::button(context, ecs::NULL_ENTITY, 0.5F + shift, 0.88F, buttonWidth, menu::BUTTON_HEIGHT * 0.8F,
                       "NOUVELLE PARTIE", "browser.create", 1001);

    context.addSystem(scene::SceneStage::Update, [this, &context](engine::Engine& /*engine*/, const ecs::time::Time&) {
        const ecs::Registry& reg = context.registry();
        const auto& list = reg.getComponents<ui::UiScrollList>().get(_list);
        const std::size_t count = _rows.size();
        std::string hint;
        if (count > list.visibleCount) {
            hint = std::to_string(list.first + 1) + "-" +
                   std::to_string(std::min(count, list.first + list.visibleCount)) + " / " + std::to_string(count) +
                   "  MOLETTE POUR DEFILER";
        }
        menu::setText(context.registry(), _hint, hint);
    });
    menu::onClick(context, [&context](std::string_view action) {
        if (action == "browser.back") {
            context.manager().switchTo<MenuScene>();
        } else if (action == "browser.create") {
            MenuScenesPlugin::request(context.engine(), protocol::encode(protocol::CreateMatch{}));
        } else if (action.starts_with(JOIN_PREFIX)) {
            std::uint16_t match = 0;
            const std::string_view id = action.substr(JOIN_PREFIX.size());
            if (std::from_chars(std::to_address(id.begin()), std::to_address(id.end()), match).ec == std::errc{}) {
                MenuScenesPlugin::request(context.engine(), protocol::encode(protocol::JoinMatch{.matchId = match}));
            }
        }
    });
    menu::onBack(context, [&context] { context.manager().switchTo<MenuScene>(); });
    context.subscribe<MatchListUpdated>([this, &context](const MatchListUpdated& /*event*/) { showMatches(context); });

    showMatches(context);
    menu::focus(registry,
                _rows.empty() || !registry.hasComponent<ui::UiFocusable>(_rows.front()) ? _back : _rows.front());
}

void BrowserScene::showMatches(scene::SceneContext& context) {
    ecs::Registry& registry = context.registry();
    // The rows are rebuilt: remember which match had the focus, to give it back to the same match.
    const ecs::Entity focused =
        registry.hasResource<ui::UiFocus>() ? registry.getResource<ui::UiFocus>().focused : ecs::NULL_ENTITY;
    const bool rowFocused = std::ranges::find(_rows, focused) != _rows.end();
    std::string focusedAction;
    if (rowFocused && registry.hasComponent<ui::UiButton>(focused)) {
        focusedAction = registry.getComponents<ui::UiButton>().get(focused).action;
    }
    for (const ecs::Entity row : _rows) {
        registry.killEntity(row);
    }
    _rows.clear();
    const std::vector<protocol::MatchInfo>& matches = stateOf(context).matches.matches;
    int order = 0;
    for (const protocol::MatchInfo& match : matches) {
        const bool full = match.bluePlayers + match.orangePlayers >= protocol::MAX_PLAYERS_PER_MATCH;
        ecs::Entity row = ecs::NULL_ENTITY;
        if (full) {
            row = context.spawn();
            registry.addComponent(row, ui::UiAnchor{.anchorX = 0.5F, .pivotX = 0.5F});
            registry.addComponent(row,
                                  ui::UiRect{.width = _rowWidth, .height = ROW_HEIGHT, .fillColor = menu::ROW_FULL});
            hierarchy::setParent(registry, row, _list);
        } else {
            row = menu::button(context, _list, 0.5F, 0.0F, _rowWidth, ROW_HEIGHT, "",
                               std::string(JOIN_PREFIX) + std::to_string(match.id), order++);
        }
        const ecs::Rgba text = full ? menu::DIM : menu::TEXT;
        rowText(context, row, 0.0F, "PARTIE " + std::to_string(match.id), text, menu::Align::Left, ROW_PADDING);
        constexpr float SCORE_AT = 0.47F;
        const float dash = menu::textWidth(context, " - ", menu::LABEL_CAP) / 2.0F;
        rowText(context, row, SCORE_AT, "-", text, menu::Align::Center);
        rowText(context, row, SCORE_AT, std::to_string(match.blueScore), menu::opaque(BLUE_TEAM), menu::Align::Right,
                -dash);
        rowText(context, row, SCORE_AT, std::to_string(match.orangeScore), menu::opaque(ORANGE_TEAM), menu::Align::Left,
                dash);
        rowText(context, row, 0.68F, clockText(match.clock, match.overtime), match.overtime ? menu::GOLD : text,
                menu::Align::Center);
        const std::string players = full ? std::string("PLEIN")
                                         : std::to_string(match.bluePlayers + match.orangePlayers) + "/" +
                                               std::to_string(static_cast<int>(protocol::MAX_PLAYERS_PER_MATCH));
        rowText(context, row, 1.0F, players, text, menu::Align::Right, -ROW_PADDING);
        _rows.push_back(row);
    }
    menu::setText(registry, _empty, matches.empty() ? "AUCUNE PARTIE EN COURS" : "");
    if (rowFocused) {
        restoreFocus(registry, focusedAction);
    }
}

void BrowserScene::restoreFocus(ecs::Registry& registry, std::string_view action) const {
    ecs::Entity fallback = ecs::NULL_ENTITY;
    for (const ecs::Entity row : _rows) {
        if (!registry.hasComponent<ui::UiButton>(row)) {
            continue;  // a full match: not joinable
        }
        if (registry.getComponents<ui::UiButton>().get(row).action == action) {
            menu::focus(registry, row);
            return;
        }
        if (fallback == ecs::NULL_ENTITY) {
            fallback = row;
        }
    }
    // The match is gone or full: the first joinable one, else the back button.
    menu::focus(registry, fallback != ecs::NULL_ENTITY ? fallback : _back);
}

void JoiningScene::onEnter(scene::SceneContext& context) {
    (void)menu::overlay(context);
    const ecs::Entity wait =
        menu::text(context, ecs::NULL_ENTITY, 0.5F, 0.5F, "CONNEXION...", WAIT_CAP, menu::TEXT, menu::Align::Center);
    context.registry().getComponents<ui::UiAnchor>().get(wait).offsetY -= WAIT_CAP / 2.0F;
    menu::focus(context.registry(), ecs::NULL_ENTITY);
    auto remaining = std::make_shared<float>(MenuScenesPlugin::REQUEST_TIMEOUT);
    context.addSystem(scene::SceneStage::Update,
                      [&context, remaining](engine::Engine& /*engine*/, const ecs::time::Time& time) {
                          *remaining -= time.deltaTime;
                          if (*remaining <= 0.0F && context.manager().top() == context.id()) {
                              *remaining = MenuScenesPlugin::REQUEST_TIMEOUT;  // pop only once
                              // A late answer is still followed: a match joined late is entered.
                              stateOf(context).requestPending = false;
                              stateOf(context).notify("PAS DE REPONSE DU SERVEUR");
                              context.manager().pop();
                          }
                      });
    // Back gives up at once; the answer, when it comes, is dropped.
    menu::onBack(context, [&context] {
        MenuScenesPlugin::cancelRequest(context.engine());
        stateOf(context).notify("DEMANDE ANNULEE");
        context.manager().pop();
    });
}

void MatchScene::onEnter(scene::SceneContext& context) {
    menu::focus(context.registry(), ecs::NULL_ENTITY);
    menu::onBack(context, [&context] { context.engine().stop(); });
}

void EndedScene::onEnter(scene::SceneContext& context) {
    ecs::Registry& registry = context.registry();
    const MenuState& state = stateOf(context);
    const float width = menu::screenWidth(context);
    const float height = menu::screenHeight(context);
    (void)menu::overlay(context);

    std::string result = "MATCH TERMINE";
    ecs::Rgba color = menu::TEXT;
    if (state.team && state.final.blueScore != state.final.orangeScore) {
        const bool blueWon = state.final.blueScore > state.final.orangeScore;
        const bool won = blueWon == (*state.team == protocol::Team::Blue);
        result = won ? "VICTOIRE" : "DEFAITE";
        color = won ? menu::GOLD : menu::DEFEAT;
    }
    const ecs::Entity glow = context.spawn();
    registry.addComponent(
        glow,
        ui::UiAnchor{
            .anchorX = 0.5F, .anchorY = 0.14F, .offsetY = (RESULT_CAP / 2.0F) - (RESULT_CAP * 1.1F), .pivotX = 0.5F});
    registry.addComponent(
        glow, ui::UiImage{.texture = registry.hasResource<GameAssets>() ? registry.getResource<GameAssets>().hudBand
                                                                        : ecs::NO_TEXTURE,
                          .width = RESULT_CAP * 10.0F,
                          .height = RESULT_CAP * 2.2F,
                          .tint = {.r = color.r, .g = color.g, .b = color.b, .a = 60}});
    (void)menu::text(context, ecs::NULL_ENTITY, 0.5F, 0.14F, result, RESULT_CAP, color);

    const float panelWidth = std::min(640.0F, width - (2.0F * SIDE_MARGIN));
    const float panelHeight = (((0.38F - 0.33F) * height) * 2.0F) + SCORE_CAP + (menu::SMALL_CAP * 3.0F);
    const ecs::Entity panel = context.spawn();
    registry.addComponent(panel, ui::UiAnchor{.anchorX = 0.5F, .anchorY = 0.33F, .pivotX = 0.5F});
    registry.addComponent(panel, ui::UiRect{.width = panelWidth, .height = panelHeight, .fillColor = menu::PANEL});
    const ecs::Entity blueBar = context.spawn();
    registry.addComponent(blueBar, ui::UiAnchor{});
    registry.addComponent(blueBar,
                          ui::UiRect{.width = panelWidth / 2.0F, .height = 6.0F, .fillColor = menu::opaque(BLUE_TEAM)});
    hierarchy::setParent(registry, blueBar, panel);
    const ecs::Entity orangeBar = context.spawn();
    registry.addComponent(orangeBar, ui::UiAnchor{.anchorX = 0.5F});
    registry.addComponent(
        orangeBar, ui::UiRect{.width = panelWidth / 2.0F, .height = 6.0F, .fillColor = menu::opaque(ORANGE_TEAM)});
    hierarchy::setParent(registry, orangeBar, panel);

    const float halfDash = menu::textWidth(context, "-", SCORE_CAP) / 2.0F;
    const float spacing = 48.0F;
    (void)menu::text(context, ecs::NULL_ENTITY, 0.5F, 0.38F, "-", SCORE_CAP, menu::DIM);
    (void)menu::text(context, ecs::NULL_ENTITY, 0.5F, 0.38F, std::to_string(state.final.blueScore), SCORE_CAP,
                     menu::opaque(BLUE_TEAM), menu::Align::Right, -halfDash - spacing);
    (void)menu::text(context, ecs::NULL_ENTITY, 0.5F, 0.38F, std::to_string(state.final.orangeScore), SCORE_CAP,
                     menu::opaque(ORANGE_TEAM), menu::Align::Left, halfDash + spacing);
    const float labels = 0.38F + ((SCORE_CAP + menu::SMALL_CAP) / height);
    (void)menu::text(context, ecs::NULL_ENTITY, 0.5F, labels, "BLEU", menu::SMALL_CAP, menu::opaque(BLUE_TEAM),
                     menu::Align::Right, -halfDash - spacing);
    (void)menu::text(context, ecs::NULL_ENTITY, 0.5F, labels, "ORANGE", menu::SMALL_CAP, menu::opaque(ORANGE_TEAM),
                     menu::Align::Left, halfDash + spacing);
    if (state.final.overtime) {
        (void)menu::text(context, ecs::NULL_ENTITY, 0.5F, 0.33F + ((panelHeight + menu::SMALL_CAP) / height),
                         "APRES PROLONGATION", menu::SMALL_CAP, menu::GOLD);
    }
    const ecs::Entity back = menu::button(context, ecs::NULL_ENTITY, 0.5F, 0.74F, menu::BUTTON_WIDTH,
                                          menu::BUTTON_HEIGHT, "RETOUR AU MENU", "ended.menu", 0);
    menu::focus(registry, back);

    const auto leave = [&context] { menusOf(context).leaveMatch(context.engine()); };
    menu::onClick(context, [leave](std::string_view action) {
        if (action == "ended.menu") {
            leave();
        }
    });
    menu::onBack(context, leave);
}

void SettingsScene::onEnter(scene::SceneContext& context) {
    ecs::Registry& registry = context.registry();
    auto service = context.engine().mediator().getService<config::Settings>();
    config::Settings* settings = service ? &service->get() : nullptr;
    (void)menu::overlay(context);
    (void)menu::text(context, ecs::NULL_ENTITY, 0.5F, 0.12F, "PARAMETRES", HEADING_CAP, menu::TEXT);

    struct Row {
        const char* caption;
        const char* key;
        const char* action;
        float anchorY;
    };
    constexpr float SLIDER_WIDTH = 320.0F;
    ecs::Entity first = ecs::NULL_ENTITY;
    int order = 0;
    for (const Row& row :
         {Row{.caption = "VOLUME GENERAL", .key = "audio.master", .action = "settings.master", .anchorY = 0.32F},
          Row{.caption = "MUSIQUE", .key = "audio.music", .action = "settings.music", .anchorY = 0.42F},
          Row{.caption = "EFFETS", .key = "audio.sfx", .action = "settings.sfx", .anchorY = 0.52F}}) {
        (void)menu::text(context, ecs::NULL_ENTITY, 0.5F, row.anchorY, row.caption, menu::SMALL_CAP, menu::DIM,
                         menu::Align::Right, -24.0F);
        const ecs::Entity slider = context.spawn();
        registry.addComponent(slider, ui::UiAnchor{.anchorX = 0.5F, .anchorY = row.anchorY, .offsetX = 24.0F});
        registry.addComponent(
            slider, ui::UiSlider{.value = settings != nullptr ? static_cast<float>(settings->getNumber(row.key)) : 1.0F,
                                 .step = 0.1F,
                                 .action = row.action,
                                 .width = SLIDER_WIDTH,
                                 .height = 18.0F,
                                 .fill = menu::opaque(BLUE_TEAM)});
        registry.addComponent(slider, ui::UiFocusable{.order = order++});
        if (first == ecs::NULL_ENTITY) {
            first = slider;
        }
    }
    (void)menu::button(context, ecs::NULL_ENTITY, 0.5F, 0.62F, menu::BUTTON_WIDTH, menu::BUTTON_HEIGHT * 0.8F,
                       "TOUCHES", "settings.controls", order++);
    (void)menu::button(context, ecs::NULL_ENTITY, 0.5F, 0.72F, menu::BUTTON_WIDTH, menu::BUTTON_HEIGHT * 0.8F, "RETOUR",
                       "settings.back", order);
    menu::focus(registry, first);

    context.subscribe<ui::UiValueChangedEvent>([settings](const ui::UiValueChangedEvent& event) {
        if (settings == nullptr) {
            return;
        }
        if (event.action == "settings.master") {
            settings->set("audio.master", static_cast<double>(event.value));
        } else if (event.action == "settings.music") {
            settings->set("audio.music", static_cast<double>(event.value));
        } else if (event.action == "settings.sfx") {
            settings->set("audio.sfx", static_cast<double>(event.value));
        }
    });
    menu::onClick(context, [&context](std::string_view action) {
        if (action == "settings.back") {
            context.manager().pop();
        } else if (action == "settings.controls") {
            context.manager().push<ControlsScene>();
        }
    });
    menu::onBack(context, [&context] { context.manager().pop(); });
}

void ControlsScene::onEnter(scene::SceneContext& context) {
    _waiting.reset();
    _wasDown.fill(true);  // A key held when the scene opens (Enter on "TOUCHES") is not a new press.
    _justBound = engine::Key::Unknown;

    ecs::Registry& registry = context.registry();
    (void)menu::overlay(context);
    (void)menu::text(context, ecs::NULL_ENTITY, 0.5F, 0.08F, "TOUCHES", HEADING_CAP, menu::TEXT);
    int order = 0;
    float anchorY = CONTROLS_TOP;
    for (const Control control : CONTROLS) {
        const auto index = static_cast<std::size_t>(control);
        const ecs::Entity button =
            menu::button(context, ecs::NULL_ENTITY, 0.5F, anchorY, menu::BUTTON_WIDTH, CONTROL_BUTTON_HEIGHT, "",
                         std::string(CONTROL_PREFIX).append(controlId(control)), order++);
        const ecs::Entity label = menu::text(context, button, 0.5F, 0.5F, "", menu::LABEL_CAP, menu::TEXT);
        registry.getComponents<ui::UiAnchor>().get(label).offsetY -= menu::LABEL_CAP / 2.0F;
        _buttons.at(index) = button;
        _labels.at(index) = label;
        anchorY += CONTROLS_STEP;
    }
    anchorY += CONTROLS_STEP * 0.3F;
    (void)menu::button(context, ecs::NULL_ENTITY, 0.5F, anchorY, menu::BUTTON_WIDTH, CONTROL_BUTTON_HEIGHT,
                       "PAR DEFAUT", "controls.reset", order++);
    anchorY += CONTROLS_STEP;
    (void)menu::button(context, ecs::NULL_ENTITY, 0.5F, anchorY, menu::BUTTON_WIDTH, CONTROL_BUTTON_HEIGHT, "RETOUR",
                       "controls.back", order);
    (void)menu::text(context, ecs::NULL_ENTITY, 0.5F, anchorY + CONTROLS_STEP, "ECHAP ANNULE - MANETTE NON MODIFIABLE",
                     menu::TINY_CAP, menu::DIM);
    refreshLabels(context);
    menu::focus(registry, _buttons.front());

    menu::onBack(context, [this, &context] {
        if (_waiting) {
            _waiting.reset();
            refreshLabels(context);
        } else {
            context.manager().pop();
        }
    });
    menu::onClick(context, [this, &context](std::string_view action) {
        if (_waiting || _justBound != engine::Key::Unknown) {
            return;
        }
        if (action == "controls.back") {
            context.manager().pop();
            return;
        }
        config::Settings* settings = settingsOf(context);
        if (action == "controls.reset") {
            if (settings != nullptr) {
                writeControlKeys(*settings, defaultControlKeys());
            }
            refreshLabels(context);
            return;
        }
        if (!action.starts_with(CONTROL_PREFIX)) {
            return;
        }
        for (const Control control : CONTROLS) {
            if (action.substr(CONTROL_PREFIX.size()) == controlId(control)) {
                _waiting = control;
                // The key that clicked (Enter, Space) may still be down: only a press after this one counts.
                _wasDown.fill(true);
                menu::setText(context.registry(), _labels.at(static_cast<std::size_t>(control)),
                              std::string(controlLabel(control)) + " : APPUIE SUR UNE TOUCHE");
            }
        }
    });
    context.addSystem(scene::SceneStage::Update,
                      [this, &context](engine::Engine& /*engine*/, const ecs::time::Time&) { pollKeys(context); });
}

void ControlsScene::refreshLabels(scene::SceneContext& context) const {
    const config::Settings* settings = settingsOf(context);
    const ControlKeys keys = settings != nullptr ? readControlKeys(*settings) : defaultControlKeys();
    for (const Control control : CONTROLS) {
        const auto index = static_cast<std::size_t>(control);
        menu::setText(context.registry(), _labels.at(index),
                      std::string(controlLabel(control)) + " : " + input::keyLabel(keys.at(index), layout()));
    }
}

void ControlsScene::pollKeys(scene::SceneContext& context) {
    auto window = context.engine().mediator().getService<engine::IWindowService>();
    if (!window) {
        return;
    }
    std::optional<engine::Key> pressed;
    for (const input::KeyName& name : input::keyNames()) {
        const bool down = window->get().isKeyDown(name.key);
        bool& wasDown = _wasDown.at(static_cast<std::size_t>(name.key));
        if (down && !wasDown && !pressed && isBindable(name.key)) {
            pressed = name.key;
        }
        wasDown = down;
    }
    if (_justBound != engine::Key::Unknown && !window->get().isKeyDown(_justBound)) {
        _justBound = engine::Key::Unknown;
    }
    if (!_waiting || !pressed) {
        return;
    }
    if (config::Settings* settings = settingsOf(context)) {
        ControlKeys keys = readControlKeys(*settings);
        assignKey(keys, *_waiting, *pressed);
        writeControlKeys(*settings, keys);
    }
    // The arrows also move the focus: give it back to the control that was rebound.
    menu::focus(context.registry(), _buttons.at(static_cast<std::size_t>(*_waiting)));
    _waiting.reset();
    _justBound = *pressed;
    refreshLabels(context);
}

}  // namespace rlss::client
