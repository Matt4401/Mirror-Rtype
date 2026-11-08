/**
 * @file UiNavigation.hpp
 * @brief Resources configuring focus navigation and holding the focused widget.
 */

#pragma once

#include <vector>

#include "components/Renderable.hpp"
#include "core/services/InputCodes.hpp"
#include "ecs/Entity.hpp"

namespace ui {

/** @brief Keys and gamepad buttons bound to one navigation command. */
struct UiNavBinding {
    /** @brief Keyboard keys. */
    std::vector<engine::Key> keys;
    /** @brief Gamepad buttons. */
    std::vector<engine::GamepadButton> buttons;
};

/**
 * @brief Registry resource configuring focus navigation; emplaced with these defaults by `UiPlugin`.
 *
 * Games rebind it freely (`registry.getResource<ui::UiNavigation>()`). Space confirms, except while a text input has
 * the focus, where it types a space.
 */
struct UiNavigation {
    /** @brief Previous widget. */
    UiNavBinding up{.keys = {engine::Key::Up}, .buttons = {engine::GamepadButton::LeftFaceUp}};
    /** @brief Next widget. */
    UiNavBinding down{.keys = {engine::Key::Down, engine::Key::Tab}, .buttons = {engine::GamepadButton::LeftFaceDown}};
    /** @brief Decrease a slider, else previous widget. */
    UiNavBinding left{.keys = {engine::Key::Left}, .buttons = {engine::GamepadButton::LeftFaceLeft}};
    /** @brief Increase a slider, else next widget. */
    UiNavBinding right{.keys = {engine::Key::Right}, .buttons = {engine::GamepadButton::LeftFaceRight}};
    /** @brief Activate the focused widget. */
    UiNavBinding confirm{
        .keys = {engine::Key::Enter, engine::Key::Space},
        .buttons = {engine::GamepadButton::RightFaceDown},
    };
    /** @brief Publish `UiBackEvent`. */
    UiNavBinding back{.keys = {engine::Key::Escape}, .buttons = {engine::GamepadButton::RightFaceRight}};
    /** @brief Gamepad read for navigation. */
    int gamepad = 0;
    /** @brief Colour of the outline drawn around the focused widget. */
    ecs::Rgba focusColor = {.r = 255, .g = 210, .b = 60, .a = 255};
    /** @brief Thickness of that outline in pixels. */
    float focusThickness = 2.0F;
};

/** @brief Registry resource holding the focused widget; emplaced by `UiPlugin`. */
struct UiFocus {
    /** @brief Focused widget, or `ecs::NULL_ENTITY`. Set it to move the focus from game code. */
    ecs::Entity focused = ecs::NULL_ENTITY;
    /** @brief Blink phase of the caret of a focused text input; driven by the UI interaction system. */
    bool caretVisible = true;
};

}  // namespace ui
