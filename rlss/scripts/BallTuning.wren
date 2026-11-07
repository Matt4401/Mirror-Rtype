/**
 * @class BallTuning
 * @brief Ball and car-ball contact constants (units: uu, s, rad).
 *
 * Based on Rocket League as measured by RocketSim, adjusted toward Sideswipe: a floaty ball, a capped ball
 * speed, a ball that stops rolling, hits whose power comes from the speed along the contact and mostly from flips
 * (the colored shots, see `Shots`).
 * Values marked "estimate" are tuned by hand: Sideswipe's are not published.
 *
 * Edit and save while the server runs: hot-reload picks the change up.
 */
class BallTuning {
    // --- Ball ---
    /** @brief A little smaller than Rocket League's 91.25. Keep `rlss::BALL_RADIUS` (shared/Arena.hpp) the same. */
    static RADIUS { 80 }
    /** @brief Masses only matter as a ratio: a car weighs six balls. */
    static MASS { 30 }
    /**
     * @brief Always toward the bottom of the screen. Estimate: the ball floats more than the cars (900 uu/s²), and a
     * bit more than in Rocket League (650 uu/s²): it stays in the air long enough to be played.
     */
    static GRAVITY { 600 }
    /** @brief Air drag: the ball loses 3% of its speed per second. */
    static DRAG { 0.03 }
    /** @brief Share of the speed into a wall kept after a bounce. */
    static RESTITUTION { 0.6 }
    /** @brief Wall friction: turns sliding into spin, and spin into sliding. */
    static FRICTION { 0.35 }
    /** @brief Below this speed into a wall, the ball stops bouncing and rolls. */
    static REST_SPEED { 15 }
    /**
     * @brief Fastest ball. Estimate: well below Rocket League's 6000 uu/s, so that even a boosted flip shot stays under
     * a ceiling, readable on screen.
     */
    static MAX_SPEED { 2600 }
    /** @brief Deceleration of a ball rolling on a surface. Estimate: a rolling ball stops after a few seconds. */
    static ROLLING_RESISTANCE { 300 }
    static MAX_ANGULAR_SPEED { 6 }
    /** @brief A bounce faster than this into the wall (uu/s) makes a sound; a ball settling down stays quiet. */
    static BOUNCE_SOUND_SPEED { 100 }
    /** @brief Moment of inertia of a solid sphere, as a share of mass * radius². */
    static INERTIA_FACTOR { 0.4 }

    // --- Car against ball ---
    static CAR_MASS { 180 }
    /**
     * @brief Bounce of the contact itself, on the speed along the contact normal: the front of a car at full speed
     * sends the ball much further than its roof brushing past. Estimate.
     */
    static CAR_RESTITUTION { 0.1 }
    static CAR_FRICTION { 2 }
    /**
     * @brief Like Rocket League, a touch adds an impulse to the ball (at most every other step), along the line from the
     * car center to the ball center, vertical part scaled by 0.35, part along the car nose scaled by 0.65. Its strength
     * is a share of the speed along the contact (capped at 4600 uu/s): a hit, not a brush, gives it.
     */
    static HIT_VERTICAL_SCALE { 0.35 }
    static HIT_FORWARD_SCALE { 0.65 }
    static HIT_MAX_RELATIVE_SPEED { 4600 }
    /** @brief A hit makes a sound when the car and the ball meet faster than this (uu/s), or when it is a colored shot. */
    static HIT_SOUND_SPEED { 150 }

    /** @brief Scale of the extra impulse below (1 in Rocket League). Estimate: softer hits, a slower ball. */
    static HIT_STRENGTH { 0.7 }

    /** @brief Share of the contact speed given by the extra impulse: 0.65 up to 500 uu/s, 0.55 at 2300, 0.3 at 4600. */
    static hitFactor(relativeSpeed) {
        if (relativeSpeed <= 500) return 0.65
        if (relativeSpeed <= 2300) return 0.65 + (0.55 - 0.65) * (relativeSpeed - 500) / 1800
        return 0.55 + (0.3 - 0.55) * ((relativeSpeed - 2300) / 2300).min(1)
    }
}
