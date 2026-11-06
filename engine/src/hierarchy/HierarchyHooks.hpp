/**
 * @file HierarchyHooks.hpp
 * @brief Registry listeners keeping parent/child links consistent when entities or links are removed.
 */

#pragma once

#include <cstddef>

#include "ecs/ComponentType.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"

namespace hierarchy {

/**
 * @brief Installs, for its lifetime, the registry listeners that maintain the hierarchy.
 *
 * - **Destroying an entity destroys all its descendants** (deepest first, without recursion proportional to the tree
 *   depth), and removes the entity from its parent's `Children`.
 * - **Removing `Parent` by hand** removes the child from its old parent's `Children`.
 * - **Removing `Children` by hand** detaches every child that pointed at that parent.
 *
 * The manual-removal fix-ups scan the hierarchy components (linear cost); prefer `removeParent`. `HierarchyPlugin`
 * owns one instance; tests can create one directly on a registry. The registry must outlive this object.
 */
class HierarchyHooks {
  public:
    /**
     * @brief Registers the listeners on a registry.
     * @param registry Registry to maintain; must outlive this object.
     */
    explicit HierarchyHooks(ecs::Registry& registry);

    /** @brief Unregisters the listeners. */
    ~HierarchyHooks();

    /** @brief Deleted copy constructor (listeners capture `this`). */
    HierarchyHooks(const HierarchyHooks&) = delete;
    /** @brief Deleted copy assignment operator. */
    HierarchyHooks& operator=(const HierarchyHooks&) = delete;
    /** @brief Deleted move constructor (listeners capture `this`). */
    HierarchyHooks(HierarchyHooks&&) = delete;
    /** @brief Deleted move assignment operator. */
    HierarchyHooks& operator=(HierarchyHooks&&) = delete;

  private:
    /**
     * @brief Destroy listener: cascades to descendants and unlinks the entity from its parent.
     * @param e Entity being destroyed (already dead, components still present).
     */
    void onDestroy(ecs::Entity e);

    /**
     * @brief Component-remove listener: repairs links after a `Parent` or `Children` was removed by hand.
     * @param e Entity that lost a component.
     * @param type Type of the removed component.
     */
    void onComponentRemoved(ecs::Entity e, ecs::ComponentTypeId type);

    /** @brief Registry being maintained. */
    ecs::Registry* _registry;
    /** @brief Id of the destroy listener. */
    std::size_t _destroyListenerId;
    /** @brief Id of the component-remove listener. */
    std::size_t _removeListenerId;
};

}  // namespace hierarchy
