#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "core/services/InputCodes.hpp"
#include "input/ActionId.hpp"
#include "input/ActionMapping.hpp"

namespace client::input {

/** @brief Button action that toggles the ready state in the lobby; it is never sent as a game input. */
inline constexpr ::input::ActionId ACTION_READY = ::input::makeActionId("ready");

/** @brief A control the player can rebind from the controls screen. */
enum class Control : std::uint8_t {
    /** @brief Negative side of `move_y`. */
    MoveUp,
    /** @brief Positive side of `move_y`. */
    MoveDown,
    /** @brief Negative side of `move_x`. */
    MoveLeft,
    /** @brief Positive side of `move_x`. */
    MoveRight,
    /** @brief `shoot`. */
    Shoot,
    /** @brief `charge_weapon`. */
    Charge,
    /** @brief `ready`, in the lobby. */
    Ready,
};

/** @brief Number of `Control` values. */
inline constexpr std::size_t CONTROL_COUNT = 7;

/** @brief Main key of each control, indexed by `Control`. */
using ControlKeys = std::array<engine::Key, CONTROL_COUNT>;

/** @brief Every control, in the order of the controls screen. */
inline constexpr std::array<Control, CONTROL_COUNT> CONTROLS = {
    Control::MoveUp, Control::MoveDown, Control::MoveLeft, Control::MoveRight,
    Control::Shoot,  Control::Charge,   Control::Ready,
};

/**
 * @brief Gets the id of a control, used in its settings key (`controls.<id>`).
 * @param control Control.
 * @return Lower-case id, e.g. "move_up".
 */
[[nodiscard]] std::string_view controlId(Control control) noexcept;

/**
 * @brief Gets the label of a control on the controls screen.
 * @param control Control.
 * @return Upper-case label, e.g. "MOVE UP".
 */
[[nodiscard]] std::string_view controlLabel(Control control) noexcept;

/**
 * @brief Gets the default main keys: W A S D, Space, Left Shift, R (physical positions, so Z Q S D on AZERTY).
 * @return The default keys.
 */
[[nodiscard]] ControlKeys defaultControlKeys() noexcept;

/**
 * @brief Gives a key to a control. A control that already had the key takes the old key of `control` instead, so
 * no key ends up driving two controls.
 * @param keys Keys to change.
 * @param control Control to rebind.
 * @param key New key.
 */
void assignKey(ControlKeys& keys, Control control, engine::Key key) noexcept;

/**
 * @brief Configures the R-Type bindings with the given main keys.
 *
 * Each control gets its main key, plus a secondary key (arrows for moving, Enter for shoot, Right Shift for charge)
 * unless a main key already uses it, and its gamepad binding.
 *
 * @param mapping Mapping to fill; it is cleared first.
 * @param keys Main key of each control.
 */
void setupBindings(::input::ActionMapping& mapping, const ControlKeys& keys);

/**
 * @brief Configures standard key and gamepad bindings for the R-Type game.
 *
 * Utilizes physical spatial cluster keys (Key::W, Key::A, Key::S, Key::D)
 * which naturally align with ZQSD on AZERTY keyboards and WASD on QWERTY keyboards,
 * completely avoiding cross-layout key conflicts.
 *
 * @param mapping Target action mapping instance to populate.
 */
void setupDefaultBindings(::input::ActionMapping& mapping);

}  // namespace client::input
