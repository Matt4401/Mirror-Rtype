import "core/Input" for Input
import "rlss/components" for Transform, Controls
import "rlss/scripts/Arena" for Arena
import "rlss/scripts/CarTuning" for CarTuning

/**
 * @class Car
 * @brief Server-side physics of one car, seen from the side, modelled on Rocket League.
 *
 * Attached to every car entity (`ScriptComponent`, `hasUpdate = true`). Reads the player's input through
 * `Input.getAxis(player, "steer")` / `Input.isDown(player, "jump")`, simulates at a fixed 120 Hz, and writes the
 * result into the entity `Transform` (x, y, rotation; `scaleX` = -1 when facing left), which the engine replicates.
 *
 * Orientation: `theta` is the car rotation, clockwise on screen. Along the car, the tangent is (cos, sin) and the roof
 * (car "up") is (sin, -cos); the nose is the tangent times `facing` (+1 or -1). Turning around mirrors the car
 * (Sideswipe style) instead of rotating it.
 *
 * Two states:
 * - on its wheels (`grounded`): the car follows the surface (floor, curved corners, walls, ceiling). Gravity along the
 *   surface slows it on a climb; the sticky force (325 uu/s²) and the push of curved corners keep it on the surface,
 *   gravity pulls it away. When the pull wins long enough for the suspension to extend fully, the wheels lose contact:
 *   the car stays on walls but only lasts a moment on the ceiling.
 * - in the air: gravity, jump hold, double jump / dodge, air rotation; the body collides with the arena walls and lands
 *   when it touches them wheels first.
 */
class Car {
    construct new(entity) {
        _id = entity is Num ? entity : entity.id
        _transform = Transform.new(_id)
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
        _prevJump = false

        _contact = false
        _contactAlign = 0
        _contactNx = 0
        _contactNy = -1

        _accumulator = 0
    }

    // --- Read-only state (tests, debugging) ---
    grounded { _grounded }
    speed { (_vx * _vx + _vy * _vy).sqrt }
    vx { _vx }
    vy { _vy }
    facing { _facing }

    update(dt) {
        if (_player == null || _transform.x == null) return
        if (_arena == null) {
            _arena = Arena.fromWorld()
            if (_arena == null) return
        }
        _accumulator = (_accumulator + dt).min(CarTuning.MAX_FRAME_TIME)
        while (_accumulator >= CarTuning.STEP) {
            _accumulator = _accumulator - CarTuning.STEP
            step_(CarTuning.STEP)
        }
        _transform.x = _x
        _transform.y = _y
        _transform.rotation = _theta
        _transform.scaleX = _facing
    }

