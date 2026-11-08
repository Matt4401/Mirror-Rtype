import "core/Input" for Input
import "rlss/components" for Transform, Controls, CarStatus, CarCues
import "rlss/scripts/Arena" for Arena
import "rlss/scripts/CarTuning" for CarTuning
import "rlss/scripts/Kickoff" for Kickoff
import "rlss/scripts/Shots" for Shots
import "rlss/scripts/Simulation" for Simulation

/**
 * @class Car
 * @brief Server-side physics of one car, seen from the side, modelled on Rocket League.
 *
 * Attached to every car entity (`ScriptComponent`). The instance registers itself in `Simulation`; the `Match` script
 * steps every car and the ball together at a fixed 120 Hz (`step(dt)`), resolves their collisions, then calls `sync()`.
 * Reads the player's input through `Input.getAxis(player, "steer" | "pitch")` / `Input.isDown(player, "jump" | "boost")`,
 * and
 * writes the result into the entity `Transform` (x, y, rotation; `scaleX` = -1 when facing left) and `CarStatus`
 * (boost), which the engine replicates.
 *
 * Orientation: `theta` is the car rotation, clockwise on screen. Along the car, the tangent is (cos, sin) and the roof
 * (car "up") is (sin, -cos); the nose is the tangent times `facing` (+1 or -1). Turning around mirrors the car
 * (Sideswipe style) instead of rotating it.
 *
 * Two states:
 * - on its wheels (`grounded`): the car follows the surface (floor, curved corners, walls, ceiling). Gravity along the
 *   surface slows it on a climb; the sticky force (325 uu/s², 1050 while driving) and the push of curved corners keep
 *   it on the surface, gravity pulls it away. When the pull wins long enough for the suspension to extend fully, the wheels lose contact.
 *   While driving (throttle or boost) the grip beats gravity, so the car drives upside down on the ceiling; once it
 *   lets go, it falls.
 * - in the air: gravity, jump hold, one double jump or dodge per jump (until the car touches a surface again), air
 *   control; the body collides with the arena walls. The stick (steer and pitch) aims: the nose turns toward where it
 *   points on screen (up, down, left, right or in between), at most 4.2 rad/s, slowing down as it gets there. Pointing
 *   the nose up and boosting climbs. Like in Sideswipe, the car never rests on its roof or its side:
 *   just before touching a surface any other way than wheels first, it turns itself over and lands on its wheels.
 *
 * Boost pushes the car toward its nose (991 uu/s² on the ground, 1058 in the air) and burns 33.3 per second. Like in
 * Sideswipe, there are no boost pads: the tank refills while the car touches a surface.
 *
 * Sounds: every jump and every hard contact with the arena (a landing, the body hitting a wall) is counted in the
 * entity `CarCues`, which the engine replicates; the clients play a sound when a counter changes.
 */
class Car {
    construct new(entity) {
        _id = entity is Num ? entity : entity.id
        _transform = Transform.new(_id)
        _status = CarStatus.new(_id)
        _cues = CarCues.new(_id)
        _player = Controls.new(_id).playerIndex
        _arena = Arena.fromWorld()

        _x = _transform.x
        _y = _transform.y
        _theta = _transform.rotation
        _facing = _transform.scaleX < 0 ? -1 : 1
        _vx = 0
        _vy = 0
        _omega = 0

        _grounded = false
        _separation = 0
        _normalSpeed = 0
        _steerSign = _facing
        _steerLocked = false

        _jumpHold = 0
        _jumpDirX = 0
        _jumpDirY = 0
        _hasJumped = false
        _secondJumpUsed = false
        _airTime = 0
        _sinceJump = 1
        _spinTime = 0
        _spin = 0
        _dodging = false
        _prevJump = false

        _contact = false
        _contactAlign = 0
        _contactNx = 0
        _contactNy = -1

        _boost = CarTuning.BOOST_START
        _boosting = false
        _boostTime = 0
        _lastBallHit = -10
        // Wheels against the ball: time since, and the direction from the car to the ball then.
        _ballWheelTime = 10
        _ballNx = 0
        _ballNy = 0
        _flipReset = false
        _purpleShot = null
        _ballJumpCooldown = 0

        // Sound cues: the counters go on from the component (a hot-reload must not replay the last sounds).
        _jumps = Car.counter_(_cues.jumps)
        _jumpKind = Car.JUMP
        _impacts = Car.counter_(_cues.impacts)
        _impactSpeed = 0

        // A car that just joined starts at its kickoff spot.
        if (_status.placeAtKickoff == true) {
            _status.placeAtKickoff = false
            placeAtKickoff()
        }
        Simulation.addCar(this)
    }

