#include "ecs/CommandBuffer.hpp"

namespace ecs {

void CommandBuffer::defer_destroy(Entity e) { _destroy_queue.push_back(e); }

void CommandBuffer::defer_spawn(std::move_only_function<void(Entity, Registry&)> initializer) {
    _spawn_queue.push_back(std::move(initializer));
}

void CommandBuffer::clear() noexcept {
    _destroy_queue.clear();
    _spawn_queue.clear();
}

[[nodiscard]] bool CommandBuffer::empty() const noexcept { return _destroy_queue.empty() && _spawn_queue.empty(); }

void CommandBuffer::flush(Registry& registry) {
    for (auto& init : _spawn_queue) {
        Entity const e = registry.spawn_entity();
        if (init) {
            init(e, registry);
        }
    }
    _spawn_queue.clear();

    for (Entity const e : _destroy_queue) {
        registry.kill_entity(e);
    }
    _destroy_queue.clear();
}

}  // namespace ecs
