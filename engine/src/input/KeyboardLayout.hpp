#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace input {

/**
 * @brief Supported hardware keyboard layouts for game input bindings.
 */
enum class KeyboardLayout : std::uint8_t {
    /** @brief Standard US / international QWERTY layout (WASD movement). */
    Qwerty,
    /** @brief French / Belgian AZERTY layout (ZQSD movement). */
    Azerty,
};

/**
 * @brief Detects the active physical keyboard layout of the host operating system.
 *
 * Evaluates native platform configuration (Win32 active layout API, Linux /etc/default/keyboard)
 * and locale environment variables (LC_ALL, LC_CTYPE, LC_MESSAGES, LANG). Defaults to Qwerty.
 *
 * @return Detected KeyboardLayout enum value.
 */
[[nodiscard]] KeyboardLayout detectKeyboardLayout() noexcept;

/**
 * @brief Converts a KeyboardLayout enum value to its human-readable uppercase string.
 *
 * @param layout Target layout enum value.
 * @return String representation ("QWERTY", "AZERTY", or "UNKNOWN").
 */
[[nodiscard]] const char* keyboardLayoutToString(KeyboardLayout layout) noexcept;

namespace detail {

/**
 * @brief Evaluates whether a POSIX locale string corresponds to an AZERTY region.
 *
 * @param locale Locale identifier string (e.g. "fr_FR.UTF-8", "fr_BE").
 * @return True if locale uses AZERTY by convention, false otherwise.
 */
[[nodiscard]] bool isAzertyLocale(std::string_view locale) noexcept;

/**
 * @brief Attempts to detect keyboard layout from Linux system configuration files.
 *
 * @return Detected layout or std::nullopt if configuration file is unavailable or missing.
 */
[[nodiscard]] std::optional<KeyboardLayout> detectFromLinuxConfig() noexcept;

/**
 * @brief Attempts to detect keyboard layout via native platform APIs (Win32 or Linux config).
 *
 * @return Detected layout or std::nullopt if unavailable or undetermined.
 */
[[nodiscard]] std::optional<KeyboardLayout> detectFromPlatform() noexcept;

/**
 * @brief Attempts to detect keyboard layout by inspecting environment locale variables.
 *
 * @return Detected layout or std::nullopt if undetermined.
 */
[[nodiscard]] std::optional<KeyboardLayout> detectFromEnvironment() noexcept;

}  // namespace detail

}  // namespace input
