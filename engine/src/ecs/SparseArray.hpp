#pragma once
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <utility>
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
    ISparseArray() = default;
    virtual ~ISparseArray() = default;

    ISparseArray(const ISparseArray&) = delete;
    ISparseArray& operator=(const ISparseArray&) = delete;
    ISparseArray(ISparseArray&&) = delete;
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
    SparseArray() = default;
    ~SparseArray() override = default;

    SparseArray(const SparseArray&) = delete;
    SparseArray& operator=(const SparseArray&) = delete;
    SparseArray(SparseArray&&) noexcept = default;
    SparseArray& operator=(SparseArray&&) noexcept = default;

    /**
     * @brief Inserts or replaces a component for an entity using copy semantics.
     *
     * @param e The entity to attach the component to.
     * @param c The component value to copy.
     * @return Reference to the stored component.
     */
    Component& insert_at(Entity e, Component const& c) {
        if (e >= _sparse.size()) {
            _sparse.resize(e + 1, std::nullopt);
        }
        if (auto const& opt = _sparse[e]; opt.has_value()) {
            _components[opt.value()] = c;
            return _components[opt.value()];
        }
        _sparse[e] = _components.size();
        _dense.push_back(e);
        _components.push_back(c);
        return _components.back();
    }

    /**
     * @brief Inserts or replaces a component for an entity using move semantics.
     *
     * @param e The entity to attach the component to.
     * @param c The component value to move.
     * @return Reference to the stored component.
     */
    Component& insert_at(Entity e, Component&& c) {
        if (e >= _sparse.size()) {
            _sparse.resize(e + 1, std::nullopt);
        }
        if (auto const& opt = _sparse[e]; opt.has_value()) {
            _components[opt.value()] = std::move(c);
            return _components[opt.value()];
        }
        _sparse[e] = _components.size();
        _dense.push_back(e);
        _components.push_back(std::move(c));
        return _components.back();
    }

    /**
     * @brief Erases the component associated with the given entity in O(1) time via swap-and-pop.
     *
     * @param e Entity to erase the component from.
     */
    void erase(Entity e) override {
        if (e >= _sparse.size()) {
            return;
        }
        auto const& opt = _sparse[e];
        if (!opt.has_value()) {
            return;
        }

        std::size_t const dense_idx = opt.value();
        std::size_t const last_dense_idx = _components.size() - 1;
        Entity const last_entity = _dense[last_dense_idx];

        if (dense_idx != last_dense_idx) {
            _components[dense_idx] = std::move(_components[last_dense_idx]);
            _dense[dense_idx] = last_entity;
            _sparse[last_entity] = dense_idx;
        }

        _components.pop_back();
        _dense.pop_back();
        _sparse[e] = std::nullopt;
    }

    /**
     * @brief Checks if a component is attached to the given entity in O(1) time.
     *
     * @param e Entity to check.
     * @return True if the entity owns this component.
     */
    [[nodiscard]] bool contains(Entity e) const { return e < _sparse.size() && _sparse[e].has_value(); }

    /**
     * @brief Retrieves a mutable reference to the component for an entity.
     *
     * @param e Entity identifier.
     * @return Mutable reference to the component.
     * @throws std::out_of_range If the component does not exist for the entity.
     */
    Component& get(Entity e) {
        if (e >= _sparse.size()) {
            throw std::out_of_range("Component not found for entity");
        }
        if (auto const& opt = _sparse[e]; opt.has_value()) {
            return _components[opt.value()];
        }
        throw std::out_of_range("Component not found for entity");
    }

    /**
     * @brief Retrieves a const reference to the component for an entity.
     *
     * @param e Entity identifier.
     * @return Const reference to the component.
     * @throws std::out_of_range If the component does not exist for the entity.
     */
    [[nodiscard]] Component const& get(Entity e) const {
        if (e >= _sparse.size()) {
            throw std::out_of_range("Component not found for entity");
        }
        if (auto const& opt = _sparse[e]; opt.has_value()) {
            return _components[opt.value()];
        }
        throw std::out_of_range("Component not found for entity");
    }

    /**
     * @brief Returns a mutable reference to the contiguous dense array of components.
     * @return Reference to the std::vector of components.
     */
    std::vector<Component>& get_dense() { return _components; }

    /**
     * @brief Returns a const reference to the contiguous dense array of components.
     * @return Const reference to the std::vector of components.
     */
    [[nodiscard]] std::vector<Component> const& get_dense() const { return _components; }

    /**
     * @brief Returns a const reference to the dense array of entity IDs matching components.
     * @return Const reference to the std::vector of entities.
     */
    [[nodiscard]] std::vector<Entity> const& get_dense_entities() const { return _dense; }

  private:
    std::vector<std::optional<std::size_t>> _sparse;
    std::vector<Entity> _dense;
    std::vector<Component> _components;
};

}  // namespace ecs