    /** @brief `CarCues.jumpKind` of a first jump (or rolling back onto the wheels). */
    static JUMP { 0 }
    /** @brief `CarCues.jumpKind` of a second jump without direction, or of a jump off the ball. */
    static DOUBLE_JUMP { 1 }
    /** @brief `CarCues.jumpKind` of a flip. */
    static DODGE { 2 }

    /** @brief Jumps made so far, and the kind of the last one (`JUMP`, `DOUBLE_JUMP`, `DODGE`). */
    jumps { _jumps }
    jumpKind { _jumpKind }
    /** @brief Hard contacts with the arena so far, and the speed into the surface of the last one. */
    impacts { _impacts }
    impactSpeed { _impactSpeed }

    /** @brief Back to the kickoff spot of this player: at rest, wheels down, full boost. */
    placeAtKickoff() {
        if (_arena == null) _arena = Arena.fromWorld()
        if (_arena == null || _player == null) return
        var spot = Kickoff.spot(_arena, _player)
        _x = spot[0]
        _y = spot[1]
        _facing = spot[2]
        _theta = 0
        _vx = 0
        _vy = 0
        _omega = 0
        _grounded = true
        _separation = 0
        _normalSpeed = 0
        _steerSign = _facing
        _steerLocked = false
        _jumpHold = 0
        _hasJumped = false
        _secondJumpUsed = false
        _spinTime = 0
        _dodging = false
        _flipReset = false
        _contact = false
        _boost = CarTuning.BOOST_START
        _boosting = false
        _boostTime = 0
        sync()
    }

    onDestroy() { Simulation.removeCar(this) }

    // --- State read and pushed by the match (collisions), tests and debugging ---
    x { _x }
    y { _y }
    theta { _theta }
    omega { _omega }
    vx { _vx }
    vy { _vy }
    facing { _facing }
    grounded { _grounded }
    speed { (_vx * _vx + _vy * _vy).sqrt }
    boost { _boost }
    /** @brief Sets the boost left (clamped to the tank). */
    boost=(value) { _boost = value.clamp(0, CarTuning.BOOST_MAX) }
    boosting { _boosting }
    /** @brief True during a dodge (flip): hitting the ball now gives it a strong extra push (see `Match.hit`). */
    dodging { _dodging }
    /** @brief True after a flip reset on the ball, until the flip is used or the car lands: the car flashes white. */
    flipReset { _flipReset }

    /**
     * @brief The wheels touch the ball (called by the match at each step of contact): the flip comes back (a flip
     * reset), and boost refills, at least `Shots.BALL_BOOST_RESET` on a new touch.
     * @param nx Direction from the car to the ball.
     */
    touchBallWithWheels(nx, ny) {
        // Right after a jump or a flip the wheels may still touch the ball: that is not a new touch.
        if (_grounded || _ballJumpCooldown > 0) return
        if (_ballWheelTime > Shots.BALL_TOUCH_GAP) {
            _boost = (_boost + Shots.BALL_BOOST_RESET).min(CarTuning.BOOST_MAX)
        }
        if (_secondJumpUsed || _hasJumped) _flipReset = true
        _secondJumpUsed = false
        _ballWheelTime = 0
        _ballNx = nx
        _ballNy = ny
    }

    /** @brief The direction of a purple shot this car just made (car to ball), once, or null. */
    takePurpleShot() {
        var shot = _purpleShot
        _purpleShot = null
        return shot
    }
    /** @brief Step of the last extra impulse this car gave the ball (see `Match.hit`). */
    lastBallHit { _lastBallHit }
    lastBallHit=(value) { _lastBallHit = value }
    /** @brief Nose direction (unit vector): where boost and front dodges push. */
    noseX { _facing * _theta.cos }
    noseY { _facing * _theta.sin }

