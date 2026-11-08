/**
 * @file PlayerLives.hpp
 * @brief The lives of the local player, read from its replicated ship, and the sheet the HUD draws them with.
 */

#pragma once

#include <cstdint>

#include "components/Renderable.hpp"
#include "ecs/Registry.hpp"

namespace client::gameplay {

/** @brief Lives a player starts with, shown until the first snapshot gives the real count. */
inline constexpr std::uint8_t STARTING_LIVES = 3;

/**
 * @brief Registry resource: the player ship spritesheet, for the lives of the HUD. Set by `RTypeGamePlugin`.
 *
 * One row per player color, `frames` frames per row; the middle one is the ship at rest.
 */
struct ShipIconSheet {
    /** @brief Texture of the sheet. */
    ecs::TextureId texture = ecs::NO_TEXTURE;
    /** @brief Width of a frame, in texture pixels. */
    int frameWidth = 0;
    /** @brief Height of a frame (and of a row), in texture pixels. */
    int frameHeight = 0;
    /** @brief Frames per row. */
    int frames = 1;
};

/**
 * @brief Gives the frame of the ship at rest of a player, for a life icon.
 * @param sheet The sheet.
 * @param playerIndex Index of the player: the row of its color.
 * @return The middle frame of its row.
 */
[[nodiscard]] ecs::TextureRect idleShipFrame(const ShipIconSheet& sheet, std::uint8_t playerIndex) noexcept;

/** @brief What the HUD shows of the local player. */
struct OwnShipStatus {
    /** @brief Lives left, the current one included. */
    std::uint8_t lives = STARTING_LIVES;
    /** @brief True once the ship lost its last life: the player watches until the end. */
    bool eliminated = false;
};

/**
 * @brief Reads the lives of a player from its replicated ship: its `health`, and its `FLAG_ELIMINATED`.
 * @param registry Client registry.
 * @param playerIndex Index of the player: the `subtype` of its ship.
 * @return Its status; `STARTING_LIVES` while its ship has not arrived yet.
 */
[[nodiscard]] OwnShipStatus ownShipStatus(const ecs::Registry& registry, std::uint8_t playerIndex);

}  // namespace client::gameplay
