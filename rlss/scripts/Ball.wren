import "rlss/components" for Transform, ShotGlow, BallCues
import "rlss/scripts/Arena" for Arena
import "rlss/scripts/BallTuning" for BallTuning
import "rlss/scripts/Shots" for Shots
import "rlss/scripts/Simulation" for Simulation

/**
 * @class Ball
 * @brief Server-side physics of the ball, seen from the side, modelled on Rocket League.
 *
 * Attached to the ball entity (`ScriptComponent`); registers itself in `Simulation` and is stepped by the `Match`
 * script. Gravity, air drag, speed limits, and bounces against the arena: 60% of the speed into the wall is kept, and
 * friction couples sliding and spin (a spinning ball kicks off the floor, a sliding one starts rolling). A rolling ball
 * slowly brakes and stops. Writes x, y and
 * its rotation into the entity `Transform`.
 *
 * Sounds: the hits by a car (`recordHit`, called by the match) and the bounces off the arena are counted in the entity
 * `BallCues`, which the engine replicates; the clients play a sound when a counter changes.
 *
 * Spin is clockwise on screen, like the rotation: a ball rolling right on the floor spins positive.
 */
class Ball {
    construct new(entity) {
        _id = entity is Num ? entity : entity.id
        _transform = Transform.new(_id)
        _glow = ShotGlow.new(_id)
        _cues = BallCues.new(_id)
        // Sound cues: the counters go on from the component (a hot-reload must not replay the last sounds).
        _hits = _cues.hits is Num ? _cues.hits : 0
        _hitSpeed = 0
        _bounces = _cues.bounces is Num ? _cues.bounces : 0
        _bounceSpeed = 0
        _shot = Shots.NONE
        _glowTime = 0
        _arena = Arena.fromWorld()
        _x = _transform.x
        _y = _transform.y
        _angle = _transform.rotation
        _vx = 0
        _vy = 0
        _spin = 0
        Simulation.ball = this
    }

    onDestroy() {
        if (Simulation.ball == this) Simulation.ball = null
    }

    x { _x }
    y { _y }
    vx { _vx }
    vy { _vy }
    spin { _spin }
    radius { BallTuning.RADIUS }
    speed { (_vx * _vx + _vy * _vy).sqrt }

    addVelocity(dx, dy) {
        _vx = _vx + dx
        _vy = _vy + dy
    }

    addSpin(delta) { _spin = _spin + delta }

    /** @brief Hits by a car so far, and the strength of the last one (uu/s). */
    hits { _hits }
    hitSpeed { _hitSpeed }
    /** @brief Bounces off the arena so far, and the speed into the wall of the last one. */
    bounces { _bounces }
    bounceSpeed { _bounceSpeed }

    /** @brief A car hit the ball this hard (uu/s): counted for the sounds. */
    recordHit(speed) {
        _hits = _hits + 1
        _hitSpeed = speed
    }

    /** @brief Color of the last shot (`Shots.RED`, `PURPLE`, `GOLD`), or `Shots.NONE`: the ball glows with it. */
    shot { _shot }
    shot=(value) {
        _shot = value
        _glowTime = value == Shots.NONE ? 0 : Shots.GLOW_TIME
    }

    /** @brief Puts the ball back at rest (kickoff). */
    respawn(x, y) {
        _x = x
        _y = y
        _vx = 0
        _vy = 0
        _spin = 0
        _angle = 0
        shot = Shots.NONE
    }

    move(dx, dy) {
        _x = _x + dx
        _y = _y + dy
    }

    ready {
        if (_transform.x == null) return false
        if (_arena == null) _arena = Arena.fromWorld()
        return _arena != null
    }

    step(dt) {
        if (_glowTime > 0) {
            _glowTime = _glowTime - dt
            if (_glowTime <= 0) _shot = Shots.NONE
        }
        // Drag first (like a linear damping), then gravity.
        var damping = (1 - BallTuning.DRAG).pow(dt)
        _vx = _vx * damping
        _vy = _vy * damping + BallTuning.GRAVITY * dt
        limit_()
        _x = _x + _vx * dt
        _y = _y + _vy * dt
        _angle = (_angle + _spin * dt) % (Num.pi * 2)
        _rolling = false
        constrain()
        if (_rolling) roll_(dt)
    }

    // Rolling on a surface slowly brakes the ball (and its spin with it): it stops after a few seconds.
    roll_(dt) {
        var tx = -_rollNy
        var ty = _rollNx
        var along = _vx * tx + _vy * ty
        var braking = BallTuning.ROLLING_RESISTANCE * dt
        var slowed = (along + braking).min(0)
        if (along > 0) slowed = (along - braking).max(0)
        _vx = _vx + tx * (slowed - along)
        _vy = _vy + ty * (slowed - along)
        _spin = (slowed / BallTuning.RADIUS).clamp(-BallTuning.MAX_ANGULAR_SPEED, BallTuning.MAX_ANGULAR_SPEED)
    }

    /** @brief Keeps the ball inside the arena and bounces it off the walls. */
    constrain() {
        var r = BallTuning.RADIUS
        _arena.sample(_x, _y)
        var depth = r - _arena.distance
        if (depth <= 0) return
        var nx = _arena.nx
        var ny = _arena.ny
        _x = _x + nx * depth
        _y = _y + ny * depth

        var vn = _vx * nx + _vy * ny
        if (vn >= 0) return
        var restitution = vn.abs < BallTuning.REST_SPEED ? 0 : BallTuning.RESTITUTION
        if (vn.abs >= BallTuning.BOUNCE_SOUND_SPEED) {
            _bounces = _bounces + 1
            _bounceSpeed = vn.abs
        }
        if (restitution == 0) {
            _rolling = true
            _rollNx = nx
            _rollNy = ny
        }
        var normalChange = -(1 + restitution) * vn
        _vx = _vx + nx * normalChange
        _vy = _vy + ny * normalChange

        // Friction at the contact point, which moves at (speed along the wall - spin * radius): bring that sliding to
        // zero if friction is strong enough. A sphere resists that change 3.5 times more than a sliding block.
        var tx = -ny
        var ty = nx
        var sliding = _vx * tx + _vy * ty - _spin * r
        var limit = BallTuning.FRICTION * normalChange
        var tangentChange = (-sliding / (1 + 1 / BallTuning.INERTIA_FACTOR)).clamp(-limit, limit)
        _vx = _vx + tx * tangentChange
        _vy = _vy + ty * tangentChange
        _spin = _spin - tangentChange / (BallTuning.INERTIA_FACTOR * r)
        limit_()
    }

    sync() {
        _transform.x = _x
        _transform.y = _y
        _transform.rotation = _angle
        _glow.shot = _shot
        _cues.hits = _hits
        _cues.hitSpeed = _hitSpeed
        _cues.bounces = _bounces
        _cues.bounceSpeed = _bounceSpeed
    }

    limit_() {
        var speed = (_vx * _vx + _vy * _vy).sqrt
        if (speed > BallTuning.MAX_SPEED) {
            _vx = _vx * BallTuning.MAX_SPEED / speed
            _vy = _vy * BallTuning.MAX_SPEED / speed
        }
        _spin = _spin.clamp(-BallTuning.MAX_ANGULAR_SPEED, BallTuning.MAX_ANGULAR_SPEED)
    }
}
