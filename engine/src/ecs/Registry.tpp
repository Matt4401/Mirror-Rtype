#pragma once
#include "ecs/Registry.hpp"

namespace ecs {

template <class Component>
void Registry::register_component() {
    _components_arrays[typeid(Component)] = std::make_unique<SparseArray<Component>>();
}

template <class Component>
SparseArray<Component>& Registry::get_components() {
    auto it = _components_arrays.find(typeid(Component));
    if (it == _components_arrays.end()) {
        throw std::runtime_error("Component not registered");
    }
    return *static_cast<SparseArray<Component>*>(it->second.get());
}

template <class Component>
Component& Registry::add_component(Entity to, Component&& c) {
    return get_components<Component>().insert_at(to, std::forward<Component>(c));
}

template <class Component>
Component& Registry::add_component(Entity to, Component const& c) {
    return get_components<Component>().insert_at(to, c);
}

template <class... Components, class Func>
void Registry::run_system(Func func) {
    using FirstComponent = std::tuple_element_t<0, std::tuple<Components...>>;
    auto& first_array = get_components<FirstComponent>();

    for (Entity const e : first_array.get_dense_entities()) {
        if ((get_components<Components>().contains(e) && ...)) {
            func(e, get_components<Components>().get(e)...);
        }
    }
}

}  // namespace ecs
