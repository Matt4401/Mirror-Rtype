#include "input/KeyNames.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "core/services/InputCodes.hpp"
#include "input/KeyboardLayout.hpp"

namespace input {

namespace {

/** @brief Every bindable key, in enum order. */
constexpr std::array KEY_NAMES = {
    KeyName{.key = engine::Key::Apostrophe, .id = "Apostrophe"},
    KeyName{.key = engine::Key::Comma, .id = "Comma"},
    KeyName{.key = engine::Key::Minus, .id = "Minus"},
    KeyName{.key = engine::Key::Period, .id = "Period"},
    KeyName{.key = engine::Key::Slash, .id = "Slash"},
    KeyName{.key = engine::Key::Digit0, .id = "Digit0"},
    KeyName{.key = engine::Key::Digit1, .id = "Digit1"},
    KeyName{.key = engine::Key::Digit2, .id = "Digit2"},
    KeyName{.key = engine::Key::Digit3, .id = "Digit3"},
    KeyName{.key = engine::Key::Digit4, .id = "Digit4"},
    KeyName{.key = engine::Key::Digit5, .id = "Digit5"},
    KeyName{.key = engine::Key::Digit6, .id = "Digit6"},
    KeyName{.key = engine::Key::Digit7, .id = "Digit7"},
    KeyName{.key = engine::Key::Digit8, .id = "Digit8"},
    KeyName{.key = engine::Key::Digit9, .id = "Digit9"},
    KeyName{.key = engine::Key::Semicolon, .id = "Semicolon"},
    KeyName{.key = engine::Key::Equal, .id = "Equal"},
    KeyName{.key = engine::Key::A, .id = "A"},
    KeyName{.key = engine::Key::B, .id = "B"},
    KeyName{.key = engine::Key::C, .id = "C"},
    KeyName{.key = engine::Key::D, .id = "D"},
    KeyName{.key = engine::Key::E, .id = "E"},
    KeyName{.key = engine::Key::F, .id = "F"},
    KeyName{.key = engine::Key::G, .id = "G"},
    KeyName{.key = engine::Key::H, .id = "H"},
    KeyName{.key = engine::Key::I, .id = "I"},
    KeyName{.key = engine::Key::J, .id = "J"},
    KeyName{.key = engine::Key::K, .id = "K"},
    KeyName{.key = engine::Key::L, .id = "L"},
    KeyName{.key = engine::Key::M, .id = "M"},
    KeyName{.key = engine::Key::N, .id = "N"},
    KeyName{.key = engine::Key::O, .id = "O"},
    KeyName{.key = engine::Key::P, .id = "P"},
    KeyName{.key = engine::Key::Q, .id = "Q"},
    KeyName{.key = engine::Key::R, .id = "R"},
    KeyName{.key = engine::Key::S, .id = "S"},
    KeyName{.key = engine::Key::T, .id = "T"},
    KeyName{.key = engine::Key::U, .id = "U"},
    KeyName{.key = engine::Key::V, .id = "V"},
    KeyName{.key = engine::Key::W, .id = "W"},
    KeyName{.key = engine::Key::X, .id = "X"},
    KeyName{.key = engine::Key::Y, .id = "Y"},
    KeyName{.key = engine::Key::Z, .id = "Z"},
    KeyName{.key = engine::Key::LeftBracket, .id = "LeftBracket"},
    KeyName{.key = engine::Key::Backslash, .id = "Backslash"},
    KeyName{.key = engine::Key::RightBracket, .id = "RightBracket"},
    KeyName{.key = engine::Key::Grave, .id = "Grave"},
    KeyName{.key = engine::Key::Space, .id = "Space"},
    KeyName{.key = engine::Key::Escape, .id = "Escape"},
    KeyName{.key = engine::Key::Enter, .id = "Enter"},
    KeyName{.key = engine::Key::Tab, .id = "Tab"},
    KeyName{.key = engine::Key::Backspace, .id = "Backspace"},
    KeyName{.key = engine::Key::Insert, .id = "Insert"},
    KeyName{.key = engine::Key::Delete, .id = "Delete"},
    KeyName{.key = engine::Key::Right, .id = "Right"},
    KeyName{.key = engine::Key::Left, .id = "Left"},
    KeyName{.key = engine::Key::Down, .id = "Down"},
    KeyName{.key = engine::Key::Up, .id = "Up"},
    KeyName{.key = engine::Key::PageUp, .id = "PageUp"},
    KeyName{.key = engine::Key::PageDown, .id = "PageDown"},
    KeyName{.key = engine::Key::Home, .id = "Home"},
    KeyName{.key = engine::Key::End, .id = "End"},
    KeyName{.key = engine::Key::CapsLock, .id = "CapsLock"},
    KeyName{.key = engine::Key::ScrollLock, .id = "ScrollLock"},
    KeyName{.key = engine::Key::NumLock, .id = "NumLock"},
    KeyName{.key = engine::Key::PrintScreen, .id = "PrintScreen"},
    KeyName{.key = engine::Key::Pause, .id = "Pause"},
    KeyName{.key = engine::Key::F1, .id = "F1"},
    KeyName{.key = engine::Key::F2, .id = "F2"},
    KeyName{.key = engine::Key::F3, .id = "F3"},
    KeyName{.key = engine::Key::F4, .id = "F4"},
    KeyName{.key = engine::Key::F5, .id = "F5"},
    KeyName{.key = engine::Key::F6, .id = "F6"},
    KeyName{.key = engine::Key::F7, .id = "F7"},
    KeyName{.key = engine::Key::F8, .id = "F8"},
    KeyName{.key = engine::Key::F9, .id = "F9"},
    KeyName{.key = engine::Key::F10, .id = "F10"},
    KeyName{.key = engine::Key::F11, .id = "F11"},
    KeyName{.key = engine::Key::F12, .id = "F12"},
    KeyName{.key = engine::Key::LeftShift, .id = "LeftShift"},
    KeyName{.key = engine::Key::LeftControl, .id = "LeftControl"},
    KeyName{.key = engine::Key::LeftAlt, .id = "LeftAlt"},
    KeyName{.key = engine::Key::LeftSuper, .id = "LeftSuper"},
    KeyName{.key = engine::Key::RightShift, .id = "RightShift"},
    KeyName{.key = engine::Key::RightControl, .id = "RightControl"},
    KeyName{.key = engine::Key::RightAlt, .id = "RightAlt"},
    KeyName{.key = engine::Key::RightSuper, .id = "RightSuper"},
    KeyName{.key = engine::Key::KeyboardMenu, .id = "KeyboardMenu"},
    KeyName{.key = engine::Key::Keypad0, .id = "Keypad0"},
    KeyName{.key = engine::Key::Keypad1, .id = "Keypad1"},
    KeyName{.key = engine::Key::Keypad2, .id = "Keypad2"},
    KeyName{.key = engine::Key::Keypad3, .id = "Keypad3"},
    KeyName{.key = engine::Key::Keypad4, .id = "Keypad4"},
    KeyName{.key = engine::Key::Keypad5, .id = "Keypad5"},
    KeyName{.key = engine::Key::Keypad6, .id = "Keypad6"},
    KeyName{.key = engine::Key::Keypad7, .id = "Keypad7"},
    KeyName{.key = engine::Key::Keypad8, .id = "Keypad8"},
    KeyName{.key = engine::Key::Keypad9, .id = "Keypad9"},
    KeyName{.key = engine::Key::KeypadDecimal, .id = "KeypadDecimal"},
    KeyName{.key = engine::Key::KeypadDivide, .id = "KeypadDivide"},
    KeyName{.key = engine::Key::KeypadMultiply, .id = "KeypadMultiply"},
    KeyName{.key = engine::Key::KeypadSubtract, .id = "KeypadSubtract"},
    KeyName{.key = engine::Key::KeypadAdd, .id = "KeypadAdd"},
    KeyName{.key = engine::Key::KeypadEnter, .id = "KeypadEnter"},
    KeyName{.key = engine::Key::KeypadEqual, .id = "KeypadEqual"},
};

/**
 * @brief Gets the key printed at the position of a QWERTY key on an AZERTY keyboard.
 * @param key Key, named after its QWERTY position.
 * @return The key whose QWERTY name is the AZERTY label, or `key` when both layouts agree.
 */
engine::Key azertyLabelKey(engine::Key key) noexcept {
    switch (key) {
        case engine::Key::A:
            return engine::Key::Q;
        case engine::Key::Q:
            return engine::Key::A;
        case engine::Key::W:
            return engine::Key::Z;
        case engine::Key::Z:
            return engine::Key::W;
        case engine::Key::Semicolon:
            return engine::Key::M;
        case engine::Key::M:
            return engine::Key::Comma;
        default:
            return key;
    }
}

}  // namespace

std::span<const KeyName> keyNames() noexcept { return KEY_NAMES; }

std::string_view keyId(engine::Key key) noexcept {
    // NOLINTNEXTLINE(readability-qualified-auto): std::array iterator is not a pointer on MSVC.
    const auto found = std::ranges::find(KEY_NAMES, key, &KeyName::key);
    return found != KEY_NAMES.end() ? found->id : std::string_view{};
}

std::optional<engine::Key> keyFromId(std::string_view id) noexcept {
    // NOLINTNEXTLINE(readability-qualified-auto): std::array iterator is not a pointer on MSVC.
    const auto found = std::ranges::find(KEY_NAMES, id, &KeyName::id);
    if (found == KEY_NAMES.end()) {
        return std::nullopt;
    }
    return found->key;
}

std::string keyLabel(engine::Key key, KeyboardLayout layout) {
    if (layout == KeyboardLayout::Azerty) {
        key = azertyLabelKey(key);
    }
    const std::string_view id = keyId(key);
    if (id.empty()) {
        return "NONE";
    }
    // "LeftShift" -> "LEFT SHIFT": a space before each upper-case letter that follows a lower-case one.
    std::string label;
    for (std::size_t i = 0; i < id.size(); ++i) {
        const auto c = static_cast<unsigned char>(id[i]);
        if (i > 0 && std::isupper(c) != 0 && std::islower(static_cast<unsigned char>(id[i - 1])) != 0) {
            label.push_back(' ');
        }
        label.push_back(static_cast<char>(std::toupper(c)));
    }
    return label;
}

}  // namespace input
