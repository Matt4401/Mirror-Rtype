/**
 * @file Hierarchy.hpp
 * @brief Functions creating, removing and querying parent/child links between entities.
 *
 * A link is stored on both sides: the child gets a `Parent` and a `LocalTransform`, the parent lists the child in its
 * `Children`. Always change links through these functions so both sides stay consistent; `HierarchyHooks` (installed
 * by `HierarchyPlugin`) keeps them consistent when entities are destroyed.
 */

#pragma once

#include <optional>
#include <span>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"

namespace hierarchy {

/**
 * @brief Makes `child` a child of `parent`, keeping the child's current world position.
 *
 * The child is detached from its previous parent first, then appended at the end of the parent's `Children`. Its
 * `LocalTransform` is computed from both current world transforms (a missing `ecs::Transform` counts as identity),
 * so attaching never makes it jump. Set the `LocalTransform` afterwards to place it at a given offset instead.
 *
 * @note A parent axis with a collapsed (zero) scale cannot be inverted: the link is still made, but the child is
 *       flattened onto the parent along that axis on the next propagation, and keeps that offset once the parent
 *       scales back up.
 *
 * @param registry Registry owning both entities.
 * @param child Entity to attach.
 * @param parent New parent.
 * @return False, without changing anything, if either entity is not alive, if `child == parent`, or if `child` is an
 *         ancestor of `parent` (the link would create a cycle). True otherwise, including when already linked.
 */
bool setParent(ecs::Registry& registry, ecs::Entity child, ecs::Entity parent);

/**
 * @brief Detaches `child` from its parent; it becomes a root.
 *
 * Removes `Parent` and `LocalTransform` from the child and the child from the parent's `Children` (the component is
 * removed once empty). The child's world `ecs::Transform` keeps its last value. Does nothing if `child` has no parent.
 *
 * @param registry Registry owning the entity.
 * @param child Entity to detach.
 */
void removeParent(ecs::Registry& registry, ecs::Entity child);

/**
 * @brief Gets the parent of an entity.
 * @param registry Registry owning the entity.
 * @param e Entity to query.
 * @return Parent handle, or `std::nullopt` if `e` has no parent.
 */
[[nodiscard]] std::optional<ecs::Entity> parentOf(const ecs::Registry& registry, ecs::Entity e);

/**
 * @brief Gets the direct children of an entity, in insertion order.
 *
 * The span is invalidated by any change to the entity's children or to the `Children` storage.
 *
 * @param registry Registry owning the entity.
 * @param e Entity to query.
 * @return Children of `e`, empty if it has none.
 */
[[nodiscard]] std::span<const ecs::Entity> childrenOf(const ecs::Registry& registry, ecs::Entity e);

/**
 * @brief Checks whether `ancestor` is a (direct or indirect) parent of `e`.
 * @param registry Registry owning the entities.
 * @param ancestor Candidate ancestor.
 * @param e Entity whose parent chain is walked.
 * @return True if `ancestor` appears in the parent chain of `e`. An entity is not its own ancestor.
 */
[[nodiscard]] bool isAncestorOf(const ecs::Registry& registry, ecs::Entity ancestor, ecs::Entity e);

/**
 * @brief Gets the root of the tree an entity belongs to.
 * @param registry Registry owning the entity.
 * @param e Entity to query.
 * @return Topmost ancestor of `e`, or `e` itself if it has no parent.
 */
[[nodiscard]] ecs::Entity rootOf(const ecs::Registry& registry, ecs::Entity e);

}  // namespace hierarchy
