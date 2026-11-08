/**
 * @file SceneManager.tpp
 * @brief Template implementations for SceneManager.
 */

#pragma once

#include <memory>
#include <stdexcept>
#include <type_traits>
#include <typeindex>
#include <utility>

#include "scene/SceneManager.hpp"  // NOLINT(misc-header-include-cycle)

namespace scene {

template <typename Scene, typename... Args>
SceneId SceneManager::registerScene(Args&&... args) {
    static_assert(std::is_base_of_v<IScene, Scene>, "Scene must derive from scene::IScene");
    return add(std::make_unique<Scene>(std::forward<Args>(args)...));
}

template <typename Scene>
Scene& SceneManager::get() {
    return static_cast<Scene&>(*slotOf(idOf<Scene>()).scene);
}

template <typename Scene>
SceneId SceneManager::idOf() const {
    const std::optional<SceneId> id = findType(std::type_index(typeid(Scene)));
    if (!id) {
        throw std::out_of_range("SceneManager: scene type not registered");
    }
    return *id;
}

}  // namespace scene
