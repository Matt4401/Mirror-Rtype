#include "hierarchy/TransformMath.hpp"

#include <cmath>

#include "components/Transform.hpp"
#include "hierarchy/HierarchyComponents.hpp"

namespace hierarchy {

namespace {

/** @brief Scale magnitude under which a parent axis is treated as collapsed. */
constexpr float MIN_SCALE = 1e-6F;

/**
 * @brief Divides by a parent scale, treating a collapsed axis as 0.
 * @param value Value to divide.
 * @param scale Parent scale.
 * @param fallback Result used when the scale is collapsed.
 * @return `value / scale`, or `fallback` if `scale` is (almost) zero.
 */
float divideByScale(float value, float scale, float fallback) noexcept {
    return std::abs(scale) < MIN_SCALE ? fallback : value / scale;
}

}  // namespace

ecs::Transform compose(const ecs::Transform& parentWorld, const LocalTransform& local) noexcept {
    const float cosR = std::cos(parentWorld.rotation);
    const float sinR = std::sin(parentWorld.rotation);
    const float scaledX = local.x * parentWorld.scaleX;
    const float scaledY = local.y * parentWorld.scaleY;
    return ecs::Transform{
        .x = parentWorld.x + (cosR * scaledX) - (sinR * scaledY),
        .y = parentWorld.y + (sinR * scaledX) + (cosR * scaledY),
        .z = parentWorld.z + local.z,
        .rotation = parentWorld.rotation + local.rotation,
        .scaleX = parentWorld.scaleX * local.scaleX,
        .scaleY = parentWorld.scaleY * local.scaleY,
    };
}

LocalTransform toLocal(const ecs::Transform& parentWorld, const ecs::Transform& world) noexcept {
    const float cosR = std::cos(parentWorld.rotation);
    const float sinR = std::sin(parentWorld.rotation);
    const float dx = world.x - parentWorld.x;
    const float dy = world.y - parentWorld.y;
    // Undo the parent's rotation (rotate by -rotation), then its scale.
    const float rotatedX = (cosR * dx) + (sinR * dy);
    const float rotatedY = (-sinR * dx) + (cosR * dy);
    return LocalTransform{
        .x = divideByScale(rotatedX, parentWorld.scaleX, 0.0F),
        .y = divideByScale(rotatedY, parentWorld.scaleY, 0.0F),
        .z = world.z - parentWorld.z,
        .rotation = world.rotation - parentWorld.rotation,
        .scaleX = divideByScale(world.scaleX, parentWorld.scaleX, 1.0F),
        .scaleY = divideByScale(world.scaleY, parentWorld.scaleY, 1.0F),
    };
}

}  // namespace hierarchy
