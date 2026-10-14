#include "ecs/Registry.hpp"

#include "ecs/Entity.hpp"

namespace ecs {

Entity Registry::spawnEntity() {
    if (!_deadEntities.empty()) {
        Entity const e = _deadEntities.front();
        _deadEntities.pop();
        return e;
    }
    return _nextEntity++;
}

void Registry::killEntity(Entity e) {
    for (auto const& array : _componentsArrays) {
        if (array) {
            array->erase(e);
        }
    }
    _deadEntities.push(e);
}

}  // namespace ecs
