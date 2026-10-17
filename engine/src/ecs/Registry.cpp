#include "ecs/Registry.hpp"

#include "ecs/Entity.hpp"

namespace ecs {

bool Registry::isAlive(Entity e) const noexcept { return e < _isAlive.size() && _isAlive[e]; }

Entity Registry::spawnEntity() {
    if (!_deadEntities.empty()) {
        Entity const e = _deadEntities.front();
        _deadEntities.pop();
        _isAlive[e] = true;
        return e;
    }
    Entity const e = _nextEntity++;
    if (e >= _isAlive.size()) {
        _isAlive.resize(e + 1, false);
    }
    _isAlive[e] = true;
    return e;
}

void Registry::killEntity(Entity e) {
    if (e >= _isAlive.size() || !_isAlive[e]) {
        return;
    }
    _isAlive[e] = false;
    for (auto const& array : _componentsArrays) {
        if (array) {
            array->erase(e);
        }
    }
    _deadEntities.push(e);
}

}  // namespace ecs
