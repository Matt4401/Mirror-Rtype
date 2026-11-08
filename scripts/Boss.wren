import "core/Entity" for Entity
import "core/Engine" for Engine
import "core/Position" for Position

/**
 * @class Boss
 * @brief Attack pattern of the `boss_core` prefab, attached by a level `boss` event:
 *
 *   [[events]]
 *   at = 6000.0
 *   type = "boss"
 *   prefab = "boss_core"
 *   script = "scripts/Boss.wren"
 *
 * The server owns everything else: the boss flies in until its `stop_x`, loses one hit point per player shot and is
 * destroyed at 0, then the level goes on. This script only moves the boss up and down and fires: missile volleys,
 * then a scout escort, faster after `enrageAfter` seconds. Everything it spawns is replicated to the clients.
 *
 * Shows the scripting features used on the server: component proxies (`Position`), lifecycle hooks (`onSpawn`,
 * `onReload`, `onDestroy`), the per-frame `update(dt)` and coroutines paced with `Engine.wait`.
 */
class Boss {
    /** @brief Seconds the boss takes to fly in before attacking. */
    static entranceDuration { 3.0 }
    /** @brief Seconds of fight after which the boss attacks faster. */
    static enrageAfter { 20.0 }
    /** @brief Vertical swing around the spawn height, in pixels. */
    static swing { 120.0 }

    construct new(entity) {
        _entity = (entity is Num) ? Entity.new(entity) : entity
        _pos = Position.new(_entity.id)
        _baseY = _pos.y
        _time = 0.0
        _alive = true
    }

    /** @brief True after `enrageAfter` seconds of fight. */
    isEnraged { _time > Boss.entranceDuration + Boss.enrageAfter }

    /** @brief Called once the boss entity exists: starts the attack pattern. */
    onSpawn() {
        startAttacks()
    }

    /** @brief Called after a hot reload of this file: the pattern restarts, the boss keeps its place. */
    onReload() {
        _baseY = _pos.y
        startAttacks()
    }

    /** @brief Called before the boss is removed (killed by the players): stops the attacks and drops a power-up. */
    onDestroy() {
        _alive = false
        Engine.spawnPrefab("power_up", _pos.x, _pos.y)
    }

    /**
     * @brief Swings the boss up and down, wider and faster once enraged.
     * @param dt Delta time in seconds.
     */
    update(dt) {
        _time = _time + dt
        var speed = isEnraged ? 1.6 : 0.8
        var amplitude = isEnraged ? Boss.swing * 1.4 : Boss.swing
        _pos.y = _baseY + (_time * speed).sin * amplitude
    }

    /** @brief Runs the attack loop in a fiber, paced with `Engine.wait`, until the boss dies. */
    startAttacks() {
        Fiber.new {
            Engine.wait(Boss.entranceDuration)
            while (_alive) {
                fireVolley()
                Engine.wait(isEnraged ? 0.8 : 1.6)
                if (!_alive) break
                fireVolley()
                Engine.wait(isEnraged ? 0.8 : 1.6)
                if (!_alive) break
                callEscort()
                Engine.wait(isEnraged ? 1.5 : 3.0)
            }
        }.call()
    }

    /** @brief Fires three missiles to the left, in a fan around the boss. */
    fireVolley() {
        for (offset in [-50, 0, 50]) {
            Engine.spawnPrefab("missile", _pos.x - 70, _pos.y + offset)
        }
    }

    /** @brief Calls two scouts, above and below the boss. */
    callEscort() {
        Engine.spawnPrefab("alien_scout", _pos.x - 40, _pos.y - 110)
        Engine.spawnPrefab("alien_scout", _pos.x - 40, _pos.y + 110)
    }
}