    /** @brief Changes the velocity (collision impulse divided by the car mass). */
    addVelocity(dx, dy) {
        _vx = _vx + dx
        _vy = _vy + dy
    }

    /** @brief Moves the car (collision push-out); on the wheels, the surface puts it back at ride height. */
    move(dx, dy) {
        _x = _x + dx
        _y = _y + dy
    }

    /** @brief Whether the entity and the arena are there to simulate. */
    ready {
        if (_player == null || _transform.x == null) return false
        if (_arena == null) _arena = Arena.fromWorld()
        return _arena != null
    }

    /** @brief One physics step, called by the match. */
    step(dt) {
        var steer = Input.getAxis(_player, "steer")
        if (steer == null || steer.abs < CarTuning.DEADZONE) steer = 0
        steer = steer.clamp(-1, 1)
        var pitch = Input.getAxis(_player, "pitch")
        if (pitch == null || pitch.abs < CarTuning.DEADZONE) pitch = 0
        pitch = pitch.clamp(-1, 1)
        var jump = Input.isDown(_player, "jump")
        var jumpPressed = jump && !_prevJump
        _prevJump = jump
        _sinceJump = _sinceJump + dt
        _ballWheelTime = _ballWheelTime + dt
        _ballJumpCooldown = (_ballJumpCooldown - dt).max(0)
        updateBoost_(dt, Input.isDown(_player, "boost"))

        if (_grounded) {
            drive_(dt, steer, jumpPressed)
        } else {
            fly_(dt, steer, pitch, jump, jumpPressed)
        }
    }

    /** @brief Publishes the state to the ECS, once per frame. */
    sync() {
        _transform.x = _x
        _transform.y = _y
        _transform.rotation = _theta
        _transform.scaleX = _facing
        _status.boost = _boost
        _status.boosting = _boosting
        _status.flipReset = _flipReset
        _cues.jumps = _jumps
        _cues.jumpKind = _jumpKind
        _cues.impacts = _impacts
        _cues.impactSpeed = _impactSpeed
    }

    // A jump of the given kind, for the sounds.
    jumped_(kind) {
        _jumps = _jumps + 1
        _jumpKind = kind
    }

    // A contact with the arena at `speed` into the surface: a sound if it is hard enough.
    impact_(speed) {
        if (speed < CarTuning.IMPACT_SOUND_SPEED) return
        _impacts = _impacts + 1
        _impactSpeed = speed
    }

    // ------------------------------------------------------------------ boost

    updateBoost_(dt, held) {
        // Once started, a boost lasts at least BOOST_MIN_TIME, like in Rocket League.
        if ((held || (_boosting && _boostTime < CarTuning.BOOST_MIN_TIME)) && _boost > 0) {
            _boostTime = _boosting ? _boostTime + dt : 0
            _boosting = true
            _boost = (_boost - CarTuning.BOOST_USE_RATE * dt).max(0)
        } else {
            _boosting = false
            _boostTime = 0
            // Sideswipe has no boost pads: the tank refills while the car touches a surface.
            var touching = _grounded || _contact || _ballWheelTime < 2 * CarTuning.STEP
            if (touching) _boost = (_boost + CarTuning.BOOST_REFILL_RATE * dt).min(CarTuning.BOOST_MAX)
        }
    }

    // ------------------------------------------------------------------ on the wheels

