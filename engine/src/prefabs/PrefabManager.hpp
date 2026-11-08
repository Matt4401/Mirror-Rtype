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
#include "utils/MoveOnlyFunction.hpp"

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
     * @param name Prefab identifier.
     * @param buffer Command buffer receiving the deferred spawn.
     * @param params Instance parameters, merged over the registration defaults.
     * @return True if the spawn was queued, false if the prefab is unknown.
     */
    bool spawn(std::string_view name, ecs::CommandBuffer& buffer, PrefabParams const& params = {}) const;

    /** @brief Callback run on a prefab instance right after its factory built it. */
    using AfterSpawn = ecs::MoveOnlyFunction<void(ecs::Entity, ecs::Registry&)>;

    /**
     * @brief Queues the creation of a prefab instance, then runs `after` on it once the factory is done.
     *
     * Same as the other overload; `after` can tag or adjust the entity without knowing the prefab.
     * @param name Prefab identifier.
     * @param buffer Command buffer receiving the deferred spawn.
     * @param params Instance parameters, merged over the registration defaults.
     * @param after Callback run on the new entity; may be empty.
     * @return True if the spawn was queued, false if the prefab is unknown.
     */
    bool spawn(std::string_view name, ecs::CommandBuffer& buffer, PrefabParams const& params, AfterSpawn after) const;

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
