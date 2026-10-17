#pragma once
#include <stdexcept>
#include <tuple>
#include <utility>

#include "ComponentType.hpp"
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
    if (id < _componentsArrays.size() && _componentsArrays[id]) {
        _componentsArrays[id]->erase(from);
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