    drive_(dt, steer, jumpPressed) {
        var arena = _arena
        arena.sample(_x, _y)
        var nx = arena.nx
        var ny = arena.ny
        var tx = -ny
        var ty = nx
        var s = _vx * tx + _vy * ty

        // The stick is screen-relative, the surface goes all around the arena: keep the direction the stick had when
        // it was pushed until it is released, so holding "right" drives up the right wall and along the ceiling.
        if (steer == 0) _steerLocked = false
        if (!_steerLocked && tx.abs > 0.35) _steerSign = tx > 0 ? 1 : -1
        if (steer != 0) _steerLocked = true
        // Boosting means full throttle forward, unless the stick asks for the other way: then the car turns around.
        var throttle = steer != 0 ? steer * _steerSign : (_boosting ? _facing : 0)

        if (throttle != 0 && throttle.sign != _facing && s.abs < CarTuning.TURN_SPEED) _facing = throttle.sign

        if (throttle != 0 && (s.abs < 1 || throttle.sign == s.sign)) {
            s = s + throttle.sign * CarTuning.throttleAccel(s.abs) * throttle.abs * dt
        } else if (throttle != 0) {
            s = Car.towardZero_(s, CarTuning.BRAKE * dt)
            // Stopped within this step: start driving the other way (otherwise, on a wall, gravity would bring back a
            // little sliding every step and the car would brake forever).
            if (s == 0) s = throttle.sign * CarTuning.throttleAccel(0) * throttle.abs * dt
        } else {
            s = Car.towardZero_(s, CarTuning.COAST * dt)
        }
        if (_boosting) s = s + _facing * CarTuning.BOOST_ACCEL_GROUND * dt
        s = (s + CarTuning.GRAVITY * ty * dt).clamp(-CarTuning.MAX_SPEED, CarTuning.MAX_SPEED)

        // Does the surface still hold the car? Gravity pulls it off the ceiling, curved corners push it in.
        var sticky = throttle != 0 ? CarTuning.DRIVING_STICKY : CarTuning.STICKY
        var press = -CarTuning.GRAVITY * ny + s * s * arena.curvature + sticky
        if (press >= 0) {
            _normalSpeed = 0
            _separation = (_separation - CarTuning.SUSPENSION_RECOVERY * dt).max(0)
        } else {
            _normalSpeed = _normalSpeed - press * dt
            _separation = _separation + _normalSpeed * dt
        }
        if (_separation > CarTuning.SUSPENSION_TRAVEL) {
            _vx = s * tx + _normalSpeed * nx
            _vy = s * ty + _normalSpeed * ny
            leaveGround_(false)
            return
        }

        // Move along the surface, turn the body toward it, then put the car back at ride height above it.
        var px = _x + tx * s * dt
        var py = _y + ty * s * dt
        arena.sample(px, py)
        tx = -arena.ny
        ty = arena.nx
        var previous = _theta
        var target = ty.atan(tx)
        _theta = Car.wrap_(_theta + Car.wrap_(target - _theta) * (CarTuning.ALIGN_RATE * dt).min(1))
        _omega = Car.wrap_(_theta - previous) / dt
        _x = px + arena.nx * (rideHeight_(arena) - arena.distance)
        _y = py + arena.ny * (rideHeight_(arena) - arena.distance)
        _vx = s * tx + _normalSpeed * arena.nx
        _vy = s * ty + _normalSpeed * arena.ny
        blockCorners_()

        if (jumpPressed) {
            // Jump along the surface normal: straight up from the floor, away from a wall.
            _vx = _vx + arena.nx * CarTuning.JUMP_IMPULSE
            _vy = _vy + arena.ny * CarTuning.JUMP_IMPULSE
            _jumpDirX = arena.nx
            _jumpDirY = arena.ny
            _jumpHold = CarTuning.JUMP_HOLD_TIME
            _omega = 0
            leaveGround_(true)
            jumped_(Car.JUMP)
        }
    }

    // Distance from the surface to the car center that keeps the whole hitbox above the surface: half the height when
    // the body is aligned, more while it is still tilted after a landing, plus the sag of a curved corner under the car.
    rideHeight_(arena) {
        var hl = CarTuning.HALF_LENGTH
        var c = _theta.cos
        var s = _theta.sin
        var tilted = (hl * (c * arena.nx + s * arena.ny)).abs + (CarTuning.HALF_HEIGHT * (s * arena.nx - c * arena.ny)).abs
        var aligned = CarTuning.HALF_HEIGHT + arena.curvature * hl * hl / 2
        return tilted.max(aligned) + _separation
    }

    leaveGround_(jumped) {
        _grounded = false
        _separation = 0
        _normalSpeed = 0
        _hasJumped = jumped
        _secondJumpUsed = false
        _airTime = 0
        if (jumped) _sinceJump = 0
    }

