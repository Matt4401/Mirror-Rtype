#pragma once
#include <memory>
#include <queue>
#include <vector>

#include "Entity.hpp"
#include "SparseArray.hpp"

namespace ecs {

/**
 * @brief Central coordinator for the Entity Component System (ECS).
 *
 * Responsibilities:
 * - Entity lifecycle: creates new entity IDs and recycles destroyed entities.
 * - Component storage: maintains a flat vector of type-erased `ISparseArray` containers indexed by dense
 * ComponentTypeId.
 * - System execution: executes variadic queries across entities possessing all requested components.
 */
class Registry {
  public:
    /** @brief Default constructor initializing an empty ECS registry. */
    Registry() = default;
    /** @brief Default destructor releasing component sparse arrays. */
    ~Registry() = default;

    /** @brief Deleted copy constructor. */
    Registry(const Registry&) = delete;
    /** @brief Deleted copy assignment operator. */
    Registry& operator=(const Registry&) = delete;
    /** @brief Defaulted move constructor. */
    Registry(Registry&&) noexcept = default;
    /** @brief Defaulted move assignment operator. */
    Registry& operator=(Registry&&) noexcept = default;

    /**
     * @brief Spawns a new entity, recycling a previously destroyed entity ID if available.
     * @return Newly created or recycled Entity ID.
     */
    Entity spawnEntity();

    /**
     * @brief Kills an entity, removing all its components across all sparse arrays and recycling its ID.
     * @param e Entity to destroy.
     */
    void killEntity(Entity e);

    /**
     * @brief Checks if an entity is currently alive.
     * @param e Entity to query.
     * @return True if the entity is active and not destroyed.
     */
    [[nodiscard]] bool isAlive(Entity e) const noexcept;

    /**
     * @brief Registers or pre-allocates storage for a component type.
     *
     * Note: In this modernized registry, components are also automatically registered
     * on first access, making this call optional.
     *
     * @tparam Component The component aggregate/struct type to register.
     */
    template <class Component>
    void registerComponent();

    /**
     * @brief Retrieves the mutable sparse array container for a component type.
     *
     * Automatically initializes storage if not previously accessed.
     *
     * @tparam Component The component type to look up.
     * @return Reference to the `SparseArray<Component>` container.
     */
    template <class Component>
    SparseArray<Component>& getComponents();

    /**
     * @brief Retrieves the const sparse array container for a component type.
     *
     * @tparam Component The component type to look up.
     * @return Const reference to the `SparseArray<Component>` container.
     * @throws std::runtime_error If the component type has not yet been registered or accessed.
     */
    template <class Component>
    [[nodiscard]] SparseArray<Component> const& getComponents() const;

    /**
     * @brief Checks if an entity currently has a component attached.
     *
     * @tparam Component Component type to check.
     * @param e Entity to query.
     * @return True if the entity owns this component.
     */
    template <class Component>
    [[nodiscard]] bool hasComponent(Entity e) const;

    /**
     * @brief Attaches a component to an entity using move semantics.
     *
     * @tparam Component Component type.
     * @param to Entity to attach the component to.
     * @param c Component rvalue to move into storage.
     * @return Reference to the newly stored component.
     */
    template <class Component>
    Component& addComponent(Entity to, Component&& c);

    /**
     * @brief Attaches a component to an entity using copy semantics.
     *
     * @tparam Component Component type.
     * @param to Entity to attach the component to.
     * @param c Component lvalue to copy into storage.
     * @return Reference to the newly stored component.
     */
    template <class Component>
    Component& addComponent(Entity to, Component const& c);

    /**
     * @brief Constructs and attaches a component in-place on an entity.
     *
     * @tparam Component Component type.
     * @tparam Args Constructor argument types.
     * @param to Entity to attach the component to.
     * @param args Arguments forwarded to Component constructor.
     * @return Reference to the newly constructed component.
     */
    template <class Component, typename... Args>
    Component& emplaceComponent(Entity to, Args&&... args);

    /**
     * @brief Removes a component from an entity without killing the entity.
     *
     * @tparam Component Component type to remove.
     * @param from Entity to remove the component from.
     */
    template <class Component>
    void removeComponent(Entity from);

    /**
     * @brief Executes a system callback over all entities possessing a specific set of components.
     *
     * Utilizes the dense entity array of the first component as the iteration driver,
     * verifying presence in remaining component sparse arrays in O(1) time.
     * All component array references are hoisted outside the loop for zero overhead.
     *
     * Example:
     * @code
     * registry.runSystem<Position, Velocity>([](Entity e, Position& pos, Velocity& vel) {
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
    void runSystem(Func func);

  private:
    /** @brief Vector of type-erased sparse array containers indexed by component type ID. */
    std::vector<std::unique_ptr<ISparseArray>> _componentsArrays;
    /** @brief Next available entity ID counter when no recycled IDs exist. */
    Entity _nextEntity = 0;
    /** @brief Queue of recycled entity IDs from previously destroyed entities. */
    std::queue<Entity> _deadEntities;
    /** @brief Liveness tracking bitmap for entity IDs. */
    std::vector<bool> _isAlive;
};

}  // namespace ecs

#include "Registry.tpp"  // IWYU pragma: export
