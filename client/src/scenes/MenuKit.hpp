/**
 * @file MenuKit.hpp
 * @brief Builds the R-Type menus with the engine UI widgets, in one consistent style.
 */

#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

#include "components/Renderable.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "scene/SceneContext.hpp"

namespace client::menu {

/** @brief Panel background. */
inline constexpr ecs::Rgba PANEL = {.r = 12, .g = 14, .b = 32, .a = 225};
/** @brief Panel border and accents. */
inline constexpr ecs::Rgba ACCENT = {.r = 70, .g = 150, .b = 255, .a = 255};
/** @brief Main text. */
inline constexpr ecs::Rgba TEXT = {.r = 235, .g = 238, .b = 255, .a = 255};
/** @brief Secondary text. */
inline constexpr ecs::Rgba MUTED = {.r = 150, .g = 158, .b = 190, .a = 255};
/** @brief Error text. */
inline constexpr ecs::Rgba ERROR = {.r = 255, .g = 110, .b = 110, .a = 255};
/** @brief Success text. */
inline constexpr ecs::Rgba GOOD = {.r = 120, .g = 230, .b = 140, .a = 255};
/** @brief Score and titles. */
inline constexpr ecs::Rgba GOLD = {.r = 255, .g = 214, .b = 70, .a = 255};

/** @brief Default button width in pixels. */
inline constexpr float BUTTON_WIDTH = 280.0F;
/** @brief Default button height in pixels. */
inline constexpr float BUTTON_HEIGHT = 40.0F;

/**
 * @brief Spawns a centred panel owned by the scene.
 * @param context Scene owning the panel.
 * @param width Width in pixels.
 * @param height Height in pixels.
 * @return The panel; widgets are its children, placed from its top.
 */
ecs::Entity panel(scene::SceneContext& context, float width, float height);

/**
 * @brief Spawns a text centred horizontally in its parent.
 * @param context Scene owning the text.
 * @param parent Parent element.
 * @param text Text.
 * @param y Offset from the top of the parent, in pixels.
 * @param size Font size.
 * @param color Colour.
 * @return The text element.
 */
ecs::Entity label(scene::SceneContext& context, ecs::Entity parent, std::string text, float y, float size = 20.0F,
                  ecs::Rgba color = TEXT);

/**
 * @brief Spawns a focusable button centred horizontally in its parent.
 * @param context Scene owning the button.
 * @param parent Parent element.
 * @param text Label.
 * @param action Action sent with `ui::UiButtonClickedEvent`.
 * @param y Offset from the top of the parent.
 * @param order Focus order.
 * @param width Width in pixels.
 * @return The button.
 */
ecs::Entity button(scene::SceneContext& context, ecs::Entity parent, std::string text, std::string action, float y,
                   int order, float width = BUTTON_WIDTH);

/**
 * @brief Spawns a focusable text input centred horizontally in its parent, with a caption above it.
 * @param context Scene owning the input.
 * @param parent Parent element.
 * @param caption Caption shown above the field.
 * @param text Initial text.
 * @param action Action sent with `ui::UiTextSubmittedEvent`.
 * @param allowed Accepted ASCII characters, or empty for any.
 * @param maxLength Maximum length.
 * @param y Offset of the field from the top of the parent.
 * @param order Focus order.
 * @return The input.
 */
ecs::Entity textInput(scene::SceneContext& context, ecs::Entity parent, std::string caption, std::string text,
                      std::string action, std::string allowed, std::size_t maxLength, float y, int order);

/**
 * @brief Spawns a focusable slider with its caption on the left.
 * @param context Scene owning the slider.
 * @param parent Parent element.
 * @param caption Caption.
 * @param value Initial value, in [0, 1].
 * @param action Action sent with `ui::UiValueChangedEvent`.
 * @param y Offset from the top of the parent.
 * @param order Focus order.
 * @return The slider.
 */
ecs::Entity slider(scene::SceneContext& context, ecs::Entity parent, std::string caption, float value,
                   std::string action, float y, int order);

/**
 * @brief Spawns a focusable toggle with its caption on the left.
 * @param context Scene owning the toggle.
 * @param parent Parent element.
 * @param caption Caption.
 * @param on Initial state.
 * @param action Action sent with `ui::UiToggleChangedEvent`.
 * @param y Offset from the top of the parent.
 * @param order Focus order.
 * @return The toggle.
 */
ecs::Entity toggle(scene::SceneContext& context, ecs::Entity parent, std::string caption, bool on, std::string action,
                   float y, int order);

/**
 * @brief Gives the keyboard and gamepad focus to a widget.
 * @param registry Registry holding the UI.
 * @param widget Widget to focus, or `ecs::NULL_ENTITY` to clear the focus.
 */
void focus(ecs::Registry& registry, ecs::Entity widget);

/**
 * @brief Changes the text of a `ui::UiText` (or the label of a `ui::UiButton`), only if it differs.
 * @param registry Registry holding the element.
 * @param element Text or button.
 * @param text New text.
 */
void setText(ecs::Registry& registry, ecs::Entity element, std::string_view text);

/**
 * @brief Changes the colour of a `ui::UiText`.
 * @param registry Registry holding the element.
 * @param element Text element.
 * @param color New colour.
 */
void setColor(ecs::Registry& registry, ecs::Entity element, ecs::Rgba color);

/**
 * @brief Runs `handler` on each `ui::UiBackEvent` while the scene is the top of the stack.
 * @param context Scene listening.
 * @param handler Called on "back" (Escape, gamepad B).
 */
void onBack(scene::SceneContext& context, std::function<void()> handler);

/**
 * @brief Runs `handler` with the action of each button clicked while the scene is the top of the stack.
 * @param context Scene listening.
 * @param handler Called with the button action.
 */
void onClick(scene::SceneContext& context, std::function<void(std::string_view action)> handler);

}  // namespace client::menu
