#pragma once
#include <algorithm>
#include <stdexcept>
#include <tuple>
#include <utility>

#include "ComponentType.hpp"
#include "ResourceType.hpp"
#include "ecs/Registry.hpp"

namespace ecs {

template <class Component>
void Registry::registerComponent() {
    (void)getComponents<Component>();
}

template <class Component>
SparseArray<Component>& Registry::getComponents() {
    const ComponentTypeId id = getComponentTypeId<Component>();
    if (id >= _componentsArrays.size()) {
        _componentsArrays.resize(id + 1);
    }
    if (!_componentsArrays[id]) {
        _componentsArrays[id] = std::make_unique<SparseArray<Component>>();
    }
    return *static_cast<SparseArray<Component>*>(_componentsArrays[id].get());
}

template <class Component>
SparseArray<Component> const& Registry::getComponents() const {
    const ComponentTypeId id = getComponentTypeId<Component>();
    if (id >= _componentsArrays.size() || !_componentsArrays[id]) {
        throw std::runtime_error("Component not registered");
    }
    return *static_cast<SparseArray<Component> const*>(_componentsArrays[id].get());
}

template <class Component>
bool Registry::hasComponent(Entity e) const {
    const ComponentTypeId id = getComponentTypeId<Component>();
    if (id >= _componentsArrays.size() || !_componentsArrays[id]) {
        return false;
    }
    return static_cast<SparseArray<Component> const*>(_componentsArrays[id].get())->contains(e);
}

template <class Component>
Component& Registry::addComponent(Entity to, Component&& c) {
    return getComponents<Component>().insertAt(to, std::forward<Component>(c));
}

template <class Component>
Component& Registry::addComponent(Entity to, Component const& c) {
    return getComponents<Component>().insertAt(to, c);
}

template <class Component, typename... Args>
Component& Registry::emplaceComponent(Entity to, Args&&... args) {
    return getComponents<Component>().insertAt(to, Component{std::forward<Args>(args)...});
}

template <class Component>
void Registry::removeComponent(Entity from) {
    const ComponentTypeId id = getComponentTypeId<Component>();
    if (id >= _componentsArrays.size() || !_componentsArrays[id]) {
        return;
    }
    auto& components = *static_cast<SparseArray<Component>*>(_componentsArrays[id].get());
    if (!components.contains(from)) {
        return;
    }
    components.erase(from);
    const auto componentRemoveListeners = _componentRemoveListeners;
    for (const auto& listenerEntry : componentRemoveListeners) {
        ComponentRemoveListener listener;
        for (const auto& registeredListener : _componentRemoveListeners) {
            if (registeredListener.first == listenerEntry.first) {
                listener = registeredListener.second;
                break;
            }
        }
        if (listener) {
            listener(from, id);
        }
    }
}

template <typename Resource, typename... Args>
Resource& Registry::emplaceResource(Args&&... args) {
    const ResourceTypeId id = getResourceTypeId<Resource>();
    if (id >= _resources.size()) {
        _resources.resize(id + 1);
    }
    _resources[id] = std::make_unique<detail::ResourceSlot<Resource>>(std::forward<Args>(args)...);
    return static_cast<detail::ResourceSlot<Resource>*>(_resources[id].get())->get();
}

template <typename Resource>
std::remove_cvref_t<Resource>& Registry::addResource(Resource&& resource) {
    using CleanType = std::remove_cvref_t<Resource>;
    return emplaceResource<CleanType>(std::forward<Resource>(resource));
}

template <typename Resource>
std::remove_cvref_t<Resource>& Registry::addResource(const Resource& resource) {
    using CleanType = std::remove_cvref_t<Resource>;
    return emplaceResource<CleanType>(resource);
}

template <typename Resource>
Resource& Registry::getResource() {
    const ResourceTypeId id = getResourceTypeId<Resource>();
    if (id >= _resources.size() || !_resources[id]) {
        throw std::runtime_error("Resource not registered in Registry");
    }
    return static_cast<detail::ResourceSlot<Resource>*>(_resources[id].get())->get();
}

template <typename Resource>
const Resource& Registry::getResource() const {
    const ResourceTypeId id = getResourceTypeId<Resource>();
    if (id >= _resources.size() || !_resources[id]) {
        throw std::runtime_error("Resource not registered in Registry");
    }
    return static_cast<const detail::ResourceSlot<Resource>*>(_resources[id].get())->get();
}

template <typename Resource>
bool Registry::hasResource() const noexcept {
    const ResourceTypeId id = getResourceTypeId<Resource>();
    return id < _resources.size() && _resources[id] != nullptr;
}

template <typename Resource>
void Registry::removeResource() {
    const ResourceTypeId id = getResourceTypeId<Resource>();
    if (id < _resources.size()) {
        _resources[id].reset();
    }
}

template <class... Components, class Func>
void Registry::runSystem(Func func) {
    if constexpr (sizeof...(Components) == 0) {
        return;
    } else {
        std::tuple<SparseArray<Components>&...> arrays{getComponents<Components>()...};
        auto& firstArray = std::get<0>(arrays);

        for (Entity const e : firstArray.getDenseEntities()) {
            if ((std::get<SparseArray<Components>&>(arrays).contains(e) && ...)) {
                func(e, std::get<SparseArray<Components>&>(arrays).get(e)...);
            }
        }
    }
}

}  // namespace ecs
