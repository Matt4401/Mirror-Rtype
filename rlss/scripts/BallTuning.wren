/**
 * @class BallTuning
 * @brief Ball and car-ball contact constants, taken from Rocket League (as measured by RocketSim; units: uu, s, rad).
 *
 * Edit and save while the server runs: hot-reload picks the change up.
 */
class BallTuning {
    // --- Ball ---
    static RADIUS { 91.25 }
    /** @brief Masses only matter as a ratio: a car weighs six balls. */
    static MASS { 30 }
    static GRAVITY { 650 }
    /** @brief Air drag: the ball loses 3% of its speed per second. */
    static DRAG { 0.03 }
    /** @brief Share of the speed into a wall kept after a bounce. */
    static RESTITUTION { 0.6 }
    /** @brief Wall friction: turns sliding into spin, and spin into sliding. */
    static FRICTION { 0.35 }
    /** @brief Below this speed into a wall, the ball stops bouncing and rolls. */
    static REST_SPEED { 15 }
    static MAX_SPEED { 6000 }
    static MAX_ANGULAR_SPEED { 6 }
    /** @brief Moment of inertia of a solid sphere, as a share of mass * radius². */
    static INERTIA_FACTOR { 0.4 }

    // --- Car against ball ---
    static CAR_MASS { 180 }
    /** @brief The contact itself does not bounce: the punch comes from the extra impulse below. */
    static CAR_RESTITUTION { 0 }
    static CAR_FRICTION { 2 }
    /**
     * @brief Rocket League adds an impulse to the ball on each touch (at most every other step): along the line from
     * the car center to the ball center, vertical part scaled by 0.35, part along the car nose scaled by 0.65, with a
     * strength that is a share of the relative speed (capped at 4600 uu/s).
     */
    static HIT_VERTICAL_SCALE { 0.35 }
    static HIT_FORWARD_SCALE { 0.65 }
    static HIT_MAX_RELATIVE_SPEED { 4600 }

    /** @brief Share of the relative speed given by the extra impulse: 0.65 up to 500 uu/s, 0.55 at 2300, 0.3 at 4600. */
    static hitFactor(relativeSpeed) {
        if (relativeSpeed <= 500) return 0.65
        if (relativeSpeed <= 2300) return 0.65 + (0.55 - 0.65) * (relativeSpeed - 500) / 1800
        return 0.55 + (0.3 - 0.55) * ((relativeSpeed - 2300) / 2300).min(1)
    }
}
