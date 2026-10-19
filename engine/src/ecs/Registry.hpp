#pragma once
#include <cstddef>
#include <functional>
#include <memory>
#include <queue>
#include <type_traits>
#include <utility>
#include <vector>

#include "ComponentType.hpp"
#include "Entity.hpp"
#include "SparseArray.hpp"

namespace ecs {

namespace detail {

/**
 * @brief Base interface for type-erased resource storage slots.
 */
class IResourceSlot {
  public:
    /** @brief Default constructor. */
    IResourceSlot() = default;

    /** @brief Virtual destructor for type-erased cleanup. */
    virtual ~IResourceSlot() = default;

    /** @brief Deleted copy constructor. */
    IResourceSlot(const IResourceSlot&) = delete;

    /** @brief Deleted copy assignment operator. */
    IResourceSlot& operator=(const IResourceSlot&) = delete;

    /** @brief Defaulted move constructor. */
    IResourceSlot(IResourceSlot&&) noexcept = default;

    /** @brief Defaulted move assignment operator. */
    IResourceSlot& operator=(IResourceSlot&&) noexcept = default;
};

/**
 * @brief Typed resource slot holding an instance of Resource.
 * @tparam Resource Type of stored resource.
 */
template <typename Resource>
class ResourceSlot final : public IResourceSlot {
  public:
    /**
     * @brief In-place constructor forwarding arguments to Resource constructor.
     * @tparam Args Constructor argument types.
     * @param args Arguments forwarded to Resource.
     */
    template <typename... Args>
    explicit ResourceSlot(Args&&... args) : _resource(std::forward<Args>(args)...) {}

    /**
     * @brief Retrieves mutable reference to stored resource.
     * @return Reference to Resource.
     */
    [[nodiscard]] Resource& get() noexcept { return _resource; }

    /**
     * @brief Retrieves const reference to stored resource.
     * @return Const reference to Resource.
     */
    [[nodiscard]] const Resource& get() const noexcept { return _resource; }

  private:
    /** @brief Stored resource instance. */
    Resource _resource;
};

}  // namespace detail

/**
 * @brief Central coordinator for the Entity Component System (ECS).
 *
 * Responsibilities:
 * - Entity lifecycle: creates new entity IDs and recycles destroyed entities.
 * - Component storage: maintains a flat vector of type-erased `ISparseArray` containers indexed by dense
 * ComponentTypeId.
 * - Resource storage: maintains global unique singleton data instances indexed by dense ResourceTypeId.
 * - System execution: executes variadic queries across entities possessing all requested components.
 */
class Registry {
  public:
    /** @brief Callback invoked immediately before an entity's components are removed. */
    using DestroyListener = std::function<void(Entity)>;
    /** @brief Callback invoked after a component has been removed from an entity. */
    using ComponentRemoveListener = std::function<void(Entity, ComponentTypeId)>;

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
     * @brief Registers a callback invoked before an entity is destroyed.
     * @param listener Callback receiving the entity being destroyed.
     * @return Identifier used to unregister the callback.
     */
    std::size_t addDestroyListener(DestroyListener listener);

    /**
     * @brief Unregisters an entity-destruction callback.
     * @param listenerId Identifier returned by `addDestroyListener`.
     */
    void removeDestroyListener(std::size_t listenerId);

    /**
     * @brief Registers a callback invoked after a component is removed.
     * @param listener Callback receiving the entity and removed component type.
     * @return Identifier used to unregister the callback.
     */
    std::size_t addComponentRemoveListener(ComponentRemoveListener listener);

    /**
     * @brief Unregisters a component-removal callback.
     * @param listenerId Identifier returned by addComponentRemoveListener.
     */
    void removeComponentRemoveListener(std::size_t listenerId);

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
     * @brief Constructs and stores a singleton resource in-place.
     *
     * @tparam Resource Resource type.
     * @tparam Args Constructor argument types.
     * @param args Arguments forwarded to Resource constructor.
     * @return Reference to the newly constructed resource.
     */
    template <typename Resource, typename... Args>
    Resource& emplaceResource(Args&&... args);

    /**
     * @brief Stores a singleton resource using move semantics.
     *
     * @tparam Resource Resource type.
     * @param resource Resource rvalue to move into storage.
     * @return Reference to the stored resource.
     */
    template <typename Resource>
    std::remove_cvref_t<Resource>& addResource(Resource&& resource);

    /**
     * @brief Stores a singleton resource using copy semantics.
     *
     * @tparam Resource Resource type.
     * @param resource Resource lvalue to copy into storage.
     * @return Reference to the stored resource.
     */
    template <typename Resource>
    std::remove_cvref_t<Resource>& addResource(const Resource& resource);

    /**
     * @brief Retrieves a mutable reference to a registered resource.
     *
     * @tparam Resource Resource type to retrieve.
     * @return Mutable reference to the resource.
     * @throws std::runtime_error If the resource has not been registered.
     */
    template <typename Resource>
    [[nodiscard]] Resource& getResource();

    /**
     * @brief Retrieves a const reference to a registered resource.
     *
     * @tparam Resource Resource type to retrieve.
     * @return Const reference to the resource.
     * @throws std::runtime_error If the resource has not been registered.
     */
    template <typename Resource>
    [[nodiscard]] const Resource& getResource() const;

    /**
     * @brief Checks if a resource of the given type is currently registered.
     *
     * @tparam Resource Resource type to check.
     * @return True if the resource is present in the registry.
     */
    template <typename Resource>
    [[nodiscard]] bool hasResource() const noexcept;

    /**
     * @brief Removes a resource from the registry.
     *
     * @tparam Resource Resource type to remove.
     */
    template <typename Resource>
    void removeResource();

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
    /** @brief Vector of type-erased resource slots indexed by resource type ID. */
    std::vector<std::unique_ptr<detail::IResourceSlot>> _resources;
    /** @brief Next available entity ID counter when no recycled IDs exist. */
    Entity _nextEntity = 0;
    /** @brief Queue of recycled entity IDs from previously destroyed entities. */
    std::queue<Entity> _deadEntities;
    /** @brief Liveness tracking bitmap for entity IDs. */
    std::vector<bool> _isAlive;
    /** @brief Callbacks notified before an entity's components are removed. */
    std::vector<std::pair<std::size_t, DestroyListener>> _destroyListeners;
    /** @brief Identifier assigned to the next registered destruction callback. */
    std::size_t _nextDestroyListenerId = 0;
    /** @brief Callbacks notified after a component is explicitly removed. */
    std::vector<std::pair<std::size_t, ComponentRemoveListener>> _componentRemoveListeners;
    /** @brief Identifier assigned to the next registered component-removal callback. */
    std::size_t _nextComponentRemoveListenerId = 0;
};

}  // namespace ecs

#include "Registry.tpp"  // IWYU pragma: export
