import "rlss/components" for MatchState
import "rlss/scripts/Arena" for Arena
import "rlss/scripts/BallTuning" for BallTuning
import "rlss/scripts/CarTuning" for CarTuning
import "rlss/scripts/Kickoff" for Kickoff
import "rlss/scripts/Shots" for Shots
import "rlss/scripts/Simulation" for Simulation

/**
 * @class Match
 * @brief Runs the match: steps every car, the ball and their contacts at a fixed 120 Hz, and applies the rules.
 *
 * Attached to the arena entity (`hasUpdate = true`). Each step: the cars move (their input, the arena), then the ball,
 * then each car that touches the ball pushes it (and is pushed back), then the ball is kept inside the arena again.
 *
 * Rules, like Rocket League: a goal counts once the whole ball is behind the goal line. The team attacking that goal
 * scores (the left goal is the blue team's, so a ball in it is a point for orange). After a short celebration, every car
 * goes back to its kickoff spot and the ball to the center; they stay frozen during a 3-2-1 countdown, then play
 * resumes. The match starts with such a kickoff.
 *
 * A match lasts `DURATION` seconds of play: the clock stops during the celebrations and the countdowns. When it runs
 * out, the match ends, unless the score is tied: then comes an overtime after a kickoff, with no time limit, and the next
 * goal wins (golden goal). Once ended, everything stays frozen. Score, phase, countdown and clock are written to the
 * arena's `MatchState`, which the server sends to the players.
 */
class Match {
    construct new(entity) {
        _id = entity is Num ? entity : entity.id
        _state = MatchState.new(_id)
        _accumulator = 0
        _step = 0
        _phase = null
        _timer = 0
        _clock = 0
        _overtime = false
    }

    static PLAYING { 0 }
    static GOAL { 1 }
    static KICKOFF { 2 }
    static ENDED { 3 }
    /** @brief Length of a match, in seconds of play (3 minutes). */
    static DURATION { 180 }

    phase { _phase }

    update(dt) {
        if (_arena == null) _arena = Arena.fromWorld()
        if (_arena == null || _state.blueScore == null) return
        if (_phase == null) start_()
        _accumulator = (_accumulator + dt).min(CarTuning.MAX_FRAME_TIME)
        var cars = Simulation.cars
        while (_accumulator >= CarTuning.STEP) {
            _accumulator = _accumulator - CarTuning.STEP
            _step = _step + 1
            rules_(cars, Simulation.ball, CarTuning.STEP)
        }
        for (car in cars) {
            if (car.ready) car.sync()
        }
        var ball = Simulation.ball
        if (ball != null && ball.ready) ball.sync()
        _state.phase = _phase
        _state.countdown = _phase == Match.KICKOFF ? _timer.ceil : 0
        _state.clock = _overtime ? _clock.floor : _clock.ceil
        _state.overtime = _overtime
    }

    // A new match starts with a kickoff and a full clock. After a hot-reload (the script is recreated, the components
    // stay), it goes on from the clock and the score of the component instead.
    start_() {
        if (_state.phase == Match.ENDED) {
            _phase = Match.ENDED
        } else if (_state.clock > 0 || _state.overtime) {
            _phase = Match.PLAYING
            _clock = _state.clock
            _overtime = _state.overtime
        } else {
            _phase = Match.KICKOFF
            _timer = Kickoff.COUNTDOWN
            _clock = Match.DURATION
        }
    }

    rules_(cars, ball, dt) {
        if (_phase == Match.ENDED) return
        if (_phase == Match.KICKOFF) {
            // Everybody frozen in place until the countdown ends.
            _timer = _timer - dt
            if (_timer <= 0) _phase = Match.PLAYING
            return
        }
        step_(cars, ball, dt)
        if (_phase == Match.GOAL) {
            _timer = _timer - dt
            if (_timer > 0) return
            // Golden goal: in overtime, the first goal ends the match.
            if (_overtime) {
                end_()
            } else {
                kickoff_(cars, ball)
            }
        } else if (ball != null && ball.ready) {
            checkGoal_(ball)
            if (_phase == Match.PLAYING) tick_(cars, ball, dt)
        }
    }

