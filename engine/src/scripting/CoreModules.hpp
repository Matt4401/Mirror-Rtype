/**
 * @file CoreModules.hpp
 * @brief Embedded Wren source code for core engine modules and dispatchers.
 */

#pragma once
#include <string_view>

namespace scripting {

/**
 * @brief Core Wren script source code for the `core/Engine` module.
 *
 * Exposes engine-level static services such as coroutines (`Engine.wait(seconds)`)
 * and deferred prefab instantiation (`Engine.spawnPrefab(...)`).
 */
inline constexpr std::string_view CORE_ENGINE_SOURCE = R"wren(
class Engine {
    foreign static registerWait(fiber, seconds)
    static wait(seconds) {
        registerWait(Fiber.current, seconds)
        Fiber.yield()
    }
    foreign static spawnPrefab(name, x, y)
}
)wren";

/**
 * @brief Core Wren script source code for the `core/Entity` module.
 *
 * Represents an entity wrapper providing ergonomic access to components
 * and deferred entity destruction (`Entity.destroy()`).
 */
inline constexpr std::string_view CORE_ENTITY_SOURCE = R"wren(
class Entity {
    construct new(id) {
        _id = id
        _components = {}
    }
    id { _id }
    getComponent(name) { _components[name] }
    setComponent(name, comp) { _components[name] = comp }
    destroy() { Entity.destroyEntity(_id) }
    foreign static destroyEntity(id)
}
)wren";

/**
 * @brief Core Wren script source code for the `core/World` module.
 *
 * Provides static spatial query methods for scripters (`getEntitiesWithTag`, `getNearestWithTag`,
 * `getEntitiesInRadius`).
 */
inline constexpr std::string_view CORE_WORLD_SOURCE = R"wren(
class World {
    foreign static getEntitiesWithTag(tag)
    foreign static getNearestWithTag(x, y, tag)
    foreign static getEntitiesInRadius(x, y, radius)
}
)wren";

/**
 * @brief Core Wren script source code for the `core/Input` module.
 *
 * Exposes static query methods for action buttons and continuous axes (`isDown`, `isJustPressed`, `isJustReleased`,
 * `getAxis`), and for the mouse (`mouseX`, `mouseY`, `mouseWheel`).
 */
inline constexpr std::string_view CORE_INPUT_SOURCE = R"wren(
class Input {
    foreign static isDown(playerIndex, action)
    foreign static isJustPressed(playerIndex, action)
    foreign static isJustReleased(playerIndex, action)
    foreign static getAxis(playerIndex, action)
    foreign static mouseX(playerIndex)
    foreign static mouseY(playerIndex)
    foreign static mouseWheel(playerIndex)

    static isDown(action) { isDown(0, action) }
    static isJustPressed(action) { isJustPressed(0, action) }
    static isJustReleased(action) { isJustReleased(0, action) }
    static getAxis(action) { getAxis(0, action) }
    static mouseX { mouseX(0) }
    static mouseY { mouseY(0) }
    static mouseWheel { mouseWheel(0) }
}
)wren";

/**
 * @brief Internal dispatcher script source code for safe optional lifecycle callback invocation.
 */
inline constexpr std::string_view INTERNAL_DISPATCHER_SOURCE = R"wren(
class Dispatcher {
    static tryCallSpawn(instance) {
        var f = Fiber.new { instance.onSpawn() }
        var err = f.try()
        if (err != null && err.indexOf("does not implement 'onSpawn()'") == -1) {
            System.print("Error in onSpawn(): " + err)
        }
    }
    static tryCallReload(instance) {
        var f = Fiber.new { instance.onReload() }
        var err = f.try()
        if (err != null && err.indexOf("does not implement 'onReload()'") == -1) {
            System.print("Error in onReload(): " + err)
        }
    }
    static tryCallDestroy(instance) {
        var f = Fiber.new { instance.onDestroy() }
        var err = f.try()
        if (err != null && err.indexOf("does not implement 'onDestroy()'") == -1) {
            System.print("Error in onDestroy(): " + err)
        }
    }
}
)wren";

/**
 * @brief Retrieves the embedded source code for a built-in module name.
 *
 * @param moduleName Module import name (e.g. "core/Engine", "core/Entity", "core/World", "_internal").
 * @return String view referencing the embedded module code, or empty string view if not found.
 */
[[nodiscard]] std::string_view getCoreModuleSource(std::string_view moduleName) noexcept;

}  // namespace scripting
