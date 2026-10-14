#include "ecs/CommandBuffer.hpp"

namespace ecs {

void CommandBuffer::deferDestroy(Entity e) { _destroyQueue.push_back(e); }

void CommandBuffer::deferSpawn(std::move_only_function<void(Entity, Registry&)> initializer) {
    _spawnQueue.push_back(std::move(initializer));
}

void CommandBuffer::clear() noexcept {
    _destroyQueue.clear();
    _spawnQueue.clear();
}

[[nodiscard]] bool CommandBuffer::empty() const noexcept { return _destroyQueue.empty() && _spawnQueue.empty(); }

void CommandBuffer::flush(Registry& registry) {
    for (auto& init : _spawnQueue) {
        Entity const e = registry.spawnEntity();
        if (init) {
            init(e, registry);
        }
    }
    _spawnQueue.clear();

    for (Entity const e : _destroyQueue) {
        registry.killEntity(e);
    }
    _destroyQueue.clear();
}

}  // namespace ecs
