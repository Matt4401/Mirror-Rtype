/**
 * @file HierarchyComponents.hpp
 * @brief Components linking entities into parent/child trees.
 *
 * Links must only be changed through the functions of `hierarchy/Hierarchy.hpp` (`setParent`, `removeParent`),
 * never by adding `Parent` or `Children` by hand: both sides of a link are kept consistent by those functions and by
 * `HierarchyHooks`.
 */

#pragma once

#include <vector>

#include "ecs/Entity.hpp"

namespace hierarchy {

/**
 * @brief Parent of an entity in a hierarchy.
 *
 * Its world `ecs::Transform` is driven by its `LocalTransform` composed with the parent's world transform.
 */
struct Parent {
    /** @brief Parent entity handle. */
    ecs::Entity entity = ecs::NULL_ENTITY;
};

/**
 * @brief Ordered list of the direct children of an entity.
 *
 * Kept in insertion order: systems that care about sibling order (UI drawing and layout) can rely on it.
 */
struct Children {
    /** @brief Direct children, in insertion order. */
    std::vector<ecs::Entity> entities;
};

/**
 * @brief Transform of an entity relative to its parent.
 *
 * Same fields and defaults as `ecs::Transform`. The world `ecs::Transform` of a child is written from it by
 * `propagateTransforms`, so a child is moved by editing its `LocalTransform`, not its `Transform`.
 */
struct LocalTransform {
    /** @brief Horizontal offset in the parent's space. */
    float x = 0.0F;

    /** @brief Vertical offset in the parent's space. */
    float y = 0.0F;

    /** @brief Depth offset added to the parent's depth (positive draws in front of the parent). */
    float z = 0.0F;

    /** @brief Rotation relative to the parent, in radians. */
    float rotation = 0.0F;

    /** @brief Horizontal scale relative to the parent. */
    float scaleX = 1.0F;

    /** @brief Vertical scale relative to the parent. */
    float scaleY = 1.0F;
};

}  // namespace hierarchy
