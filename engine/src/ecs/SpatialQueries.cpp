/**
 * @file SpatialQueries.cpp
 * @brief Linear-scan implementation of the Registry spatial queries.
 */

#include "ecs/SpatialQueries.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "components/Tag.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"

namespace ecs {

namespace {

/** @brief Squared distance between a transform and a point. */
[[nodiscard]] float squaredDistance(Transform const& t, float x, float y) noexcept {
    float const dx = t.x - x;
    float const dy = t.y - y;
    return (dx * dx) + (dy * dy);
}

}  // namespace

void entitiesWithTag(Registry const& registry, std::string_view tag, std::vector<Entity>& out) {
    out.clear();
    auto const* tags = registry.findComponents<Tag>();
    if (tags == nullptr) {
        return;
    }
    auto const& entities = tags->getDenseEntities();
    auto const& values = tags->getDense();
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (values[i].name == tag) {
            out.emplace_back(entities[i]);
        }
    }
}

std::optional<Entity> nearestWithTag(Registry const& registry, float x, float y, std::string_view tag,
                                     std::span<Entity const> excluded) {
    auto const* tags = registry.findComponents<Tag>();
    auto const* transforms = registry.findComponents<Transform>();
    if (tags == nullptr || transforms == nullptr || !std::isfinite(x) || !std::isfinite(y)) {
        return std::nullopt;
    }
    std::optional<Entity> best;
    float bestDistance = 0.0F;
    auto const& entities = tags->getDenseEntities();
    auto const& values = tags->getDense();
    for (std::size_t i = 0; i < values.size(); ++i) {
        Entity const e = entities[i];
        if (values[i].name != tag || !transforms->contains(e) || std::ranges::find(excluded, e) != excluded.end()) {
            continue;
        }
        float const distance = squaredDistance(transforms->get(e), x, y);
        if (!best.has_value() || distance < bestDistance) {
            best = e;
            bestDistance = distance;
        }
    }
    return best;
}

void entitiesInRadius(Registry const& registry, float x, float y, float radius, std::vector<Entity>& out) {
    out.clear();
    auto const* transforms = registry.findComponents<Transform>();
    if (transforms == nullptr || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(radius) || radius < 0.0F) {
        return;
    }
    float const clamped = std::min(radius, MAX_QUERY_RADIUS);
    float const maxDistance = clamped * clamped;
    auto const& entities = transforms->getDenseEntities();
    auto const& values = transforms->getDense();
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (squaredDistance(values[i], x, y) <= maxDistance) {
            out.emplace_back(entities[i]);
        }
    }
}

}  // namespace ecs
