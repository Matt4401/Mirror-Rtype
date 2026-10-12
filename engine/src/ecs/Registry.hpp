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

/**
 * @brief Central coordinator for the Entity Component System (ECS).
 *
 * Responsibilities:
 * - Entity lifecycle: creates new entity IDs and recycles destroyed entities.
 * - Component storage: maintains a map of type-erased `ISparseArray` containers for each registered component.
 * - System execution: executes variadic queries across entities possessing all requested components.
 */
class Registry {
  public:
    Registry() = default;
    ~Registry() = default;

    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;
    Registry(Registry&&) noexcept = default;
    Registry& operator=(Registry&&) noexcept = default;

    /**
     * @brief Spawns a new entity, recycling a previously destroyed entity ID if available.
     * @return Newly created or recycled Entity ID.
     */
    Entity spawn_entity();

    /**
     * @brief Kills an entity, removing all its components across all sparse arrays and recycling its ID.
     * @param e Entity to destroy.
     */
    void kill_entity(Entity e);

    /**
     * @brief Registers a new component type within the ECS world.
     *
     * Allocates a dedicated `SparseArray<Component>` for storing instances of this type.
     * Must be called before adding or querying components of this type.
     *
     * @tparam Component The component aggregate/struct type to register.
     */
    template <class Component>
    void register_component() {
        _components_arrays[typeid(Component)] = std::make_unique<SparseArray<Component>>();
    }

    /**
     * @brief Retrieves the sparse array container for a registered component type.
     *
     * @tparam Component The component type to look up.
     * @return Reference to the `SparseArray<Component>` container.
     * @throws std::runtime_error If the component type has not been registered.
     */
    template <class Component>
    SparseArray<Component>& get_components() {
        auto it = _components_arrays.find(typeid(Component));
        if (it == _components_arrays.end()) {
            throw std::runtime_error("Component not registered");
        }
        return *static_cast<SparseArray<Component>*>(it->second.get());
    }

    /**
     * @brief Attaches a component to an entity using move semantics.
     *
     * @tparam Component Component type.
     * @param to Entity to attach the component to.
     * @param c Component rvalue to move into storage.
     * @return Reference to the newly stored component.
     */
    template <class Component>
    Component& add_component(Entity to, Component&& c) {
        return get_components<Component>().insert_at(to, std::forward<Component>(c));
    }

    /**
     * @brief Attaches a component to an entity using copy semantics.
     *
     * @tparam Component Component type.
     * @param to Entity to attach the component to.
     * @param c Component lvalue to copy into storage.
     * @return Reference to the newly stored component.
     */
    template <class Component>
    Component& add_component(Entity to, Component const& c) {
        return get_components<Component>().insert_at(to, c);
    }

    /**
     * @brief Executes a system callback over all entities possessing a specific set of components.
     *
     * Utilizes the dense entity array of the first component as the iteration driver,
     * verifying presence in remaining component sparse arrays in O(1) time.
     *
     * Example:
     * @code
     * registry.run_system<Position, Velocity>([](Entity e, Position& pos, Velocity& vel) {
     *     pos.x += vel.vx;
     *     pos.y += vel.vy;
     * });
     * @endcode
     *
     * @tparam Components Parameter pack of component types required for the query.
     * @tparam Func Callable signature matching `void(Entity, Components&...)`.
     * @param func Callable executed for each matching entity.
     */
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