    // The clock only runs while the ball is in play.
    tick_(cars, ball, dt) {
        if (_overtime) {
            _clock = _clock + dt
            return
        }
        _clock = _clock - dt
        if (_clock > 0) return
        _clock = 0
        if (_state.blueScore != _state.orangeScore) {
            end_()
            return
        }
        System.print("[Match] Time is up, %(_state.blueScore) - %(_state.orangeScore): overtime!")
        _overtime = true
        kickoff_(cars, ball)
    }

    end_() {
        _phase = Match.ENDED
        System.print("[Match] Final score: Blue %(_state.blueScore) - %(_state.orangeScore) Orange")
    }

    checkGoal_(ball) {
        var r = BallTuning.RADIUS
        if (ball.x + r < _arena.left) {
            _state.orangeScore = _state.orangeScore + 1
        } else if (ball.x - r > _arena.right) {
            _state.blueScore = _state.blueScore + 1
        } else {
            return
        }
        System.print("[Match] Goal! Blue %(_state.blueScore) - %(_state.orangeScore) Orange")
        _phase = Match.GOAL
        _timer = Kickoff.GOAL_CELEBRATION
    }

    kickoff_(cars, ball) {
        for (car in cars) {
            if (car.ready) car.placeAtKickoff()
        }
        if (ball != null && ball.ready) ball.respawn(_arena.centerX, _arena.centerY)
        _phase = Match.KICKOFF
        _timer = Kickoff.COUNTDOWN
    }

    step_(cars, ball, dt) {
        for (car in cars) {
            if (car.ready) car.step(dt)
        }
        if (ball == null || !ball.ready) return
        // Purple shots: a car that just jumped off the ball pushes it away from its wheels.
        for (car in cars) {
            var purple = car.ready ? car.takePurpleShot() : null
            if (purple != null) {
                ball.addVelocity(purple[0] * Shots.PURPLE_IMPULSE, purple[1] * Shots.PURPLE_IMPULSE)
                ball.shot = Shots.PURPLE
                ball.recordHit(Shots.PURPLE_IMPULSE)
            }
        }
        ball.step(dt)
        for (car in cars) {
            if (car.ready) Match.hit(car, ball, _step)
        }
        // Separating a car from the ball may have pushed either of them into a wall.
        ball.constrain()
        for (car in cars) {
            if (car.ready) car.keepInArena()
        }
    }

