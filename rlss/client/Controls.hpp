/**
 * @file Controls.hpp
 * @brief The RLSS keyboard controls a player can rebind, their saved settings and the bindings built from them.
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "core/services/InputCodes.hpp"
#include "input/ActionMapping.hpp"

namespace config {
class Settings;
}

namespace rlss::client {

/** @brief A control with its own key: each side of an axis is one control. */
enum class Control : std::uint8_t {
    /** @brief Steer left (negative side of the steer axis). */
    SteerLeft,
    /** @brief Steer right (positive side of the steer axis). */
    SteerRight,
    /** @brief Nose up in the air (positive side of the pitch axis). */
    NoseUp,
    /** @brief Nose down in the air (negative side of the pitch axis). */
    NoseDown,
    /** @brief Jump. */
    Jump,
    /** @brief Boost. */
    Boost,
};

/** @brief Number of controls. */
inline constexpr std::size_t CONTROL_COUNT = 6;

/** @brief Main key of each control, indexed by `Control`. */
using ControlKeys = std::array<engine::Key, CONTROL_COUNT>;

/** @brief Every control, in the order of the controls screen. */
inline constexpr std::array<Control, CONTROL_COUNT> CONTROLS = {
    Control::SteerLeft, Control::SteerRight, Control::NoseUp, Control::NoseDown, Control::Jump, Control::Boost};

/** @brief Prefix of the settings holding the keys; the control id follows. */
inline constexpr std::string_view CONTROLS_PREFIX = "controls.";

/**
 * @brief Gets the id of a control, used in its setting key.
 * @param control Control.
 * @return Id, e.g. "steer_left".
 */
[[nodiscard]] std::string_view controlId(Control control) noexcept;

/**
 * @brief Gets the label of a control on the controls screen.
 * @param control Control.
 * @return Upper-case label, e.g. "GAUCHE".
 */
[[nodiscard]] std::string_view controlLabel(Control control) noexcept;

/**
 * @brief Gets the setting holding the key of a control.
 * @param control Control.
 * @return Key, e.g. "controls.jump".
 */
[[nodiscard]] std::string controlSetting(Control control);

/**
 * @brief Default keys: A / D steer, W / S aim the nose, Space jumps, left Shift boosts.
 * @return The keys.
 */
[[nodiscard]] ControlKeys defaultControlKeys() noexcept;

/**
 * @brief Gives a key to a control; a control that already had it takes the old key, so no key drives two controls.
 * @param keys Keys to change.
 * @param control Control to rebind.
 * @param key New key.
 */
void assignKey(ControlKeys& keys, Control control, engine::Key key) noexcept;

/**
 * @brief Declares a setting per control, holding its default key.
 * @param settings Settings.
 */
void defineControlSettings(config::Settings& settings);

/**
 * @brief Reads the key of each control; a missing or unknown id keeps the default key.
 * @param settings Settings.
 * @return The keys.
 */
[[nodiscard]] ControlKeys readControlKeys(const config::Settings& settings);

/**
 * @brief Saves the key of each control.
 * @param settings Settings.
 * @param keys Keys.
 */
void writeControlKeys(config::Settings& settings, const ControlKeys& keys);

/**
 * @brief Rebuilds the bindings: the main keys, the arrows and right Shift as second keys (unless a main key uses
 * one), and the fixed gamepad layout (left stick, bottom face button jumps, right face button or right trigger boosts).
 * @param mapping Mapping to fill; its old bindings are removed.
 * @param keys Main keys.
 */
void setupBindings(input::ActionMapping& mapping, const ControlKeys& keys);

/**
 * @brief Default controls (`defaultControlKeys`): steer with A/D or the arrows, aim the nose with W/S or the arrows,
 * jump with Space, boost with either Shift; on a gamepad, the left stick, the bottom face button (jump) and the right
 * face button or right trigger (boost). `MenuScenesPlugin` replaces them with the keys saved in the settings.
 * @return The mapping, for `input::InputPlugin`.
 */
[[nodiscard]] input::ActionMapping makeControls();

}  // namespace rlss::client
