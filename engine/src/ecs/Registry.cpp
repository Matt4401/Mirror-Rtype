/*
** EPITECH PROJECT, 2026
** Rtype
** File description:
** Registry
*/

#include "ecs/Registry.hpp"

#include "ecs/Entity.hpp"

namespace ecs {

Entity Registry::spawn_entity() {
    if (!_dead_entities.empty()) {
        Entity const e = _dead_entities.front();
        _dead_entities.pop();
        return e;
    }
    return _next_entity++;
}

void Registry::kill_entity(Entity e) {
    for (auto& [type, array] : _components_arrays) {
        array->erase(e);
    }
    _dead_entities.push(e);
}

}  // namespace ecs
