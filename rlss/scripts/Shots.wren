/**
 * @class Shots
 * @brief Sideswipe's colored shots and resets: which part of the car touched the ball, and what the car was doing.
 *
 * - Red (power shot): flipping into the ball with the nose. The most powerful and the easiest.
 * - Purple (underside shot): the wheels against the ball, then a jump without direction: the car jumps off the ball
 *   and pushes it away. Holding a direction flips instead and misses the ball. A bit less powerful than red.
 * - Gold (back shot): flipping so that the rear swings into the ball (rear toward the ball, flip away from it): a lob
 *   that sends the ball up and back, over an opponent.
 *
 * The ball then glows with the shot color, until another touch or `GLOW_TIME`. Touching the ball with the wheels resets
 * the flip (the car flashes white until the flip is used) and refills boost: at least `BALL_BOOST_RESET` on a new
 * touch, more while the contact lasts.
 *
 * Sideswipe's values are not published: the impulses below are estimates.
 */
class Shots {
    static NONE { 0 }
    static RED { 1 }
    static PURPLE { 2 }
    static GOLD { 3 }

    /** @brief Extra speed given to the ball by a red shot, on top of the hit. */
    static RED_IMPULSE { 700 }
    /** @brief Speed given to the ball by a purple shot, away from the car's wheels. */
    static PURPLE_IMPULSE { 900 }
    /** @brief Speed given to the ball by a gold shot, up and toward the rear of the car. */
    static GOLD_IMPULSE { 900 }
    /** @brief Direction of a gold shot: this much toward the rear for one upward. */
    static GOLD_BACK { 0.5 }
    /** @brief A flip touching the ball with another part (roof, a corner) still adds this much. */
    static OTHER_FLIP_IMPULSE { 250 }
    /** @brief The purple jump must come within this long of the wheels touching the ball. */
    static PURPLE_WINDOW { 0.15 }
    /** @brief The ball glows this long after a colored shot (unless touched again). */
    static GLOW_TIME { 3 }
    /** @brief Boost given by a new touch of the ball with the wheels (a touch after `BALL_TOUCH_GAP` without). */
    static BALL_BOOST_RESET { 25 }
    static BALL_TOUCH_GAP { 0.2 }
    /** @brief A contact normal within this of a car side counts as that side (cos 50°). */
    static FACE_ALIGN { 0.64 }

    /**
     * @brief Part of the car touching the ball, from the contact normal in the car frame (along the car, toward the
     * roof) and the car facing: "nose", "rear", "wheels", "roof", or "corner".
     */
    static face(normalAlong, normalUp, facing) {
        var forward = normalAlong * facing
        if (forward >= Shots.FACE_ALIGN) return "nose"
        if (forward <= -Shots.FACE_ALIGN) return "rear"
        if (normalUp <= -Shots.FACE_ALIGN) return "wheels"
        if (normalUp >= Shots.FACE_ALIGN) return "roof"
        return "corner"
    }
}
