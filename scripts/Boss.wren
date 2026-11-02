import "core/Entity" for Entity
import "core/Engine" for Engine
import "core/Position" for Position
import "core/World" for World

/**
 * @class Boss
 * @brief Autonomous Boss AI script showcasing current engine features:
 *  - ECS Component proxies (Position)
 *  - Full lifecycle hooks (onSpawn, onReload, onDestroy)
 *  - Optional per-frame continuous update loop (update(dt))
 *  - Asynchronous coroutines & timed phases (Fiber & Engine.wait)
 *  - Deferred entity mutations & prefab spawning (CommandBuffer)
 *  - Spatial queries (World API)
 *  - Event handling (onCollision, onDamage)
 */
class Boss {
    construct new(entity) {
        if (entity is Num) {
            _entity = Entity.new(entity)
        } else {
            _entity = entity
        }

        _pos = Position.new(_entity.id)
        _hp = 300
        _maxHp = 300
        _phase = 1
        _isEnraged = false
        _time = 0.0
        _baseY = _pos.y
    }

    // --- Property Getters ---
    entity    { _entity }
    position  { _pos }
    hp        { _hp }
    maxHp     { _maxHp }
    phase     { _phase }
    isEnraged { _isEnraged }

    // --- Lifecycle Hooks ---

    /**
     * @brief Called when the entity script is first initialized by ScriptSystem.
     */
    onSpawn() {
        System.print("[Boss] Entering battlefield! Entity ID: %(_entity.id)")
        startBehaviorSequence()
    }

    /**
     * @brief Called by ScriptSystem when this script is hot-reloaded from disk.
     * Preserves native ECS state and resumes without interrupting running game logic.
     */
    onReload() {
        System.print("[Boss] Script hot-reloaded! Current HP: %(_hp)/%(_maxHp), Phase: %(_phase)")
        _baseY = _pos.y
        startBehaviorSequence()
    }

    /**
     * @brief Called right before the entity is safely removed by the CommandBuffer.
     */
    onDestroy() {
        System.print("[Boss] Destroyed! Spawning victory loot and explosion.")
        Engine.spawnPrefab("power_up", _pos.x, _pos.y)
    }

    // --- Optional Continuous Update (hasUpdate = true) ---

    /**
     * @brief Per-frame update loop for continuous motion and spatial tracking.
     * @param dt Delta time in seconds.
     */
    update(dt) {
        _time = _time + dt

        // Dynamic vertical oscillation (faster when enraged in Phase 2)
        var speed = _isEnraged ? 4.0 : 2.0
        var amplitude = _isEnraged ? 60.0 : 35.0
        _pos.y = _baseY + (_time * speed).sin * amplitude

        // Query nearest player for slight tracking bias
        var nearestPlayer = World.getNearestWithTag(_pos.x, _pos.y, "Player")
        if (nearestPlayer != null) {
            var playerPos = Position.new(nearestPlayer.id)
            if (playerPos.y > _baseY + 10.0) {
                _baseY = _baseY + (15.0 * dt)
            } else if (playerPos.y < _baseY - 10.0) {
                _baseY = _baseY - (15.0 * dt)
            }
        }
    }

    // --- Asynchronous Behavior & Attack Coroutines ---

    /**
     * @brief Runs the boss behavior pattern using Wren Fibers and non-blocking Engine.wait delays.
     */
    startBehaviorSequence() {
        Fiber.new {
            // Entrance sequence: move to combat position
            System.print("[Boss] Phase 1: Deploying into arena...")
            _pos.x = 650.0
            Engine.wait(1.5)

            // Autonomous attack pattern loop
            while (_hp > 0) {
                // Volley 1: Fire missiles
                fireMissileBarrage()
                Engine.wait(_isEnraged ? 1.0 : 2.0)

                // Volley 2: Spawn scout support minion
                System.print("[Boss] Calling scout support...")
                Engine.spawnPrefab("alien_scout", _pos.x - 30.0, _pos.y + 40.0)
                Engine.wait(_isEnraged ? 1.2 : 2.5)

                // Tactical repositioning
                _pos.x = _isEnraged ? 520.0 : 600.0
                Engine.wait(1.0)
                _pos.x = _isEnraged ? 580.0 : 650.0
                Engine.wait(1.5)
            }
        }.call()
    }

    /**
     * @brief Spawns missile prefabs forward from boss coordinates.
     */
    fireMissileBarrage() {
        System.print("[Boss] Firing missile barrage!")
        Engine.spawnPrefab("missile", _pos.x - 40.0, _pos.y - 20.0)
        Engine.spawnPrefab("missile", _pos.x - 40.0, _pos.y + 20.0)
    }

    // --- Event Callbacks ---

    /**
     * @brief Handles damage intake, phase transition and deferred death.
     * @param amount Damage points taken.
     */
    onDamage(amount) {
        _hp = _hp - amount
        System.print("[Boss] Took %(amount) damage! Remaining HP: %(_hp)/%(_maxHp)")

        // Phase 2 transition trigger at 50% health
        if (!_isEnraged && _hp <= (_maxHp * 0.5)) {
            _phase = 2
            _isEnraged = true
            System.print("[Boss] WARNING: Phase 2 initiated! Speed and firing rate increased!")
        }

        if (_hp <= 0) {
            _hp = 0
            System.print("[Boss] Critical failure! Initiating destruction sequence.")
            _entity.destroy()
        }
    }

    /**
     * @brief Magic method invoked when physics detects a collision.
     * @param other Colliding entity or entity ID.
     */
    onCollision(other) {
        var otherId = null
        if (other is Num) {
            otherId = other
        } else {
            otherId = other.id
        }

        System.print("[Boss] Collision detected with entity ID: %(otherId)")
        onDamage(25)

        // Destroy the collided projectile via deferred CommandBuffer
        if (other is Entity) {
            other.destroy()
        } else {
            Entity.destroyEntity(otherId)
        }
    }
}
