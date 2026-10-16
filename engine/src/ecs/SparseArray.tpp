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

    std::size_t const denseIdx = opt.value();
    std::size_t const lastDenseIdx = _components.size() - 1;
    Entity const lastEntity = _dense[lastDenseIdx];

    if (denseIdx != lastDenseIdx) {
        _components[denseIdx] = std::move(_components[lastDenseIdx]);
        _dense[denseIdx] = lastEntity;
        _sparse[lastEntity] = denseIdx;
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
