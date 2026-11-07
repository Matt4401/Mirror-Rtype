/**
 * @file TransformPropagation.hpp
 * @brief Writes the world transform of every child from its parent's world transform.
 */

#pragma once

#include "ecs/Registry.hpp"

namespace hierarchy {

/**
 * @brief Updates the world `ecs::Transform` of every entity that has a parent.
 *
 * Every tree is walked from its root (an entity with `Children` and no living parent), parents before children, so a
 * single pass is enough whatever the depth. For each child with a `LocalTransform`, the world transform becomes
 * `compose(parentWorld, local)`; the `ecs::Transform` is created if missing. A child without `LocalTransform` keeps
 * its world transform, and its own children are placed from it. A parent without `ecs::Transform` counts as identity.
 *
 * Uses an explicit stack, so deep trees cannot overflow the call stack. `HierarchyPlugin` runs it in `lateUpdate`.
 *
 * @param registry Registry holding the hierarchy.
 */
void propagateTransforms(ecs::Registry& registry);

}  // namespace hierarchy
