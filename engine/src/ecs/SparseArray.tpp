#pragma once
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <utility>

#include "ecs/SparseArray.hpp"

namespace ecs {

template <typename Component>
std::optional<std::size_t> SparseArray<Component>::denseIndexOf(Entity e) const noexcept {
    std::size_t const idx = entityIndex(e);
    if (idx >= _sparse.size()) {
        return std::nullopt;
    }
    std::optional<std::size_t> const denseIdx = _sparse[idx];
    if (!denseIdx.has_value() || _dense[*denseIdx] != e) {
        return std::nullopt;
    }
    return denseIdx;
}

template <typename Component>
template <typename Value>
Component& SparseArray<Component>::insertImpl(Entity e, Value&& c) {
    std::size_t const idx = entityIndex(e);
    if (idx >= _sparse.size()) {
        _sparse.resize(idx + 1, std::nullopt);
    }
    if (std::optional<std::size_t> const denseIdx = _sparse[idx]; denseIdx.has_value()) {
        if (_dense[*denseIdx] != e) {
            throw std::invalid_argument("SparseArray: slot is owned by another handle of the same entity index");
        }
        _components[*denseIdx] = std::forward<Value>(c);
        return _components[*denseIdx];
    }
    _sparse[idx] = _components.size();
    _dense.push_back(e);
    _components.push_back(std::forward<Value>(c));
    return _components.back();
}

template <typename Component>
Component& SparseArray<Component>::insertAt(Entity e, Component const& c) {
    return insertImpl(e, c);
}

template <typename Component>
Component& SparseArray<Component>::insertAt(Entity e, Component&& c) {
    return insertImpl(e, std::move(c));
}

template <typename Component>
void SparseArray<Component>::erase(Entity e) {
    std::optional<std::size_t> const found = denseIndexOf(e);
    if (!found.has_value()) {
        return;
    }

    std::size_t const denseIdx = found.value();
    std::size_t const lastDenseIdx = _components.size() - 1;
    Entity const lastEntity = _dense[lastDenseIdx];

    if (denseIdx != lastDenseIdx) {
        _components[denseIdx] = std::move(_components[lastDenseIdx]);
        _dense[denseIdx] = lastEntity;
        _sparse[entityIndex(lastEntity)] = denseIdx;
    }

    _components.pop_back();
    _dense.pop_back();
    _sparse[entityIndex(e)] = std::nullopt;
}

template <typename Component>
bool SparseArray<Component>::contains(Entity e) const noexcept {
    return denseIndexOf(e).has_value();
}

template <typename Component>
Component& SparseArray<Component>::get(Entity e) {
    if (std::optional<std::size_t> const found = denseIndexOf(e); found.has_value()) {
        return _components[found.value()];
    }
    throw std::out_of_range("Component not found for entity");
}

template <typename Component>
Component const& SparseArray<Component>::get(Entity e) const {
    if (std::optional<std::size_t> const found = denseIndexOf(e); found.has_value()) {
        return _components[found.value()];
    }
    throw std::out_of_range("Component not found for entity");
}

template <typename Component>
std::vector<Component>& SparseArray<Component>::getDense() noexcept {
    return _components;
}

template <typename Component>
std::vector<Component> const& SparseArray<Component>::getDense() const noexcept {
    return _components;
}

template <typename Component>
std::vector<Entity> const& SparseArray<Component>::getDenseEntities() const noexcept {
    return _dense;
}

}  // namespace ecs
