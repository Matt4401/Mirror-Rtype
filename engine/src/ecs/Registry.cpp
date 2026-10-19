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
    const auto destroyListeners = _destroyListeners;
    for (const auto& listenerEntry : destroyListeners) {
        DestroyListener listener;
        for (const auto& registeredListener : _destroyListeners) {
            if (registeredListener.first == listenerEntry.first) {
                listener = registeredListener.second;
                break;
            }
        }
        if (listener) {
            listener(e);
        }
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
    _destroyListeners.emplace_back(listenerId, std::move(listener));
    return listenerId;
}

void Registry::removeDestroyListener(std::size_t listenerId) {
    std::erase_if(_destroyListeners, [listenerId](const auto& listener) { return listener.first == listenerId; });
}

std::size_t Registry::addComponentRemoveListener(ComponentRemoveListener listener) {
    const std::size_t listenerId = _nextComponentRemoveListenerId++;
    _componentRemoveListeners.emplace_back(listenerId, std::move(listener));
    return listenerId;
}

void Registry::removeComponentRemoveListener(std::size_t listenerId) {
    std::erase_if(_componentRemoveListeners,
                  [listenerId](const auto& listener) { return listener.first == listenerId; });
}

}  // namespace ecs
