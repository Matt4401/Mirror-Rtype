#include "hierarchy/HierarchyHooks.hpp"

#include <cstddef>
#include <ranges>
#include <vector>

#include "ecs/ComponentType.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "hierarchy/HierarchyComponents.hpp"

namespace hierarchy {

namespace {

/**
 * @brief Collects every descendant of an entity, parents before their children.
 * @param registry Registry owning the tree.
 * @param root Entity whose descendants are collected (not included).
 * @return Descendants in breadth-first order.
 */
std::vector<ecs::Entity> collectDescendants(const ecs::Registry& registry, ecs::Entity root) {
    std::vector<ecs::Entity> descendants;
    if (!registry.hasComponent<Children>(root)) {
        return descendants;
    }
    const auto& childrenArray = registry.getComponents<Children>();
    const auto& first = childrenArray.get(root).entities;
    descendants.assign(first.begin(), first.end());
    for (std::size_t i = 0; i < descendants.size(); ++i) {
        if (childrenArray.contains(descendants[i])) {
            const auto& next = childrenArray.get(descendants[i]).entities;
            descendants.insert(descendants.end(), next.begin(), next.end());
        }
    }
    return descendants;
}

}  // namespace

HierarchyHooks::HierarchyHooks(ecs::Registry& registry)
    : _registry(&registry),
      _destroyListenerId(registry.addDestroyListener([this](ecs::Entity e) { onDestroy(e); })),
      _removeListenerId(registry.addComponentRemoveListener(
          [this](ecs::Entity e, ecs::ComponentTypeId type) { onComponentRemoved(e, type); })) {}

HierarchyHooks::~HierarchyHooks() {
    _registry->removeDestroyListener(_destroyListenerId);
    _registry->removeComponentRemoveListener(_removeListenerId);
}

void HierarchyHooks::onDestroy(ecs::Entity e) {
    if (_registry->hasComponent<Parent>(e)) {
        const ecs::Entity parent = _registry->getComponents<Parent>().get(e).entity;
        if (_registry->isAlive(parent) && _registry->hasComponent<Children>(parent)) {
            std::erase(_registry->getComponents<Children>().get(parent).entities, e);
        }
    }

    const std::vector<ecs::Entity> descendants = collectDescendants(*_registry, e);
    for (const ecs::Entity descendant : std::views::reverse(descendants)) {
        _registry->killEntity(descendant);
    }
}

void HierarchyHooks::onComponentRemoved(ecs::Entity e, ecs::ComponentTypeId type) {
    if (type == ecs::getComponentTypeId<Parent>()) {
        for (Children& children : _registry->getComponents<Children>().getDense()) {
            std::erase(children.entities, e);
        }
        return;
    }
    if (type == ecs::getComponentTypeId<Children>()) {
        std::vector<ecs::Entity> orphans;
        auto& parents = _registry->getComponents<Parent>();
        for (std::size_t i = 0; i < parents.getDense().size(); ++i) {
            if (parents.getDense()[i].entity == e) {
                orphans.push_back(parents.getDenseEntities()[i]);
            }
        }
        for (const ecs::Entity orphan : orphans) {
            _registry->removeComponent<Parent>(orphan);
            _registry->removeComponent<LocalTransform>(orphan);
        }
    }
}

}  // namespace hierarchy
