#pragma once
#include <functional>
#include <vector>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"

namespace ecs {

/**
 * @brief Thread-safe deferred command buffer for recording structural ECS mutations.
 *
 * Allows scripts and systems executing within iteration loops to safely schedule
 * entity destruction and creation without invalidating sparse arrays or iterating views.
 */
class CommandBuffer {
  public:
    CommandBuffer() = default;
    ~CommandBuffer() = default;

    CommandBuffer(const CommandBuffer&) = delete;
    CommandBuffer& operator=(const CommandBuffer&) = delete;
    CommandBuffer(CommandBuffer&&) noexcept = default;
    CommandBuffer& operator=(CommandBuffer&&) noexcept = default;

    /**
     * @brief Defers the destruction of an entity until the next flush.
     *
     * @param e Entity to kill.
     */
    void deferDestroy(Entity e);

    /**
     * @brief Defers the spawning of a new entity with an optional initialization lambda.
     *
     * @param initializer Move-only callback invoked with the newly allocated entity and registry.
     */
    void deferSpawn(std::move_only_function<void(Entity, Registry&)> initializer = nullptr);

    /**
     * @brief Flushes all queued commands into the provided ECS Registry.
     *
     * Spawns all queued entities, invokes initializers, and destroys scheduled entities.
     * Queues are cleared upon completion.
     *
     * @param registry Active ECS Registry to apply mutations to.
     */
    void flush(Registry& registry);

    /**
     * @brief Clears all pending commands without applying them.
     */
    void clear() noexcept;

    /**
     * @brief Checks if there are any pending deferred commands.
     * @return True if no commands are queued.
     */
    [[nodiscard]] bool empty() const noexcept;

  private:
    std::vector<Entity> _destroyQueue;
    std::vector<std::move_only_function<void(Entity, Registry&)>> _spawnQueue;
};

}  // namespace ecs
