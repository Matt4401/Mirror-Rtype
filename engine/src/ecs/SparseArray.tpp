#pragma once
#include <stdexcept>

#include "ecs/SparseArray.hpp"

namespace ecs {

template <typename Component>
Component& SparseArray<Component>::insertAt(Entity e, Component const& c) {
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

template <typename Component>
Component& SparseArray<Component>::insertAt(Entity e, Component&& c) {
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

template <typename Component>
void SparseArray<Component>::erase(Entity e) {
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

template <typename Component>
bool SparseArray<Component>::contains(Entity e) const noexcept {
    return e < _sparse.size() && _sparse[e].has_value();
}

template <typename Component>
Component& SparseArray<Component>::get(Entity e) {
    if (e >= _sparse.size()) {
        throw std::out_of_range("Component not found for entity");
    }
    if (auto const& opt = _sparse[e]; opt.has_value()) {
        return _components[opt.value()];
    }
    throw std::out_of_range("Component not found for entity");
}

template <typename Component>
Component const& SparseArray<Component>::get(Entity e) const {
    if (e >= _sparse.size()) {
        throw std::out_of_range("Component not found for entity");
    }
    if (auto const& opt = _sparse[e]; opt.has_value()) {
        return _components[opt.value()];
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
