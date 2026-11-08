import "core/Engine" for Engine
import "core/Position" for Position
import "game/Combat" for Combat

/**
 * @class Warden
 * @brief The Hive Warden: a drifting carrier that releases drone fans and fires aimed bursts from its turrets.
 *
 * Meant for a boss with a `weak_point` and one or more `turret` parts (see `server/levels/`):
 *
 *   [[events]]
 *   type = "boss"
 *   prefab = "boss_core"
 *   variant = 1
 *   script = "scripts/bosses/Warden.wren"
 *
 * Pattern, after a `entranceDuration` flight in:
 * - drifts along a slow figure of eight around the place it stopped at;
 * - releases a fan of `alien_scout` drones;
 * - each living turret fires a burst of `burstShots` shots aimed at the nearest ship;
 * - frenzy, once every turret is down or the boss is under half of its hit points: the core fires a three-shot spread
 *   itself and the cycle runs faster.
 * When it dies, it drops a power-up.
 */
class Warden {
    /** @brief Seconds the boss takes to fly in before attacking. */
    static entranceDuration { 3.0 }
    /** @brief Half-height of the figure of eight, in pixels. */
    static driftHeight { 110.0 }
    /** @brief Half-width of the figure of eight, in pixels. */
    static driftWidth { 40.0 }
    /** @brief Shots of a turret burst. */
    static burstShots { 3 }
    /** @brief Speed of the aimed shots, in pixels per second. */
    static shotSpeed { 380.0 }
    /** @brief Heights of the drones of a fan, around the boss. */
    static droneOffsets { [-130, -45, 45, 130] }

    construct new(entity) {
        _id = (entity is Num) ? entity : entity.id
        _pos = Position.new(_id)
        _time = 0.0
        _alive = true
        _anchored = false
    }

    /** @brief True once every turret is down, or under half of its hit points. */
    inFrenzy {
        var health = Combat.health(_id)
        var hurt = health != null && health[0] * 2 < health[1]
        return hurt || turrets.count == 0
    }

    /** @brief Ids of the living turrets. */
    turrets { Combat.parts(_id).where {|part| Combat.partRole(part) == "turret" }.toList }

    /** @brief Called once the boss exists: starts the attack cycle. */
    onSpawn() {
        startAttacks()
    }

    /** @brief Called after a hot reload: the drift restarts from where the boss is. */
    onReload() {
        _anchored = false
        startAttacks()
    }

    /** @brief Called when the boss is destroyed: stops the cycle and drops a power-up. */
    onDestroy() {
        _alive = false
        Engine.spawnPrefab("power_up", _pos.x, _pos.y)
    }

    /**
     * @brief Drifts along the figure of eight once the flight in is over.
     * @param dt Delta time in seconds.
     */
    update(dt) {
        _time = _time + dt
        if (_time < Warden.entranceDuration) return
        if (!_anchored) {
            _anchored = true
            _baseX = _pos.x
            _baseY = _pos.y
            _driftTime = 0.0
        }
        _driftTime = _driftTime + dt * (inFrenzy ? 1.6 : 1.0)
        _pos.x = _baseX + (_driftTime * 0.5).sin * Warden.driftWidth
        _pos.y = _baseY + (_driftTime).sin * Warden.driftHeight
    }

    /** @brief Runs the attack cycle in a fiber until the boss dies. */
    startAttacks() {
        Fiber.new {
            Engine.wait(Warden.entranceDuration)
            while (_alive) {
                releaseDrones()
                Engine.wait(inFrenzy ? 0.8 : 1.5)
                if (!_alive) break
                for (turret in turrets) {
                    fireBurst(turret)
                }
                Engine.wait(1.0)
                if (!_alive) break
                if (inFrenzy) {
                    fireSpread()
                    Engine.wait(0.6)
                } else {
                    Engine.wait(1.4)
                }
            }
        }.call()
    }

    /** @brief Releases a fan of drones from the front of the boss. */
    releaseDrones() {
        for (offset in Warden.droneOffsets) {
            Engine.spawnPrefab("alien_scout", _pos.x - 70, _pos.y + offset)
        }
    }

    /**
     * @brief Fires a burst of aimed shots from a turret; stops early if the turret or the boss dies.
     * @param turret Id of the turret.
     */
    fireBurst(turret) {
        var at = Position.new(turret)
        for (i in 0...Warden.burstShots) {
            if (!_alive || Combat.partRole(turret) == null) return
            var ship = Combat.nearestShip(at.x, at.y)
            if (ship == null) return
            Combat.fireAt(at.x, at.y, ship[0], ship[1], Warden.shotSpeed)
            Engine.wait(0.15)
        }
    }

    /** @brief Fires three shots from the core: at the nearest ship, and above and below it. */
    fireSpread() {
        var ship = Combat.nearestShip(_pos.x, _pos.y)
        if (ship == null) return
        for (offset in [-90, 0, 90]) {
            Combat.fireAt(_pos.x - 60, _pos.y, ship[0], ship[1] + offset, Warden.shotSpeed * 1.2)
        }
    }
}
