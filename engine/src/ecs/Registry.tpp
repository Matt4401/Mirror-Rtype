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
    if (id >= _components_arrays.size()) {
        _components_arrays.resize(id + 1);
    }
    if (!_components_arrays[id]) {
        _components_arrays[id] = std::make_unique<SparseArray<Component>>();
    }
    return *static_cast<SparseArray<Component>*>(_components_arrays[id].get());
}

template <class Component>
SparseArray<Component> const& Registry::getComponents() const {
    const ComponentTypeId id = getComponentTypeId<Component>();
    if (id >= _components_arrays.size() || !_components_arrays[id]) {
        throw std::runtime_error("Component not registered");
    }
    return *static_cast<SparseArray<Component> const*>(_components_arrays[id].get());
}

template <class Component>
bool Registry::hasComponent(Entity e) const {
    const ComponentTypeId id = getComponentTypeId<Component>();
    if (id >= _components_arrays.size() || !_components_arrays[id]) {
        return false;
    }
    return static_cast<SparseArray<Component> const*>(_components_arrays[id].get())->contains(e);
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
    if (id < _components_arrays.size() && _components_arrays[id]) {
        _components_arrays[id]->erase(from);
    }
}

template <class... Components, class Func>
void Registry::runSystem(Func func) {
    if constexpr (sizeof...(Components) == 0) {
        return;
    } else {
        std::tuple<SparseArray<Components>&...> arrays{getComponents<Components>()...};
        auto& first_array = std::get<0>(arrays);

        for (Entity const e : first_array.getDenseEntities()) {
            if ((std::get<SparseArray<Components>&>(arrays).contains(e) && ...)) {
                func(e, std::get<SparseArray<Components>&>(arrays).get(e)...);
            }
        }
    }
}

}  // namespace ecs
