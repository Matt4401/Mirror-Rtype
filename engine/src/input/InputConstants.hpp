#pragma once

#include <cstddef>
#include <cstdint>

namespace input {

/** @brief FNV-1a 32-bit initial offset basis (0x811C9DC5). */
inline constexpr std::uint32_t FNV1A_OFFSET_BASIS_32 = 2166136261U;

/** @brief FNV-1a 32-bit prime multiplier (0x01000193). */
inline constexpr std::uint32_t FNV1A_PRIME_32 = 16777619U;

/** @brief Maximum number of alternative keyboard keycodes mapped to a single action. */
inline constexpr std::size_t MAX_KEY_BINDINGS = 4;

/** @brief Maximum number of gamepad buttons mapped to a single action. */
inline constexpr std::size_t MAX_PAD_BUTTON_BINDINGS = 2;

/** @brief Default analog stick deadzone to filter hardware noise. */
inline constexpr float DEFAULT_DEADZONE = 0.2F;

/** @brief Windows Primary Language Identifier for French (France) AZERTY layout. */
inline constexpr std::uint16_t WIN_LANGID_FRENCH_FRANCE = 0x040CU;

/** @brief Windows Primary Language Identifier for French (Belgium) AZERTY layout. */
inline constexpr std::uint16_t WIN_LANGID_FRENCH_BELGIUM = 0x080CU;

}  // namespace input
