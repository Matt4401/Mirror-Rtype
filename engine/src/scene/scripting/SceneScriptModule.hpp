/**
 * @file SceneScriptModule.hpp
 * @brief Wren source of the `core/Scene` built-in module and the owner ids of scenes.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "scene/SceneTypes.hpp"
#include "systems/script/ScriptSystem.hpp"

namespace scene {

/** @brief Import name of the scene module. */
inline constexpr std::string_view SCENE_MODULE_NAME = "core/Scene";

/**
 * @brief Wren source of `core/Scene`.
 *
 * - `Scene`: changes and queries the scene stack by name. Transitions are deferred to the next frame, like in C++.
 * - `SceneHooks_`: internal, calls the optional hooks of a scripted scene inside a fiber, so a hook may use
 *   `Engine.wait` and a missing hook is not an error.
 */
inline constexpr std::string_view SCENE_MODULE_SOURCE = R"wren(
class Scene {
    foreign static switchTo(name)
    foreign static push(name)
    foreign static pop()
    foreign static replace(name)
    foreign static current
    foreign static isActive(name)
    foreign static isRunning(name)
}

class SceneHooks_ {
    static enter(instance) { run_(Fiber.new { instance.onEnter() }, "onEnter()") }
    static exit(instance) { run_(Fiber.new { instance.onExit() }, "onExit()") }
    static pause(instance) { run_(Fiber.new { instance.onPause() }, "onPause()") }
    static resume(instance) { run_(Fiber.new { instance.onResume() }, "onResume()") }

    static run_(fiber, hook) {
        var error = fiber.try()
        if (error != null && error.indexOf("does not implement '%(hook)'") == -1) {
            System.print("Error in scene %(hook): %(error)")
        }
    }
}
)wren";

/** @brief Marker of scene owners inside the extension owner space of `systems::ScriptSystem`. */
inline constexpr systems::ScriptOwner SCENE_OWNER_TAG =
    systems::ScriptSystem::EXTENSION_OWNER_BIT | (systems::ScriptOwner{0x5CE0} << 32U);

/** @brief Mask selecting the marker part of a scene owner. */
inline constexpr systems::ScriptOwner SCENE_OWNER_MASK = ~systems::ScriptOwner{0xFFFF};

/**
 * @brief Gets the script owner standing for a scene.
 * @param id Scene id.
 * @return Owner id given to the script system while the scene's Wren code runs.
 */
[[nodiscard]] constexpr systems::ScriptOwner sceneOwner(SceneId id) noexcept {
    return SCENE_OWNER_TAG | systems::ScriptOwner{id};
}

/**
 * @brief Gets the scene a script owner stands for.
 * @param owner Script owner.
 * @return The scene id, or nothing if the owner is not a scene.
 */
[[nodiscard]] constexpr std::optional<SceneId> sceneOfOwner(systems::ScriptOwner owner) noexcept {
    if ((owner & SCENE_OWNER_MASK) != SCENE_OWNER_TAG) {
        return std::nullopt;
    }
    return static_cast<SceneId>(owner & 0xFFFFU);
}

}  // namespace scene
