#pragma once
#include <array>
#include <cstddef>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

#include "ComponentType.hpp"
#include "ResourceType.hpp"
#include "ecs/Registry.hpp"

namespace ecs {

namespace detail {

template <std::size_t DriverIdx, class TargetComponent, class... Components>
decltype(auto) getComponentFor(std::tuple<SparseArray<Components>&...>& arrays, Entity e, std::size_t driverIndex) {
    using DriverComponent = std::tuple_element_t<DriverIdx, std::tuple<Components...>>;
    if constexpr (std::is_same_v<TargetComponent, DriverComponent>) {
        (void)e;
        return std::get<DriverIdx>(arrays).getDense()[driverIndex];
    } else {
        (void)driverIndex;
        return std::get<SparseArray<TargetComponent>&>(arrays).get(e);
    }
}

template <std::size_t DriverIdx, class... Components, std::size_t... Is>
bool checkOtherContains(const std::tuple<SparseArray<Components>&...>& arrays, Entity e, std::index_sequence<Is...>) {
    return ((Is == DriverIdx || std::get<Is>(arrays).contains(e)) && ...);
}

template <std::size_t DriverIdx, class... Components, class Func>
void runSystemWithDriver(std::tuple<SparseArray<Components>&...>& arrays, Func& func) {
    auto& driverArray = std::get<DriverIdx>(arrays);
    const auto& driverEntities = driverArray.getDenseEntities();
    auto& driverComponents = driverArray.getDense();
    const std::size_t count = driverEntities.size();

    for (std::size_t i = 0; i < count; ++i) {
        const Entity e = driverEntities[i];
        if (checkOtherContains<DriverIdx, Components...>(arrays, e, std::index_sequence_for<Components...>{})) {
            func(e, getComponentFor<DriverIdx, Components>(arrays, e, i)...);
        }
    }
}

}  // namespace detail

template <class Component>
void Registry::registerComponent() {
    (void)getComponents<Component>();
}

template <class Component>
SparseArray<Component>& Registry::getComponents() {
    const ComponentTypeId id = getComponentTypeId<Component>();
    if (id >= _componentsArrays.size()) {
        _componentsArrays.resize(id + 1);
    }
    if (!_componentsArrays[id]) {
        _componentsArrays[id] = std::make_unique<SparseArray<Component>>();
    }
    return *static_cast<SparseArray<Component>*>(_componentsArrays[id].get());
}

template <class Component>
SparseArray<Component> const& Registry::getComponents() const {
    const ComponentTypeId id = getComponentTypeId<Component>();
    if (id >= _componentsArrays.size() || !_componentsArrays[id]) {
        throw std::runtime_error("Component not registered");
    }
    return *static_cast<SparseArray<Component> const*>(_componentsArrays[id].get());
}

template <class Component>
bool Registry::hasComponent(Entity e) const {
    const ComponentTypeId id = getComponentTypeId<Component>();
    if (id >= _componentsArrays.size() || !_componentsArrays[id]) {
        return false;
    }
    return static_cast<SparseArray<Component> const*>(_componentsArrays[id].get())->contains(e);
}

template <class Component>
Component& Registry::addComponent(Entity to, Component&& c) {
    return getComponents<Component>().insertAt(to, std::forward<Component>(c));
}

template <class Component>
Component& Registry::addComponent(Entity to, Component const& c) {
    return getComponents<Component>().insertAt(to, c);
}

template <class Component, typename... Args>
Component& Registry::emplaceComponent(Entity to, Args&&... args) {
    return getComponents<Component>().insertAt(to, Component{std::forward<Args>(args)...});
}

template <class Component>
void Registry::removeComponent(Entity from) {
    const ComponentTypeId id = getComponentTypeId<Component>();
    if (id >= _componentsArrays.size() || !_componentsArrays[id]) {
        return;
    }
    auto& components = *static_cast<SparseArray<Component>*>(_componentsArrays[id].get());
    if (!components.contains(from)) {
        return;
    }
    components.erase(from);
    ++_componentRemoveDispatchDepth;
    const std::size_t count = _componentRemoveListeners.size();
    for (std::size_t i = 0; i < count; ++i) {
        if (_componentRemoveListeners[i].active && _componentRemoveListeners[i].callback) {
            _componentRemoveListeners[i].callback(from, id);
        }
    }
    --_componentRemoveDispatchDepth;
    if (_componentRemoveDispatchDepth == 0) {
        std::erase_if(_componentRemoveListeners, [](const auto& entry) { return !entry.active; });
    }
}

template <typename Resource, typename... Args>
Resource& Registry::emplaceResource(Args&&... args) {
    const ResourceTypeId id = getResourceTypeId<Resource>();
    if (id >= _resources.size()) {
        _resources.resize(id + 1);
    }
    _resources[id] = std::make_unique<detail::ResourceSlot<Resource>>(std::forward<Args>(args)...);
    return static_cast<detail::ResourceSlot<Resource>*>(_resources[id].get())->get();
}

template <typename Resource>
std::remove_cvref_t<Resource>& Registry::addResource(Resource&& resource) {
    using CleanType = std::remove_cvref_t<Resource>;
    return emplaceResource<CleanType>(std::forward<Resource>(resource));
}

template <typename Resource>
std::remove_cvref_t<Resource>& Registry::addResource(const Resource& resource) {
    using CleanType = std::remove_cvref_t<Resource>;
    return emplaceResource<CleanType>(resource);
}

template <typename Resource>
Resource& Registry::getResource() {
    const ResourceTypeId id = getResourceTypeId<Resource>();
    if (id >= _resources.size() || !_resources[id]) {
        throw std::runtime_error("Resource not registered in Registry");
    }
    return static_cast<detail::ResourceSlot<Resource>*>(_resources[id].get())->get();
}

template <typename Resource>
const Resource& Registry::getResource() const {
    const ResourceTypeId id = getResourceTypeId<Resource>();
    if (id >= _resources.size() || !_resources[id]) {
        throw std::runtime_error("Resource not registered in Registry");
    }
    return static_cast<const detail::ResourceSlot<Resource>*>(_resources[id].get())->get();
}

template <typename Resource>
bool Registry::hasResource() const noexcept {
    const ResourceTypeId id = getResourceTypeId<Resource>();
    return id < _resources.size() && _resources[id] != nullptr;
}

template <typename Resource>
void Registry::removeResource() {
    const ResourceTypeId id = getResourceTypeId<Resource>();
    if (id < _resources.size()) {
        _resources[id].reset();
    }
}

template <class... Components, class Func>
void Registry::runSystem(Func func) {
    if constexpr (sizeof...(Components) == 0) {
        return;
    } else if constexpr (sizeof...(Components) == 1) {
        auto& arr = getComponents<Components...>();
        auto& denseComponents = arr.getDense();
        const auto& denseEntities = arr.getDenseEntities();
        const std::size_t count = denseEntities.size();
        for (std::size_t i = 0; i < count; ++i) {
            func(denseEntities[i], denseComponents[i]);
        }
    } else {
        std::tuple<SparseArray<Components>&...> arrays{getComponents<Components>()...};
        const std::array<std::size_t, sizeof...(Components)> sizes{
            std::get<SparseArray<Components>&>(arrays).getDenseEntities().size()...};

        std::size_t minIdx = 0;
        for (std::size_t i = 1; i < sizes.size(); ++i) {
            if (sizes[i] < sizes[minIdx]) {
                minIdx = i;
            }
        }

        if (sizes[minIdx] == 0) {
            return;
        }

        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            (((minIdx == Is) ? (detail::runSystemWithDriver<Is, Components...>(arrays, func), true) : false) || ...);
        }(std::index_sequence_for<Components...>{});
    }
}

}  // namespace ecs
