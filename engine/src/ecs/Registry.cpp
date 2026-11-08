#include "ecs/Registry.hpp"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <utility>

#include "ecs/Entity.hpp"

namespace ecs {

bool Registry::isAlive(Entity e) const noexcept {
    std::uint32_t const idx = entityIndex(e);
    return idx < _isAlive.size() && _isAlive[idx] && e == makeEntity(idx, _generations[idx]);
}

std::size_t Registry::aliveCount() const noexcept { return _nextEntity - _deadEntities.size(); }

void Registry::ensureAlive(Entity e) const {
    if (!isAlive(e)) {
        throw std::invalid_argument("Cannot attach a component to an entity that is not alive");
    }
}

Entity Registry::spawnEntity() {
    if (!_deadEntities.empty()) {
        std::uint32_t const idx = _deadEntities.front();
        _deadEntities.pop();
        _isAlive[idx] = true;
        return makeEntity(idx, _generations[idx]);
    }
    if (_nextEntity == MAX_ENTITY_INDEX) {
        throw std::length_error("Registry: entity slot indices exhausted");
    }
    std::uint32_t const idx = _nextEntity++;
    if (idx >= _isAlive.size()) {
        _isAlive.resize(static_cast<std::size_t>(idx) + 1, false);
        _generations.resize(static_cast<std::size_t>(idx) + 1, 0);
    }
    _isAlive[idx] = true;
    return makeEntity(idx, _generations[idx]);
}

void Registry::killEntity(Entity e) {
    if (!isAlive(e)) {
        return;
    }
    std::uint32_t const idx = entityIndex(e);
    _isAlive[idx] = false;

    // A throwing listener must not leave the entity half-destroyed: the other listeners still run, the components are
    // still removed and the slot still recycled, then the first exception is rethrown.
    std::exception_ptr listenerError;
    ++_destroyDispatchDepth;
    const std::size_t count = _destroyListeners.size();
    for (std::size_t i = 0; i < count; ++i) {
        if (_destroyListeners[i].active && _destroyListeners[i].callback) {
            try {
                _destroyListeners[i].callback(e);
            } catch (...) {
                if (!listenerError) {
                    listenerError = std::current_exception();
                }
            }
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
    _generations[idx] = (_generations[idx] + 1) & MAX_ENTITY_GENERATION;
    _deadEntities.push(idx);

    if (listenerError) {
        std::rethrow_exception(listenerError);
    }
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
