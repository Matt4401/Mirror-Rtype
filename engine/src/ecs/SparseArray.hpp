/*
** EPITECH PROJECT, 2026
** Rtype
** File description:
** SparseArray
*/

#pragma once
#include <vector>
#include <optional>
#include <stdexcept>
#include <utility>
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
            if (_sparse[e].has_value()) {
                _components[_sparse[e].value()] = c;
            } else {
                _sparse[e] = _components.size();
                _dense.push_back(e);
                _components.push_back(c);
            }
            return _components[_sparse[e].value()];
        }

        Component& insert_at(Entity e, Component&& c) {
            if (e >= _sparse.size()) {
                _sparse.resize(e + 1, std::nullopt);
            }
            if (_sparse[e].has_value()) {
                _components[_sparse[e].value()] = std::move(c);
            } else {
                _sparse[e] = _components.size();
                _dense.push_back(e);
                _components.push_back(std::move(c));
            }
            return _components[_sparse[e].value()];
        }

        void erase(Entity e) override {
            if (e >= _sparse.size() || !_sparse[e].has_value()) {
                return;
            }

            std::size_t dense_idx = _sparse[e].value();
            std::size_t last_dense_idx = _components.size() - 1;
            Entity last_entity = _dense[last_dense_idx];

            if (dense_idx != last_dense_idx) {
                _components[dense_idx] = std::move(_components[last_dense_idx]);
                _dense[dense_idx] = last_entity;
                _sparse[last_entity] = dense_idx;
            }

            _components.pop_back();
            _dense.pop_back();
            _sparse[e] = std::nullopt;
        }

        bool contains(Entity e) const {
            return e < _sparse.size() && _sparse[e].has_value();
        }

        Component& get(Entity e) {
            if (!contains(e)) {
                throw std::out_of_range("Component not found for entity");
            }
            return _components[_sparse[e].value()];
        }

        Component const& get(Entity e) const {
            if (!contains(e)) {
                throw std::out_of_range("Component not found for entity");
            }
            return _components[_sparse[e].value()];
        }

        std::vector<Component>& get_dense() { return _components; }
        std::vector<Component> const& get_dense() const { return _components; }

    private:
        std::vector<std::optional<std::size_t>> _sparse;
        std::vector<Entity> _dense;
        std::vector<Component> _components;
    };
}

