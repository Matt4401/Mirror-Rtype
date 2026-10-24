#include "ecs/Registry.hpp"

#include <cstddef>
#include <utility>

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

    ++_destroyDispatchDepth;
    const std::size_t count = _destroyListeners.size();
    for (std::size_t i = 0; i < count; ++i) {
        if (_destroyListeners[i].active && _destroyListeners[i].callback) {
            _destroyListeners[i].callback(e);
        }
    }
    --_destroyDispatchDepth;
    if (_destroyDispatchDepth == 0) {
        std::erase_if(_destroyListeners, [](const auto& entry) { return !entry.active; });
    }

    for (auto const& array : _componentsArrays) {
        if (array) {
            array->erase(e);
        }
    }
    _deadEntities.push(e);
}

std::size_t Registry::addDestroyListener(DestroyListener listener) {
    const std::size_t listenerId = _nextDestroyListenerId++;
    _destroyListeners.push_back(
        DestroyListenerEntry{.id = listenerId, .callback = std::move(listener), .active = true});
    return listenerId;
}

void Registry::removeDestroyListener(std::size_t listenerId) {
    for (auto& entry : _destroyListeners) {
        if (entry.id == listenerId) {
            entry.active = false;
            break;
        }
    }
    if (_destroyDispatchDepth == 0) {
        std::erase_if(_destroyListeners, [](const auto& entry) { return !entry.active; });
    }
}

std::size_t Registry::addComponentRemoveListener(ComponentRemoveListener listener) {
    const std::size_t listenerId = _nextComponentRemoveListenerId++;
    _componentRemoveListeners.push_back(
        ComponentRemoveEntry{.id = listenerId, .callback = std::move(listener), .active = true});
    return listenerId;
}

void Registry::removeComponentRemoveListener(std::size_t listenerId) {
    for (auto& entry : _componentRemoveListeners) {
        if (entry.id == listenerId) {
            entry.active = false;
            break;
        }
    }
    if (_componentRemoveDispatchDepth == 0) {
        std::erase_if(_componentRemoveListeners, [](const auto& entry) { return !entry.active; });
    }
}

}  // namespace ecs
