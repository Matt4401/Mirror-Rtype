#include "ecs/Registry.hpp"

#include "ecs/Entity.hpp"

namespace ecs {

Entity Registry::spawnEntity() {
    if (!_dead_entities.empty()) {
        Entity const e = _dead_entities.front();
        _dead_entities.pop();
        return e;
    }
    return _next_entity++;
}

void Registry::killEntity(Entity e) {
    for (auto const& array : _components_arrays) {
        if (array) {
            array->erase(e);
        }
    }
    _dead_entities.push(e);
}

}  // namespace ecs
