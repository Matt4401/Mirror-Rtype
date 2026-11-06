/**
 * @file SpatialQueries.hpp
 * @brief Tag and distance queries over the Registry (`Transform` + `Tag`), independent of any physics backend.
 */

#pragma once

#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"

namespace ecs {

/**
 * @brief Largest radius accepted by `entitiesInRadius`, in world units; bigger values are clamped to it.
 */
inline constexpr float MAX_QUERY_RADIUS = 100000.0F;

/**
 * @brief Collects every entity whose `Tag` equals `tag`.
 *
 * @param registry Registry to search; if `Tag` was never registered the result is empty.
 * @param tag Exact, case-sensitive label to match.
 * @param out Cleared, then filled with the matching entities (its capacity is reused across calls).
 */
void entitiesWithTag(Registry const& registry, std::string_view tag, std::vector<Entity>& out);

/**
 * @brief Finds the entity tagged `tag` closest to a point.
 *
 * Only entities owning both a `Tag` and a `Transform` are considered. Ties keep the first entity found.
 *
 * @param registry Registry to search.
 * @param x Point X, in world units (same space as `Transform`).
 * @param y Point Y, in world units.
 * @param tag Exact, case-sensitive label to match.
 * @param excluded Entities to ignore (e.g. queued for destruction); expected to be small.
 * @return The closest matching entity, or `std::nullopt` if none matches or the point is not finite.
 */
[[nodiscard]] std::optional<Entity> nearestWithTag(Registry const& registry, float x, float y, std::string_view tag,
                                                   std::span<Entity const> excluded = {});

/**
 * @brief Collects every entity whose `Transform` lies within `radius` of a point (boundary included).
 *
 * @param registry Registry to search; if `Transform` was never registered the result is empty.
 * @param x Point X, in world units.
 * @param y Point Y, in world units.
 * @param radius Search radius, clamped to `MAX_QUERY_RADIUS`; negative or non-finite values give an empty result.
 * @param out Cleared, then filled with the matching entities (its capacity is reused across calls).
 */
void entitiesInRadius(Registry const& registry, float x, float y, float radius, std::vector<Entity>& out);

}  // namespace ecs