    step_(dt) {
        var steer = Input.getAxis(_player, "steer")
        if (steer == null || steer.abs < CarTuning.DEADZONE) steer = 0
        steer = steer.clamp(-1, 1)
        var jump = Input.isDown(_player, "jump")
        var jumpPressed = jump && !_prevJump
        _prevJump = jump
        _sinceJump = _sinceJump + dt

        if (_grounded) {
            drive_(dt, steer, jumpPressed)
        } else {
            fly_(dt, steer, jump, jumpPressed)
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
        var throttle = steer * _steerSign

        if (throttle != 0 && throttle.sign != _facing && s.abs < CarTuning.TURN_SPEED) _facing = throttle.sign

        if (throttle != 0 && (s.abs < 1 || throttle.sign == s.sign)) {
            s = s + throttle.sign * CarTuning.throttleAccel(s.abs) * throttle.abs * dt
        } else if (throttle != 0) {
            s = Car.towardZero_(s, CarTuning.BRAKE * dt)
        } else {
            s = Car.towardZero_(s, CarTuning.COAST * dt)
        }
        s = (s + CarTuning.GRAVITY * ty * dt).clamp(-CarTuning.MAX_SPEED, CarTuning.MAX_SPEED)

        // Does the surface still hold the car? Gravity pulls it off the ceiling, curved corners push it in.
        var press = -CarTuning.GRAVITY * ny + s * s * arena.curvature + CarTuning.STICKY
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

        if (jumpPressed) {
            // Jump along the surface normal: straight up from the floor, away from a wall.
            _vx = _vx + arena.nx * CarTuning.JUMP_IMPULSE
            _vy = _vy + arena.ny * CarTuning.JUMP_IMPULSE
            _jumpDirX = arena.nx
            _jumpDirY = arena.ny
            _jumpHold = CarTuning.JUMP_HOLD_TIME
            _omega = 0
            leaveGround_(true)
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

    fly_(dt, steer, jump, jumpPressed) {
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
            if (_contact && _contactAlign < CarTuning.LAND_ALIGN) {
                recover_()
            } else if (!_secondJumpUsed && (!_hasJumped || _airTime < CarTuning.SECOND_JUMP_WINDOW)) {
                secondJump_(steer)
            }
        }

        if (_spinTime > 0) {
            _spinTime = _spinTime - dt
            _omega = _spin
        } else {
            var torque = CarTuning.AIR_TORQUE * steer - CarTuning.AIR_DAMPING * _omega * (1 - steer.abs)
            var maxSpin = CarTuning.MAX_ANGULAR_SPEED
            _omega = (_omega + torque * dt).clamp(-maxSpin, maxSpin)
        }

        _vy = _vy + CarTuning.GRAVITY * dt
        limitSpeed_()
        _x = _x + _vx * dt
        _y = _y + _vy * dt
        _theta = Car.wrap_(_theta + _omega * dt)
        collide_(dt)
    }

    // Double jump without direction, dodge (flip) toward the stick otherwise.
    secondJump_(steer) {
        _secondJumpUsed = true
        _jumpHold = 0
        if (steer == 0) {
            _vx = _vx + _theta.sin * CarTuning.JUMP_IMPULSE
            _vy = _vy - _theta.cos * CarTuning.JUMP_IMPULSE
            return
        }
        var direction = steer.sign
        var noseX = _facing * _theta.cos
        var forward = noseX.abs < 0.2 || noseX.sign == direction
        var speedRatio = (_vx * direction).max(0) / CarTuning.MAX_SPEED
        var impulse = CarTuning.BACK_DODGE_IMPULSE * (1 + 1.5 * speedRatio)
        if (forward) impulse = CarTuning.DODGE_IMPULSE * (1 + 0.9 * speedRatio)
        _vx = _vx + direction * impulse
        _vy = _vy > 0 ? 0 : _vy * CarTuning.DODGE_VERTICAL_KEEP
        // Pushing right spins clockwise: a front flip for a car facing right, a back flip for one facing left.
        _spin = direction * CarTuning.MAX_ANGULAR_SPEED
        _spinTime = CarTuning.DODGE_TIME
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
    }

    // Keeps the four corners of the hitbox inside the arena, and lands when the wheels touch first.
    collide_(dt) {
        var c = _theta.cos
        var s = _theta.sin
        var hl = CarTuning.HALF_LENGTH
        var hh = CarTuning.HALF_HEIGHT
        var deepest = 0
        var nx = 0
        var ny = 0
        for (corner in Car.CORNERS) {
            var lx = corner[0] * hl
            var ly = corner[1] * hh
            _arena.sample(_x + c * lx + s * ly, _y + s * lx - c * ly)
            if (_arena.distance < deepest) {
                deepest = _arena.distance
                nx = _arena.nx
                ny = _arena.ny
            }
        }
        _contact = false
        if (deepest > -0.01) return

        _x = _x - nx * deepest
        _y = _y - ny * deepest
        var align = s * nx - c * ny
        var vn = _vx * nx + _vy * ny
        _contact = true
        _contactAlign = align
        _contactNx = nx
        _contactNy = ny

        if (align > CarTuning.LAND_ALIGN && _sinceJump > CarTuning.JUMP_LAND_DELAY && vn <= 0) {
            land_(nx, ny, vn)
            return
        }

        // The body hit the wall: bounce a little, slide with friction, tip over toward a resting position.
        if (vn < 0) {
            _vx = _vx - nx * vn * (1 + CarTuning.RESTITUTION)
            _vy = _vy - ny * vn * (1 + CarTuning.RESTITUTION)
        }
        var tx = -ny
        var ty = nx
        var vt = _vx * tx + _vy * ty
        var slowed = Car.towardZero_(vt, CarTuning.BODY_FRICTION * dt)
        _vx = _vx + tx * (slowed - vt)
        _vy = _vy + ty * (slowed - vt)
        if (_spinTime <= 0) {
            var wheelsDown = ty.atan(tx)
            var target = align >= 0 ? wheelsDown : wheelsDown + Num.pi
            _omega = Car.wrap_(target - _theta) * CarTuning.TIP_RATE
        }
    }

    land_(nx, ny, vn) {
        // Keep the speed along the surface, lose the speed into it.
        _vx = _vx - nx * vn
        _vy = _vy - ny * vn
        _grounded = true
        _separation = 0
        _normalSpeed = 0
        _hasJumped = false
        _secondJumpUsed = false
        _jumpHold = 0
        _spinTime = 0
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

    static wrap_(angle) {
        var turn = Num.pi * 2
        while (angle > Num.pi) angle = angle - turn
        while (angle < -Num.pi) angle = angle + turn
        return angle
    }
}
