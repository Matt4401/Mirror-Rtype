/**
 * @file PrefabManager.hpp
 * @brief Registry of named entity templates (prefabs) spawned through the CommandBuffer.
 */

#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "ecs/CommandBuffer.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"

namespace prefabs {

/**
 * @brief Maps prefab names to factories that attach a pre-configured set of components to an entity.
 *
 * Spawning never touches the Registry directly: it queues the factory into a `CommandBuffer`,
 * so prefabs can be requested from inside a system loop without invalidating sparse array iterators.
 */
class PrefabManager {
  public:
    /**
     * @brief Callback configuring a freshly allocated entity at position (x, y).
     *
     * Invoked at `CommandBuffer::flush` time, once the entity exists in the Registry.
     */
    using Factory = std::function<void(ecs::Entity, ecs::Registry&, float x, float y)>;

    /**
     * @brief Registers (or replaces) a prefab under the given name.
     *
     * @param name Identifier used by `spawn` (e.g. "missile").
     * @param factory Callback attaching the prefab components.
     */
    void registerPrefab(std::string name, Factory factory);

    /**
     * @brief Checks whether a prefab is registered under the given name.
     * @param name Prefab identifier.
     * @return True if a factory exists for this name.
     */
    [[nodiscard]] bool has(std::string_view name) const;

    /**
     * @brief Queues the creation of a prefab instance into the command buffer.
     *
     * The entity is only created on the next `CommandBuffer::flush`.
     * Unknown names log a warning and queue nothing.
     *
     * @param name Prefab identifier.
     * @param buffer Command buffer receiving the deferred spawn.
     * @param x Initial horizontal position in world units.
     * @param y Initial vertical position in world units.
     * @return True if the spawn was queued, false if the prefab is unknown.
     */
    bool spawn(std::string_view name, ecs::CommandBuffer& buffer, float x, float y) const;

  private:
    /** @brief Transparent hash allowing `std::string_view` lookups without allocation. */
    struct StringHash {
        using is_transparent = void;
        [[nodiscard]] std::size_t operator()(std::string_view s) const noexcept {
            return std::hash<std::string_view>{}(s);
        }
    };

    /** @brief Registered factories indexed by prefab name. */
    std::unordered_map<std::string, Factory, StringHash, std::equal_to<>> _factories;
};

}  // namespace prefabs
