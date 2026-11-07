/**
 * @file TransformMath.hpp
 * @brief Conversions between world transforms and parent-relative transforms.
 */

#pragma once

#include "components/Transform.hpp"
#include "hierarchy/HierarchyComponents.hpp"

namespace hierarchy {

/**
 * @brief Computes the world transform of a child from its parent's world transform.
 *
 * The local position is scaled by the parent's scale, rotated by the parent's rotation, then offset by the parent's
 * position. Rotations and depths add up, scales multiply.
 *
 * @note The shear that a non-uniform parent scale combined with a child rotation would produce is not represented:
 *       a 2D `Transform` has no shear term. The child's own position is exact, but its shape and its descendants'
 *       positions can differ from full affine composition.
 *
 * @param parentWorld World transform of the parent.
 * @param local Transform of the child relative to the parent.
 * @return World transform of the child.
 */
[[nodiscard]] ecs::Transform compose(const ecs::Transform& parentWorld, const LocalTransform& local) noexcept;

/**
 * @brief Computes the parent-relative transform giving a child a wanted world transform (inverse of `compose`).
 *
 * If a parent scale component is (almost) zero, the matching local position axis becomes 0 and the matching local
 * scale 1, so the result never contains infinities or NaN.
 *
 * @param parentWorld World transform of the parent.
 * @param world Wanted world transform of the child.
 * @return Local transform such that `compose(parentWorld, result)` gives back `world`.
 */
[[nodiscard]] LocalTransform toLocal(const ecs::Transform& parentWorld, const ecs::Transform& world) noexcept;

}  // namespace hierarchy