    // ------------------------------------------------------------------ in the air

    fly_(dt, steer, pitch, jump, jumpPressed) {
        _airTime = _airTime + dt

        // Holding jump keeps pushing for a short while: a longer press jumps higher.
        if (_jumpHold > 0) {
            if (jump) {
                var held = _jumpHold.min(dt)
                _vx = _vx + _jumpDirX * CarTuning.JUMP_HOLD_ACCEL * held
                _vy = _vy + _jumpDirY * CarTuning.JUMP_HOLD_ACCEL * held
                _jumpHold = _jumpHold - dt
            } else {
                _jumpHold = 0
            }
        }

        if (jumpPressed) {
            if (!_secondJumpUsed && steer == 0 && _ballWheelTime < Shots.PURPLE_WINDOW) {
                purpleJump_()
            } else if (_contact && _contactAlign < CarTuning.LAND_ALIGN) {
                recover_()
            } else if (!_secondJumpUsed) {
                // One flip (or double jump) per jump, given back when the car touches a surface again.
                secondJump_(steer)
            }
        }

        selfRight_()
        if (_spinTime > 0) {
            _spinTime = _spinTime - dt
            _omega = _spin
            if (_spinTime <= 0) _dodging = false
        } else {
            airControl_(dt, steer, pitch)
        }

        if (_boosting) {
            _vx = _vx + noseX * CarTuning.BOOST_ACCEL_AIR * dt
            _vy = _vy + noseY * CarTuning.BOOST_ACCEL_AIR * dt
        }
        _vy = _vy + CarTuning.GRAVITY * dt
        limitSpeed_()
        _x = _x + _vx * dt
        _y = _y + _vy * dt
        _theta = Car.wrap_(_theta + _omega * dt)
        collide_(dt)
    }

    // The stick aims: the nose turns toward where it points on screen. The car turns at most MAX_ANGULAR_SPEED, speeds
    // up and slows down at AIR_TORQUE, and plans its stop (AIM_BRAKING) so that it ends on the aim without overshooting.
    // Released, the rotation dies down.
    airControl_(dt, steer, pitch) {
        if (steer == 0 && pitch == 0) {
            _omega = _omega - CarTuning.AIR_DAMPING * _omega * dt
            return
        }
        // Screen y points down: pushing up aims at -y. The nose is the car tangent times `facing`.
        var aim = (-pitch).atan(steer)
        if (_facing < 0) aim = aim + Num.pi
        var error = Car.wrap_(aim - _theta)
        // Aiming right behind: keep turning the way the car already turns, or else lift the nose (over the top).
        if (error.abs > Num.pi - 0.05) error = error.abs * (_omega.abs > 0.1 ? _omega.sign : -_facing)
        var speed = (2 * CarTuning.AIM_BRAKING * error.abs).sqrt.min(CarTuning.AIM_GAIN * error.abs)
        var wanted = error.sign * speed.min(CarTuning.MAX_ANGULAR_SPEED)
        var step = CarTuning.AIR_TORQUE * dt
        _omega = _omega + (wanted - _omega).clamp(-step, step)
    }

    // Sideswipe turns a car over just before it would touch a surface on its roof or its side: when the car flies toward
    // a surface that is close and it is not wheels first, it spins onto its wheels in the time left before the contact.
    selfRight_() {
        _arena.sample(_x, _y)
        var nx = _arena.nx
        var ny = _arena.ny
        var align = _theta.sin * nx - _theta.cos * ny
        if (align > CarTuning.LAND_ALIGN) return
        // Worst case: the car could touch with a corner of its full length.
        var gap = _arena.distance - CarTuning.HALF_LENGTH
        var approach = -(_vx * nx + _vy * ny)
        var timeLeft = gap <= 0 ? 0 : (approach > 1 ? gap / approach : Num.largest)
        if (timeLeft > CarTuning.SELF_RIGHT_TIME && gap > CarTuning.SELF_RIGHT_GAP) return
        var turn = Car.wrap_((nx).atan(-ny) - _theta)
        var duration = timeLeft.clamp(CarTuning.SELF_RIGHT_MIN_TIME, CarTuning.SELF_RIGHT_TIME)
        var maxSpin = CarTuning.SELF_RIGHT_MAX_SPIN
        _spin = (turn / duration).clamp(-maxSpin, maxSpin)
        _spinTime = duration
    }

