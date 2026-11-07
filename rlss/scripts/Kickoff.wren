import "rlss/scripts/CarTuning" for CarTuning

/**
 * @class Kickoff
 * @brief Kickoff spots and the timing of a goal, shared by `Match` (resets) and `Car` (a new car joins at its spot).
 *
 * Even player slots form the blue team: they start on the left, facing right, and defend the left goal. Odd slots form
 * the orange team, mirrored. Teammates line up one behind the other.
 */
class Kickoff {
    /** @brief Celebration after a goal, before everybody is put back in place (the ball stays in play meanwhile). */
    static GOAL_CELEBRATION { 2 }
    /** @brief Kickoff countdown, cars and ball frozen (3, 2, 1, like Rocket League). */
    static COUNTDOWN { 3 }
    /** @brief Distance from a side wall to the first kickoff spot: clear of the curved corner. */
    static MARGIN { 420 }
    /** @brief Distance between teammates. */
    static SPACING { 160 }
    /** @brief Closest a car starts to the center line. */
    static CENTER_GAP { 150 }

    static team(playerIndex) { playerIndex % 2 }

    /** @brief Kickoff spot of a player: [x, y, facing], wheels on the floor. */
    static spot(arena, playerIndex) {
        var rank = (playerIndex / 2).floor
        var offset = Kickoff.MARGIN + rank * Kickoff.SPACING
        var y = arena.bottom - CarTuning.HALF_HEIGHT
        if (team(playerIndex) == 0) return [(arena.left + offset).min(arena.centerX - Kickoff.CENTER_GAP), y, 1]
        return [(arena.right - offset).max(arena.centerX + Kickoff.CENTER_GAP), y, -1]
    }
}
