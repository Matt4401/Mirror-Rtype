#pragma once
#include <stdexcept>
#include <tuple>
#include <utility>

#include "ComponentType.hpp"
#include "ecs/Registry.hpp"

namespace ecs {

template <class Component>
void Registry::register_component() {
    (void)get_components<Component>();
}

template <class Component>
SparseArray<Component>& Registry::get_components() {
    const ComponentTypeId id = get_component_type_id<Component>();
    if (id >= _components_arrays.size()) {
        _components_arrays.resize(id + 1);
    }
    if (!_components_arrays[id]) {
        _components_arrays[id] = std::make_unique<SparseArray<Component>>();
    }
    return *static_cast<SparseArray<Component>*>(_components_arrays[id].get());
}

template <class Component>
SparseArray<Component> const& Registry::get_components() const {
    const ComponentTypeId id = get_component_type_id<Component>();
    if (id >= _components_arrays.size() || !_components_arrays[id]) {
        throw std::runtime_error("Component not registered");
    }
    return *static_cast<SparseArray<Component> const*>(_components_arrays[id].get());
}

template <class Component>
bool Registry::has_component(Entity e) const {
    const ComponentTypeId id = get_component_type_id<Component>();
    if (id >= _components_arrays.size() || !_components_arrays[id]) {
        return false;
    }
    return static_cast<SparseArray<Component> const*>(_components_arrays[id].get())->contains(e);
}

template <class Component>
Component& Registry::add_component(Entity to, Component&& c) {
    return get_components<Component>().insert_at(to, std::forward<Component>(c));
}

template <class Component>
Component& Registry::add_component(Entity to, Component const& c) {
    return get_components<Component>().insert_at(to, c);
}

template <class Component, typename... Args>
Component& Registry::emplace_component(Entity to, Args&&... args) {
    return get_components<Component>().insert_at(to, Component{std::forward<Args>(args)...});
}

template <class Component>
void Registry::remove_component(Entity from) {
    const ComponentTypeId id = get_component_type_id<Component>();
    if (id < _components_arrays.size() && _components_arrays[id]) {
        _components_arrays[id]->erase(from);
    }
}

template <class... Components, class Func>
void Registry::run_system(Func func) {
    if constexpr (sizeof...(Components) == 0) {
        return;
    } else {
        std::tuple<SparseArray<Components>&...> arrays{get_components<Components>()...};
        auto& first_array = std::get<0>(arrays);

        for (Entity const e : first_array.get_dense_entities()) {
            if ((std::get<SparseArray<Components>&>(arrays).contains(e) && ...)) {
                func(e, std::get<SparseArray<Components>&>(arrays).get(e)...);
            }
        }
    }
}

}  // namespace ecs
