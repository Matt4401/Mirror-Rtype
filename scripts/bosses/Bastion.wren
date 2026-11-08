import "core/Engine" for Engine
import "core/Position" for Position
import "game/Combat" for Combat

/**
 * @class Bastion
 * @brief The Bastion: an armored fortress that changes tactics as it loses its hit points.
 *
 * Meant for a boss with a `weak_point` behind `armor` plates and two `turret` parts (see `server/levels/`):
 *
 *   [[events]]
 *   type = "boss"
 *   prefab = "boss_core"
 *   variant = 2
 *   script = "scripts/bosses/Bastion.wren"
 *
 * Phases, by the part of its hit points left:
 * - 1, siege (above two thirds): slides up and down; the turrets fire straight lines, the core an aimed shot;
 * - 2, assault (above one third): charges towards the ships and back, leaving slow drifting mines behind;
 * - 3, last stand: holds still in the middle of its lane and fires a turning spiral of shots all around it.
 * When it dies, it drops two power-ups.
 */
class Bastion {
    /** @brief Seconds the boss takes to fly in before attacking. */
    static entranceDuration { 3.0 }
    /** @brief Half-height of the siege slide, in pixels. */
    static slideHeight { 150.0 }
    /** @brief How far a charge goes to the left, in pixels. */
    static chargeDistance { 420.0 }
    /** @brief Speed of a charge, in pixels per second. */
    static chargeSpeed { 520.0 }
    /** @brief Look of its shots: the red of the turret shots (`ALIEN_GUNNER_SHOT_LOOK`). */
    static shotLook { 1 }
    /** @brief Shots of one turn of the spiral. */
    static spiralShots { 10 }

    construct new(entity) {
        _id = (entity is Num) ? entity : entity.id
        _pos = Position.new(_id)
        _time = 0.0
        _alive = true
        _anchored = false
        _charge = 0.0
        _chargeDirection = 0
        _spiralAngle = 0.0
    }

    /**
     * @brief Current phase: 1 above two thirds of the hit points, 2 above one third, 3 below.
     * @return The phase; 1 if the hit points cannot be read.
     */
    phase {
        var health = Combat.health(_id)
        if (health == null) return 1
        if (health[0] * 3 > health[1] * 2) return 1
        if (health[0] * 3 > health[1]) return 2
        return 3
    }

    /** @brief Called once the boss exists: starts the attacks. */
    onSpawn() {
        startAttacks()
    }

    /** @brief Called after a hot reload: the moves restart from where the boss is. */
    onReload() {
        _anchored = false
        startAttacks()
    }

    /** @brief Called when the boss is destroyed: stops the attacks and drops two power-ups. */
    onDestroy() {
        _alive = false
        Engine.spawnPrefab("power_up", _pos.x, _pos.y - 40)
        Engine.spawnPrefab("power_up", _pos.x, _pos.y + 40)
    }

    /**
     * @brief Moves the boss for its phase once the flight in is over.
     * @param dt Delta time in seconds.
     */
    update(dt) {
        _time = _time + dt
        if (_time < Bastion.entranceDuration) return
        if (!_anchored) {
            _anchored = true
            _homeX = _pos.x
            _homeY = _pos.y
            _slideTime = 0.0
        }
        var current = phase
        if (current == 1) {
            _slideTime = _slideTime + dt
            _pos.y = _homeY + (_slideTime * 0.7).sin * Bastion.slideHeight
        } else if (current == 2) {
            moveCharge(dt)
        } else {
            // Back home, then still: the spiral is dangerous enough.
            _pos.x = _pos.x + (_homeX - _pos.x) * (dt * 3).min(1)
            _pos.y = _pos.y + (_homeY - _pos.y) * (dt * 3).min(1)
        }
    }

    /**
     * @brief Moves a charge in progress: out to `chargeDistance`, then back home.
     * @param dt Delta time in seconds.
     */
    moveCharge(dt) {
        if (_chargeDirection == 0) return
        _charge = (_charge + _chargeDirection * Bastion.chargeSpeed * dt).clamp(0, Bastion.chargeDistance)
        if (_charge >= Bastion.chargeDistance) _chargeDirection = -1
        if (_charge <= 0) _chargeDirection = 0
        _pos.x = _homeX - _charge
    }

    /** @brief Runs the attacks in a fiber until the boss dies, one round per phase. */
    startAttacks() {
        Fiber.new {
            Engine.wait(Bastion.entranceDuration)
            while (_alive) {
                var current = phase
                if (current == 1) {
                    siegeRound()
                } else if (current == 2) {
                    assaultRound()
                } else {
                    lastStandRound()
                }
            }
        }.call()
    }

    /** @brief Phase 1: straight lines from the turrets, then an aimed shot from the core. */
    siegeRound() {
        for (turret in Combat.parts(_id)) {
            if (Combat.partRole(turret) == "turret") {
                var at = Position.new(turret)
                Combat.fireAt(at.x, at.y, at.x - 100, at.y, 420, Bastion.shotLook)
            }
        }
        Engine.wait(0.7)
        if (!_alive) return
        aimFromCore(360)
        Engine.wait(1.1)
    }

    /** @brief Phase 2: a charge towards the ships, dropping mines on the way, then a pause. */
    assaultRound() {
        _chargeDirection = 1
        while (_alive && _chargeDirection != 0) {
            // Mines: slow shots drifting to the left behind the boss.
            Combat.fireAt(_pos.x + 60, _pos.y, _pos.x - 100, _pos.y, 60, Bastion.shotLook)
            Engine.wait(0.25)
        }
        if (!_alive) return
        aimFromCore(450)
        Engine.wait(1.2)
    }

    /** @brief Phase 3: one turn of the spiral, its start angle moving at each turn. */
    lastStandRound() {
        for (i in 0...Bastion.spiralShots) {
            if (!_alive) return
            var angle = _spiralAngle + i * Num.pi * 2 / Bastion.spiralShots
            Combat.fireAt(_pos.x, _pos.y, _pos.x + angle.cos * 100, _pos.y + angle.sin * 100, 300, Bastion.shotLook)
        }
        _spiralAngle = _spiralAngle + 0.35
        Engine.wait(0.45)
    }

    /**
     * @brief Fires one shot from the core at the nearest ship.
     * @param speed Shot speed, in pixels per second.
     */
    aimFromCore(speed) {
        var ship = Combat.nearestShip(_pos.x, _pos.y)
        if (ship != null) Combat.fireAt(_pos.x - 60, _pos.y, ship[0], ship[1], speed, Bastion.shotLook)
    }
}