    // Purple shot: the wheels are against the ball and the player jumps without direction: the car jumps off the ball,
    // which takes the push the other way (applied by the match).
    purpleJump_() {
        _secondJumpUsed = true
        _flipReset = false
        _jumpHold = 0
        _vx = _vx - _ballNx * CarTuning.JUMP_IMPULSE
        _vy = _vy - _ballNy * CarTuning.JUMP_IMPULSE
        _purpleShot = [_ballNx, _ballNy]
        _ballWheelTime = 10
        _ballJumpCooldown = Shots.BALL_TOUCH_GAP
        jumped_(Car.DOUBLE_JUMP)
    }

    // Double jump without direction, dodge (flip) toward the stick otherwise.
    secondJump_(steer) {
        _secondJumpUsed = true
        _flipReset = false
        // Still touching the ball right after using the flip does not give it back: it takes a new touch.
        _ballJumpCooldown = Shots.BALL_TOUCH_GAP
        _jumpHold = 0
        if (steer == 0) {
            _vx = _vx + _theta.sin * CarTuning.JUMP_IMPULSE
            _vy = _vy - _theta.cos * CarTuning.JUMP_IMPULSE
            jumped_(Car.DOUBLE_JUMP)
            return
        }
        jumped_(Car.DODGE)
        var direction = steer.sign
        var noseX = _facing * _theta.cos
        var forward = noseX.abs < 0.2 || noseX.sign == direction
        var speedRatio = (_vx * direction).max(0) / CarTuning.MAX_SPEED
        var impulse = CarTuning.BACK_DODGE_IMPULSE * (1 + 1.5 * speedRatio)
        if (forward) impulse = CarTuning.DODGE_IMPULSE * (1 + 0.9 * speedRatio)
        // The flip redirects the car: speed going against the stick is partly cancelled.
        var against = (-_vx * direction).max(0)
        _vx = _vx + direction * (impulse + against * CarTuning.DODGE_REDIRECT)
        _vy = _vy > 0 ? 0 : _vy * CarTuning.DODGE_VERTICAL_KEEP
        // Pushing right spins clockwise: a front flip for a car facing right, a back flip for one facing left.
        _spin = direction * CarTuning.DODGE_SPIN
        _spinTime = CarTuning.DODGE_TIME
        _dodging = true
    }

    // On the roof or on the side: jumping rolls the car back onto its wheels.
    recover_() {
        _vx = _vx + _contactNx * CarTuning.JUMP_IMPULSE
        _vy = _vy + _contactNy * CarTuning.JUMP_IMPULSE
        var wheelsDown = (_contactNx).atan(-_contactNy)
        var turn = Car.wrap_(wheelsDown - _theta)
        if (turn.abs > 3) turn = Num.pi * _facing
        _spin = turn / CarTuning.RECOVERY_TIME
        _spinTime = CarTuning.RECOVERY_TIME
        _sinceJump = 0
        _contact = false
        jumped_(Car.JUMP)
    }

