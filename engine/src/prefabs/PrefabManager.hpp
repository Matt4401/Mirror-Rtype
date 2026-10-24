/**
 * @file PrefabManager.hpp
 * @brief Registry of named entity templates (prefabs) spawned through the CommandBuffer.
 */

#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "ecs/CommandBuffer.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "prefabs/PrefabParams.hpp"

namespace prefabs {

/**
 * @brief Maps prefab names to factories that attach a pre-configured set of components to an entity.
 *
 * Spawning never touches the Registry directly: it queues the factory into a `CommandBuffer`,
 * so prefabs can be requested from inside a system loop without invalidating sparse array iterators.
 *
 * Factories receive a `PrefabParams` bag instead of a fixed argument list: a prefab reads the keys it needs
 * (position, rotation, speed, size...) and uses its own defaults for the others.
 */
class PrefabManager {
  public:
    /**
     * @brief Callback configuring a freshly allocated entity.
     *
     * Invoked at `CommandBuffer::flush` time, once the entity exists in the Registry.
     */
    using Factory = std::function<void(ecs::Entity, ecs::Registry&, PrefabParams const&)>;

    /**
     * @brief Registers (or replaces) a prefab under the given name.
     *
     * The same factory can be registered several times with different defaults to create variants:
     * @code
     * manager.registerPrefab("obstacle", buildObstacle);
     * manager.registerPrefab("obstacle_wide", buildObstacle, {{"halfWidth", 128.0F}, {"halfHeight", 16.0F}});
     * @endcode
     *
     * @param name Identifier used by `spawn` (e.g. "missile").
     * @param factory Callback attaching the prefab components.
     * @param defaults Parameters applied to every spawn, overridden by the spawn parameters.
     */
    void registerPrefab(std::string_view name, Factory factory, PrefabParams defaults = {});

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
     * @warning The queued command references the registered factory: the manager must outlive the flush,
     * and re-registering the same name before the flush makes the pending spawn use the new factory.
     *
     * @param name Prefab identifier.
     * @param buffer Command buffer receiving the deferred spawn.
     * @param params Instance parameters, merged over the registration defaults.
     * @return True if the spawn was queued, false if the prefab is unknown.
     */
    bool spawn(std::string_view name, ecs::CommandBuffer& buffer, PrefabParams const& params = {}) const;

  private:
    /** @brief A registered factory and its default parameters. */
    struct Prefab {
        /** @brief Factory callback instantiating the entity with configured parameters. */
        Factory factory;
        /** @brief Default fallback parameters defined at registration. */
        PrefabParams defaults;
    };

    /** @brief Registered prefabs indexed by name. */
    std::unordered_map<std::string, Prefab, detail::StringHash, std::equal_to<>> _prefabs;
};

}  // namespace prefabs
