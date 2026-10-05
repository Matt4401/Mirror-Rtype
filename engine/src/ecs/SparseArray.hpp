/*
** EPITECH PROJECT, 2026
** Rtype
** File description:
** SparseArray
*/

#pragma once
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "Entity.hpp"

namespace ecs {
class ISparseArray {
  public:
    ISparseArray() = default;
    virtual ~ISparseArray() = default;

    ISparseArray(const ISparseArray&) = delete;
    ISparseArray& operator=(const ISparseArray&) = delete;
    ISparseArray(ISparseArray&&) = delete;
    ISparseArray& operator=(ISparseArray&&) = delete;
    virtual void erase(Entity e) = 0;
};

template <typename Component>
class SparseArray : public ISparseArray {
  public:
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

    void erase(Entity e) override {
        if (e >= _sparse.size()) {
            return;
        }
        auto const& opt = _sparse[e];
        if (!opt.has_value()) {
            return;
        }

        std::size_t dense_idx = opt.value();
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

    [[nodiscard]] [[nodiscard]] bool contains(Entity e) const { return e < _sparse.size() && _sparse[e].has_value(); }

    Component& get(Entity e) {
        if (e >= _sparse.size()) {
            throw std::out_of_range("Component not found for entity");
        }
        if (auto const& opt = _sparse[e]; opt.has_value()) {
            return _components[opt.value()];
        }
        throw std::out_of_range("Component not found for entity");
    }

    [[nodiscard]] Component const& get(Entity e) const {
        if (e >= _sparse.size()) {
            throw std::out_of_range("Component not found for entity");
        }
        if (auto const& opt = _sparse[e]; opt.has_value()) {
            return _components[opt.value()];
        }
        throw std::out_of_range("Component not found for entity");
    }

    std::vector<Component>& get_dense() { return _components; }
    [[nodiscard]] std::vector<Component> const& get_dense() const { return _components; }
    [[nodiscard]] std::vector<Entity> const& get_dense_entities() const { return _dense; }

  private:
    std::vector<std::optional<std::size_t>> _sparse;
    std::vector<Entity> _dense;
    std::vector<Component> _components;
};
}  // namespace ecs