    /**
     * @brief Resolves the contact between a car hitbox (118 x 36, rotated) and the ball (a circle), if they touch.
     * @param step Current step number, for the "at most every other step" rule of the extra impulse.
     */
    static hit(car, ball, step) {
        var hl = CarTuning.HALF_LENGTH
        var hh = CarTuning.HALF_HEIGHT
        var r = BallTuning.RADIUS
        var c = car.theta.cos
        var s = car.theta.sin
        var dx = ball.x - car.x
        var dy = ball.y - car.y
        // Ball center in the car frame: along the car, toward the roof.
        var along = dx * c + dy * s
        var up = dx * s - dy * c
        var closestAlong = along.clamp(-hl, hl)
        var closestUp = up.clamp(-hh, hh)
        var offAlong = along - closestAlong
        var offUp = up - closestUp
        var distance = (offAlong * offAlong + offUp * offUp).sqrt
        if (distance >= r) return

        var normalAlong = 0
        var normalUp = 0
        var depth = 0
        if (distance > 0.0001) {
            normalAlong = offAlong / distance
            normalUp = offUp / distance
            depth = r - distance
        } else if (hl - along.abs < hh - up.abs) {
            // Ball center inside the hitbox: leave through the closest side.
            normalAlong = along >= 0 ? 1 : -1
            depth = r + hl - along.abs
        } else {
            normalUp = up >= 0 ? 1 : -1
            depth = r + hh - up.abs
        }
        var nx = c * normalAlong + s * normalUp
        var ny = s * normalAlong - c * normalUp
        var contactX = car.x + c * closestAlong + s * closestUp
        var contactY = car.y + s * closestAlong - c * closestUp
        var face = Shots.face(normalAlong, normalUp, car.facing)
        // Wheels on the ball: flip reset and boost, even when just resting on it.
        if (face == "wheels") car.touchBallWithWheels(nx, ny)

        // Separate them: the ball is six times lighter, it moves six times more.
        var carMass = BallTuning.CAR_MASS
        var ballMass = BallTuning.MASS
        var ballShare = carMass / (carMass + ballMass)
        ball.move(nx * depth * ballShare, ny * depth * ballShare)
        car.move(-nx * depth * (1 - ballShare), -ny * depth * (1 - ballShare))

        // Velocities of both bodies at the contact point (a flipping car hits harder with its spinning nose).
        var carPointX = car.vx - car.omega * (contactY - car.y)
        var carPointY = car.vy + car.omega * (contactX - car.x)
        var ballPointX = ball.vx - ball.spin * (contactY - ball.y)
        var ballPointY = ball.vy + ball.spin * (contactX - ball.x)
        var relX = ballPointX - carPointX
        var relY = ballPointY - carPointY
        var closing = relX * nx + relY * ny
        if (closing >= 0) return  // touching but not hitting (resting on the roof, moving apart)

        var impulse = -(1 + BallTuning.CAR_RESTITUTION) * closing / (1 / ballMass + 1 / carMass)
        ball.addVelocity(nx * impulse / ballMass, ny * impulse / ballMass)
        car.addVelocity(-nx * impulse / carMass, -ny * impulse / carMass)

        // Friction at the contact: the ball starts to spin instead of sliding along the car.
        var tx = -ny
        var ty = nx
        var sliding = relX * tx + relY * ty
        var resistance = (1 + 1 / BallTuning.INERTIA_FACTOR) / ballMass + 1 / carMass
        var limit = BallTuning.CAR_FRICTION * impulse
        var friction = (-sliding / resistance).clamp(-limit, limit)
        ball.addVelocity(tx * friction / ballMass, ty * friction / ballMass)
        ball.addSpin(-friction / (BallTuning.INERTIA_FACTOR * ballMass * r))
        car.addVelocity(-tx * friction / carMass, -ty * friction / carMass)

        // Extra impulse, Rocket League style: its strength comes from the speed along the contact (a hit with the front
        // at full speed, not a brush with the roof). A flip adds a shot, colored by the part of the car that hits.
        if (step - car.lastBallHit > 1) {
            car.lastBallHit = step
            var hitX = dx
            var hitY = dy * BallTuning.HIT_VERTICAL_SCALE
            var forward = hitX * car.noseX + hitY * car.noseY
            hitX = hitX - car.noseX * forward * (1 - BallTuning.HIT_FORWARD_SCALE)
            hitY = hitY - car.noseY * forward * (1 - BallTuning.HIT_FORWARD_SCALE)
            var length = (hitX * hitX + hitY * hitY).sqrt
            var speed = (-closing).min(BallTuning.HIT_MAX_RELATIVE_SPEED)
            var push = speed * BallTuning.hitFactor(speed) * BallTuning.HIT_STRENGTH
            var shot = Shots.NONE
            if (car.dodging && face == "rear") {
                // Gold shot: the rear swings into the ball and lobs it, up and back.
                shot = Shots.GOLD
                var lobX = -car.noseX * Shots.GOLD_BACK
                var lobY = -car.noseY * Shots.GOLD_BACK - 1
                var lobLength = (lobX * lobX + lobY * lobY).sqrt
                ball.addVelocity(lobX / lobLength * Shots.GOLD_IMPULSE, lobY / lobLength * Shots.GOLD_IMPULSE)
            } else if (car.dodging && face == "nose") {
                shot = Shots.RED
                push = push + Shots.RED_IMPULSE
            } else if (car.dodging) {
                push = push + Shots.OTHER_FLIP_IMPULSE
            }
            if (length > 0.0001) ball.addVelocity(hitX * push / length, hitY * push / length)
            // Sound: as loud as the meeting, or as the ball sent flying, whichever is stronger.
            if (speed >= BallTuning.HIT_SOUND_SPEED || shot != Shots.NONE) ball.recordHit(speed.max(ball.speed))
            // Any hit changes the glow: a colored shot lights the ball, a plain hit turns it off. (A car jumping off the
            // ball after a purple shot moves away from it: no hit, the purple glow stays.)
            ball.shot = shot
        }
    }
}
