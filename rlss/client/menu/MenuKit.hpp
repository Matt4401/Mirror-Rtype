/**
 * @file MenuKit.hpp
 * @brief Builds the RLSS menus with the engine UI widgets, in the RLSS style (Russo One, team colours).
 */

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

#include "Arena.hpp"
#include "components/Renderable.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "scene/SceneContext.hpp"

namespace rlss::client::menu {

/** @brief Dark veil over the arena behind every menu. */
inline constexpr ecs::Rgba OVERLAY{.r = 4, .g = 8, .b = 20, .a = 170};
/** @brief Button background. */
inline constexpr ecs::Rgba BUTTON{.r = 14, .g = 22, .b = 44, .a = 235};
/** @brief Button under the cursor. */
inline constexpr ecs::Rgba HOVER{.r = 34, .g = 88, .b = 196, .a = 245};
/** @brief Thin edge of the buttons. */
inline constexpr ecs::Rgba EDGE{.r = 255, .g = 255, .b = 255, .a = 34};
/** @brief A full match in the list. */
inline constexpr ecs::Rgba ROW_FULL{.r = 26, .g = 28, .b = 38, .a = 215};
/** @brief Main text. */
inline constexpr ecs::Rgba TEXT{.r = 240, .g = 244, .b = 250, .a = 255};
/** @brief Secondary text. */
inline constexpr ecs::Rgba DIM{.r = 150, .g = 162, .b = 185, .a = 255};
/** @brief Messages (refused join, invalid name). */
inline constexpr ecs::Rgba NOTICE{.r = 255, .g = 130, .b = 110, .a = 255};
/** @brief Victory, overtime. */
inline constexpr ecs::Rgba GOLD{.r = 255, .g = 206, .b = 64, .a = 255};
/** @brief Defeat. */
inline constexpr ecs::Rgba DEFEAT{.r = 230, .g = 96, .b = 96, .a = 255};
/** @brief Score panel. */
inline constexpr ecs::Rgba PANEL{.r = 6, .g = 10, .b = 24, .a = 200};
/** @brief Name field. */
inline constexpr ecs::Rgba FIELD{.r = 8, .g = 14, .b = 30, .a = 240};

/** @brief Main button width. */
inline constexpr float BUTTON_WIDTH = 460.0F;
/** @brief Main button height. */
inline constexpr float BUTTON_HEIGHT = 72.0F;
/** @brief Gap between two buttons. */
inline constexpr float BUTTON_GAP = 22.0F;
/** @brief Capitals of a button label. */
inline constexpr float LABEL_CAP = 24.0F;
/** @brief Small text. */
inline constexpr float SMALL_CAP = 18.0F;
/** @brief Tiny text. */
inline constexpr float TINY_CAP = 14.0F;

/** @brief Horizontal alignment of a text on its anchor. */
enum class Align : std::uint8_t {
    /** @brief The text starts at the anchor. */
    Left,
    /** @brief The text is centred on the anchor. */
    Center,
    /** @brief The text ends at the anchor. */
    Right,
};

/**
 * @brief Converts a team colour to an opaque UI colour.
 * @param color Team colour.
 * @return Opaque colour.
 */
[[nodiscard]] constexpr ecs::Rgba opaque(Rgb color) noexcept {
    return ecs::Rgba{.r = color.r, .g = color.g, .b = color.b, .a = 255};
}

/**
 * @brief Spawns the dark veil covering the whole screen.
 * @param context Scene owning it.
 * @return The veil.
 */
ecs::Entity overlay(scene::SceneContext& context);

/**
 * @brief Spawns a text whose capitals are `cap` pixels high, their top at the anchor.
 * @param context Scene owning it.
 * @param parent Parent element, or `ecs::NULL_ENTITY` for the screen.
 * @param anchorX Horizontal anchor in the parent, as a fraction of its width.
 * @param anchorY Vertical anchor in the parent, as a fraction of its height.
 * @param text Text.
 * @param cap Height of the capitals in pixels.
 * @param color Colour.
 * @param align Alignment on the anchor.
 * @param offsetX Extra horizontal offset in pixels.
 * @return The text element.
 */
ecs::Entity text(scene::SceneContext& context, ecs::Entity parent, float anchorX, float anchorY, std::string text,
                 float cap, ecs::Rgba color, Align align = Align::Center, float offsetX = 0.0F);

/**
 * @brief Spawns the game title with its glow and its team bar, centred, its capitals at `anchorY` of the screen.
 * @param context Scene owning it.
 * @param anchorY Top of the capitals, as a fraction of the screen height.
 * @param cap Height of the capitals.
 */
void title(scene::SceneContext& context, float anchorY, float cap);

/**
 * @brief Spawns a focusable button in the RLSS style: edge, blue accent strip, centred label.
 * @param context Scene owning it.
 * @param parent Parent element, or `ecs::NULL_ENTITY` for the screen.
 * @param anchorX Horizontal anchor of its centre.
 * @param anchorY Vertical anchor of its top.
 * @param width Width in pixels.
 * @param height Height in pixels.
 * @param label Label, or empty.
 * @param action Action sent with `ui::UiButtonClickedEvent`.
 * @param order Focus order.
 * @return The button.
 */
ecs::Entity button(scene::SceneContext& context, ecs::Entity parent, float anchorX, float anchorY, float width,
                   float height, std::string label, std::string action, int order);

/**
 * @brief Gives the keyboard and gamepad focus to a widget.
 * @param registry Registry holding the UI.
 * @param widget Widget, or `ecs::NULL_ENTITY`.
 */
void focus(ecs::Registry& registry, ecs::Entity widget);

/**
 * @brief Changes the text of a `ui::UiText`, only if it differs.
 * @param registry Registry holding it.
 * @param element Text element.
 * @param text New text.
 */
void setText(ecs::Registry& registry, ecs::Entity element, std::string_view text);

/**
 * @brief Measures a text in the RLSS font.
 * @param context Scene context (for the draw service).
 * @param text Text.
 * @param cap Height of the capitals.
 * @return Width in pixels.
 */
[[nodiscard]] float textWidth(scene::SceneContext& context, std::string_view text, float cap);

/**
 * @brief Gets the height of the screen.
 * @param context Scene context.
 * @return Height in pixels.
 */
[[nodiscard]] float screenHeight(scene::SceneContext& context);

/**
 * @brief Gets the width of the screen.
 * @param context Scene context.
 * @return Width in pixels.
 */
[[nodiscard]] float screenWidth(scene::SceneContext& context);

/**
 * @brief Runs `handler` on each `ui::UiBackEvent` while the scene is the top of the stack.
 * @param context Scene listening.
 * @param handler Called on "back" (Escape, gamepad B).
 */
void onBack(scene::SceneContext& context, std::function<void()> handler);

/**
 * @brief Runs `handler` with the action of each button clicked while the scene is the top of the stack.
 * @param context Scene listening.
 * @param handler Called with the action.
 */
void onClick(scene::SceneContext& context, std::function<void(std::string_view action)> handler);

}  // namespace rlss::client::menu
