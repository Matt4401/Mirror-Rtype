/**
 * @class CarTuning
 * @brief Car physics constants, taken from Rocket League (units: uu, seconds, radians).
 *
 * Edit and save while the server runs: hot-reload picks the change up.
 */
class CarTuning {
    // --- Simulation ---
    /** @brief Physics step: the match is simulated at 120 Hz, like Rocket League, whatever the frame rate. */
    static STEP { 1 / 120 }
    /** @brief Longest time simulated in one frame (avoids a spiral of death after a freeze). */
    static MAX_FRAME_TIME { 0.25 }
    /** @brief Stick values below this are ignored. */
    static DEADZONE { 0.15 }

    // --- Hitbox (Octane: 118 x 36 uu seen from the side) ---
    static HALF_LENGTH { 59 }
    static HALF_HEIGHT { 18 }

    // --- World ---
    static GRAVITY { 650 }
    /** @brief No car goes faster than this (2300 uu/s in Rocket League, reached with boost or gravity). */
    static MAX_SPEED { 2300 }

    // --- Driving ---
    /** @brief Top speed reachable with the throttle alone (no boost). */
    static MAX_DRIVE_SPEED { 1410 }
    /** @brief Braking deceleration when steering against the motion. */
    static BRAKE { 3500 }
    /** @brief Deceleration when no input is given (rolling friction). */
    static COAST { 525 }
    /** @brief Below this speed, steering the other way turns the car around instead of braking. */
    static TURN_SPEED { 200 }
    /** @brief Pull toward the surface while the wheels touch it: lets the car drive on walls. */
    static STICKY { 325 }
    /** @brief How far the car can move away from a surface before its wheels lose contact. */
    static SUSPENSION_TRAVEL { 20 }
    /** @brief Speed at which a compressed suspension relaxes, in uu/s. */
    static SUSPENSION_RECOVERY { 200 }
    /** @brief How fast the car body follows the surface orientation, per second. */
    static ALIGN_RATE { 30 }

    /** @brief Throttle acceleration at a given speed: 1600 uu/s² at rest, 160 at 1400 uu/s, 0 at 1410. */
    static throttleAccel(speed) {
        if (speed < 1400) return 1600 + (160 - 1600) * speed / 1400
        if (speed < CarTuning.MAX_DRIVE_SPEED) {
            return 160 * (CarTuning.MAX_DRIVE_SPEED - speed) / (CarTuning.MAX_DRIVE_SPEED - 1400)
        }
        return 0
    }

    // --- Boost ---
    /** @brief Boost tank size, and its content when the car appears (Sideswipe starts full). */
    static BOOST_MAX { 100 }
    static BOOST_START { 100 }
    /** @brief Boost burnt per second while boosting. */
    static BOOST_USE_RATE { 100 / 3 }
    /**
     * @brief Boost gained per second while the car touches a surface (Sideswipe has no boost pads). Not a published
     * value: an estimate of Sideswipe's refill (empty to full in about 4 s).
     */
    static BOOST_REFILL_RATE { 25 }
    /** @brief Acceleration given by boosting, toward the nose. */
    static BOOST_ACCEL_GROUND { 2975 / 3 }
    static BOOST_ACCEL_AIR { 3175 / 3 }
    /** @brief A boost, once started, lasts at least this long. */
    static BOOST_MIN_TIME { 0.1 }

    // --- Jumping ---
    /** @brief Velocity given instantly by a jump, along the car roof. */
    static JUMP_IMPULSE { 292 }
    /** @brief Extra acceleration while the jump button stays held... */
    static JUMP_HOLD_ACCEL { 1458 }
    /** @brief ...for at most this long. */
    static JUMP_HOLD_TIME { 0.2 }
    /** @brief The second jump (double jump or dodge) is available this long after the first jump. */
    static SECOND_JUMP_WINDOW { 1.25 }
    /** @brief The car cannot land again right after jumping. */
    static JUMP_LAND_DELAY { 0.1 }

    // --- Dodges (flips) ---
    /** @brief Velocity given by a front dodge, increased with the current speed. */
    static DODGE_IMPULSE { 500 }
    /** @brief Velocity given by a back dodge. */
    static BACK_DODGE_IMPULSE { 533 }
    /** @brief While dodging, the car spins at full speed and ignores air control. */
    static DODGE_TIME { 0.65 }
    /** @brief Share of the upward speed kept when dodging (a falling car stops falling). */
    static DODGE_VERTICAL_KEEP { 0.5 }

    // --- Air control ---
    /** @brief Angular acceleration given by the stick in the air (Rocket League pitch). */
    static AIR_TORQUE { 12.46 }
    /** @brief Angular damping when the stick is released. */
    static AIR_DAMPING { 2.798 }
    /** @brief Fastest rotation of a car. */
    static MAX_ANGULAR_SPEED { 5.5 }

    // --- Contacts other than the wheels ---
    /** @brief Landing on the wheels needs the roof within ~60° of the surface normal (cos 60° = 0.5). */
    static LAND_ALIGN { 0.5 }
    /** @brief A car resting against a wall still lands if it moves away from it slower than this (uu/s). */
    static LAND_MAX_SEPARATION { 50 }
    /** @brief Bounce of the body (roof, nose) against a wall. */
    static RESTITUTION { 0.1 }
    /** @brief Friction of the body sliding against a wall, in uu/s². */
    static BODY_FRICTION { 1200 }
    /** @brief How fast a car lying on its side tips over, per second. */
    static TIP_RATE { 6 }
    /** @brief Time a car on its roof takes to roll back on its wheels after pressing jump. */
    static RECOVERY_TIME { 0.55 }
}
