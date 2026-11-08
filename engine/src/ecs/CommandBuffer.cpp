#include "ecs/CommandBuffer.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "utils/MoveOnlyFunction.hpp"

namespace ecs {

void CommandBuffer::deferDestroy(Entity e) { _destroyQueue.push_back(e); }

void CommandBuffer::deferSpawn(MoveOnlyFunction<void(Entity, Registry&)> initializer) {
    _spawnQueue.push_back(std::move(initializer));
}

void CommandBuffer::clear() noexcept {
    _destroyQueue.clear();
    _spawnQueue.clear();
}

[[nodiscard]] bool CommandBuffer::empty() const noexcept { return _destroyQueue.empty() && _spawnQueue.empty(); }

void CommandBuffer::flush(Registry& registry) {
    // Take the queues out first: an initializer or a destroy listener may queue into this buffer while it is
    // flushed, which would reallocate the vectors being walked. What they queue waits for the next flush.
    std::vector<MoveOnlyFunction<void(Entity, Registry&)>> spawns = std::exchange(_spawnQueue, {});
    std::vector<Entity> destroys = std::exchange(_destroyQueue, {});

    for (auto& init : spawns) {
        Entity const e = registry.spawnEntity();
        if (init) {
            init(e, registry);
        }
    }

    std::ranges::sort(destroys);
    auto const [first, last] = std::ranges::unique(destroys);
    destroys.erase(first, last);

    for (Entity const e : destroys) {
        registry.killEntity(e);
    }
}

}  // namespace ecs
