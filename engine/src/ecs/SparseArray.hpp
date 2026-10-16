#pragma once
#include <cstddef>
#include <optional>
#include <vector>

#include "Entity.hpp"

namespace ecs {

/**
 * @brief Type-erased base interface for component sparse arrays.
 *
 * Allows the ECS Registry to manage and clear component storage generically
 * (for example, when an entity is destroyed) without requiring compile-time knowledge
 * of all registered component types.
 */
class ISparseArray {
  public:
    /** @brief Default constructor for the type-erased sparse array interface. */
    ISparseArray() = default;
    /** @brief Virtual destructor. */
    virtual ~ISparseArray() = default;

    /** @brief Deleted copy constructor. */
    ISparseArray(const ISparseArray&) = delete;
    /** @brief Deleted copy assignment operator. */
    ISparseArray& operator=(const ISparseArray&) = delete;
    /** @brief Deleted move constructor. */
    ISparseArray(ISparseArray&&) = delete;
    /** @brief Deleted move assignment operator. */
    ISparseArray& operator=(ISparseArray&&) = delete;

    /**
     * @brief Removes the component associated with the given entity, if present.
     * @param e Entity whose component should be erased.
     */
    virtual void erase(Entity e) = 0;
};

/**
 * @brief Cache-friendly sparse set container mapping entities to component instances.
 *
 * Implements the Sparse Set pattern:
 * - A sparse vector indexed by Entity ID storing indices into the dense vectors ($O(1)$ lookup).
 * - A dense vector of components stored contiguously in memory for optimal cache locality during system iteration.
 * - A dense vector of entities mirroring component positions.
 * - Deletions use the "swap-and-pop" idiom in $O(1)$ time, maintaining dense contiguity.
 *
 * @tparam Component The component type stored in this array.
 */
template <typename Component>
class SparseArray : public ISparseArray {
  public:
    /** @brief Default constructor initializing an empty sparse set. */
    SparseArray() = default;
    /** @brief Override destructor releasing stored component arrays. */
    ~SparseArray() override = default;

    /** @brief Deleted copy constructor. */
    SparseArray(const SparseArray&) = delete;
    /** @brief Deleted copy assignment operator. */
    SparseArray& operator=(const SparseArray&) = delete;
    /** @brief Defaulted move constructor. */
    SparseArray(SparseArray&&) noexcept = default;
    /** @brief Defaulted move assignment operator. */
    SparseArray& operator=(SparseArray&&) noexcept = default;

    /**
     * @brief Inserts or replaces a component for an entity using copy semantics.
     *
     * @param e The entity to attach the component to.
     * @param c The component value to copy.
     * @return Reference to the stored component.
     */
    Component& insertAt(Entity e, Component const& c);

    /**
     * @brief Inserts or replaces a component for an entity using move semantics.
     *
     * @param e The entity to attach the component to.
     * @param c The component value to move.
     * @return Reference to the stored component.
     */
    Component& insertAt(Entity e, Component&& c);

    /**
     * @brief Erases the component associated with the given entity in O(1) time via swap-and-pop.
     *
     * @param e Entity to erase the component from.
     */
    void erase(Entity e) override;

    /**
     * @brief Checks if a component is attached to the given entity in O(1) time.
     *
     * @param e Entity to check.
     * @return True if the entity owns this component.
     */
    [[nodiscard]] bool contains(Entity e) const noexcept;

    /**
     * @brief Retrieves a mutable reference to the component for an entity.
     *
     * @param e Entity identifier.
     * @return Mutable reference to the component.
     * @throws std::out_of_range If the component does not exist for the entity.
     */
    Component& get(Entity e);

    /**
     * @brief Retrieves a const reference to the component for an entity.
     *
     * @param e Entity identifier.
     * @return Const reference to the component.
     * @throws std::out_of_range If the component does not exist for the entity.
     */
    [[nodiscard]] Component const& get(Entity e) const;

    /**
     * @brief Returns a mutable reference to the contiguous dense array of components.
     * @return Reference to the std::vector of components.
     */
    std::vector<Component>& getDense() noexcept;

    /**
     * @brief Returns a const reference to the contiguous dense array of components.
     * @return Const reference to the std::vector of components.
     */
    [[nodiscard]] std::vector<Component> const& getDense() const noexcept;

    /**
     * @brief Returns a const reference to the dense array of entity IDs matching components.
     * @return Const reference to the std::vector of entities.
     */
    [[nodiscard]] std::vector<Entity> const& getDenseEntities() const noexcept;

  private:
    /** @brief Sparse array indexed by Entity ID holding indices into the dense vectors. */
    std::vector<std::optional<std::size_t>> _sparse;
    /** @brief Dense array of active entity IDs corresponding to components. */
    std::vector<Entity> _dense;
    /** @brief Contiguous dense array storing packed component instances. */
    std::vector<Component> _components;
};

}  // namespace ecs

#include "SparseArray.tpp"  // IWYU pragma: export
