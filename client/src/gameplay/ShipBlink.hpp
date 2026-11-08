/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** Makes the invulnerable ships blink.
*/

#pragma once

#include <cstdint>

#include "ecs/Registry.hpp"

namespace client::gameplay {

/** @brief Seconds of one blink: visible for the first half, faded for the second. */
inline constexpr float BLINK_PERIOD = 0.2F;
/** @brief Opacity of a blinking entity in the faded half of a blink. */
inline constexpr std::uint8_t BLINK_FADED_ALPHA = 40;
/** @brief Opacity of a visible entity. */
inline constexpr std::uint8_t OPAQUE_ALPHA = 255;

/**
 * @brief Gives the opacity of an entity at some time.
 * @param flags `rtype::game::EntityNetState::flags` of the entity.
 * @param seconds Time since any fixed origin, shared by all the entities so they blink together.
 * @return `BLINK_FADED_ALPHA` in the faded half of a blink of an entity with `FLAG_INVULNERABLE`, else `OPAQUE_ALPHA`.
 */
[[nodiscard]] std::uint8_t blinkAlpha(std::uint8_t flags, float seconds) noexcept;

/**
 * @brief Sets the opacity of every player ship from its flags: the ships that just respawned blink, the others are
 * opaque. The other entities keep their own opacity.
 * @param registry Client registry.
 * @param seconds Time since any fixed origin.
 */
void blinkInvulnerableShips(ecs::Registry& registry, float seconds);

}  // namespace client::gameplay
