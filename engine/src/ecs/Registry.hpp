#pragma once
#include <memory>
#include <queue>
#include <stdexcept>
#include <tuple>
#include <typeindex>
#include <unordered_map>
#include <utility>

#include "Entity.hpp"
#include "SparseArray.hpp"

namespace ecs {
class Registry {
  public:
    Entity spawn_entity();
    void kill_entity(Entity e);

    template <class Component>
    void register_component() {
        _components_arrays[typeid(Component)] = std::make_unique<SparseArray<Component>>();
    }

    template <class Component>
    SparseArray<Component>& get_components() {
        auto it = _components_arrays.find(typeid(Component));
        if (it == _components_arrays.end()) {
            throw std::runtime_error("Component not registered");
        }
        return *static_cast<SparseArray<Component>*>(it->second.get());
    }

    template <class Component>
    Component& add_component(Entity to, Component&& c) {
        return get_components<Component>().insert_at(to, std::forward<Component>(c));
    }

    template <class Component>
    Component& add_component(Entity to, Component const& c) {
        return get_components<Component>().insert_at(to, c);
    }

    template <class... Components, class Func>
    void run_system(Func func) {
        using FirstComponent = std::tuple_element_t<0, std::tuple<Components...>>;
        auto& first_array = get_components<FirstComponent>();

        for (Entity const e : first_array.get_dense_entities()) {
            if ((get_components<Components>().contains(e) && ...)) {
                func(e, get_components<Components>().get(e)...);
            }
        }
    }

  private:
    std::unordered_map<std::type_index, std::unique_ptr<ISparseArray>> _components_arrays;
    Entity _next_entity = 0;
    std::queue<Entity> _dead_entities;
};
}  // namespace ecs