    // Keeps the four corners of the hitbox inside the arena, and lands when the wheels touch first.
    collide_(dt) {
        _contact = false
        // Corners of goals are sharp: a car can touch two walls at once, so push out a few times.
        var first = null
        for (i in 0...3) {
            var hit = deepestCorner_()
            if (hit == null) break
            if (first == null) first = hit
            _x = _x - hit[1] * hit[0]
            _y = _y - hit[2] * hit[0]
        }
        if (first == null) return

        var nx = first[1]
        var ny = first[2]
        var c = _theta.cos
        var s = _theta.sin
        var align = s * nx - c * ny
        var vn = _vx * nx + _vy * ny
        _contact = true
        _contactAlign = align
        _contactNx = nx
        _contactNy = ny

        if (_sinceJump > CarTuning.JUMP_LAND_DELAY && vn < CarTuning.LAND_MAX_SEPARATION) {
            // Like in Sideswipe, a car never rests on its roof or its side: if it touches a surface any other way than
            // wheels first (the self-righting did not have time), it is turned over onto its wheels as it lands.
            if (align <= CarTuning.LAND_ALIGN) _theta = (nx).atan(-ny)
            land_(nx, ny, vn)
            return
        }

        // The body hit the wall: bounce a little, slide with friction, tip over toward a resting position.
        if (vn < 0) {
            impact_(-vn)
            _vx = _vx - nx * vn * (1 + CarTuning.RESTITUTION)
            _vy = _vy - ny * vn * (1 + CarTuning.RESTITUTION)
        }
        var tx = -ny
        var ty = nx
        var vt = _vx * tx + _vy * ty
        var slowed = Car.towardZero_(vt, CarTuning.BODY_FRICTION * dt)
        _vx = _vx + tx * (slowed - vt)
        _vy = _vy + ty * (slowed - vt)
        if (_spinTime <= 0) _omega = Car.wrap_(ty.atan(tx) - _theta) * CarTuning.TIP_RATE
    }

    /** @brief The hitbox corner deepest in a wall: [depth (negative), nx, ny], or null if none is in a wall. */
    deepestCorner_() {
        var c = _theta.cos
        var s = _theta.sin
        var hl = CarTuning.HALF_LENGTH
        var hh = CarTuning.HALF_HEIGHT
        var deepest = -0.01
        var hit = null
        for (corner in Car.CORNERS) {
            var lx = corner[0] * hl
            var ly = corner[1] * hh
            _arena.sample(_x + c * lx + s * ly, _y + s * lx - c * ly)
            if (_arena.distance < deepest) {
                deepest = _arena.distance
                hit = [deepest, _arena.nx, _arena.ny]
            }
        }
        return hit
    }

    /** @brief Pushes the hitbox back inside the arena (after the ball pushed the car), stopping it against the wall. */
    keepInArena() {
        blockCorners_()
    }

    // On the wheels, the nose or the roof can still run into a wall (the back of a goal): stop against it.
    blockCorners_() {
        for (i in 0...3) {
            var hit = deepestCorner_()
            if (hit == null) return
            _x = _x - hit[1] * hit[0]
            _y = _y - hit[2] * hit[0]
            var into = _vx * hit[1] + _vy * hit[2]
            if (into < 0) {
                _vx = _vx - hit[1] * into
                _vy = _vy - hit[2] * into
            }
        }
    }

    land_(nx, ny, vn) {
        impact_(-vn)
        // Keep the speed along the surface, lose the speed into it.
        _vx = _vx - nx * vn
        _vy = _vy - ny * vn
        _grounded = true
        _separation = 0
        _normalSpeed = 0
        // The stick direction kept from before the flight is stale on a new surface (the floor, then the ceiling):
        // read the surface again, or the car would drive away from where the stick points.
        _steerLocked = false
        _hasJumped = false
        _secondJumpUsed = false
        _jumpHold = 0
        _spinTime = 0
        _dodging = false
        _flipReset = false
        _omega = 0
        _contact = false
    }

    limitSpeed_() {
        var speed = (_vx * _vx + _vy * _vy).sqrt
        if (speed > CarTuning.MAX_SPEED) {
            _vx = _vx * CarTuning.MAX_SPEED / speed
            _vy = _vy * CarTuning.MAX_SPEED / speed
        }
    }

    // --- Helpers ---

    /** @brief Corners of the hitbox, in half-sizes: wheels side (y = -1) first. */
    static CORNERS {
        if (__corners == null) __corners = [[-1, -1], [1, -1], [-1, 1], [1, 1]]
        return __corners
    }

    static towardZero_(value, amount) { value > 0 ? (value - amount).max(0) : (value + amount).min(0) }

    // A counter read from a component: 0 when the component is missing (tests) or new.
    static counter_(value) { value is Num ? value : 0 }

    static wrap_(angle) {
        var turn = Num.pi * 2
        while (angle > Num.pi) angle = angle - turn
        while (angle < -Num.pi) angle = angle + turn
        return angle
    }
}
