#pragma once

#include "systems/render/RenderSystem.hpp"

namespace render::detail {

/** @brief Orders draw items by visual depth, texture grouping, then entity identity. */
inline bool drawsBefore(const DrawItem& lhs, const DrawItem& rhs) {
    if (lhs.layer != rhs.layer) {
        return lhs.layer < rhs.layer;
    }
    if (lhs.transform.z != rhs.transform.z) {
        return lhs.transform.z < rhs.transform.z;
    }
    if (lhs.textureId != rhs.textureId) {
        return lhs.textureId < rhs.textureId;
    }
    return lhs.entity < rhs.entity;
}

}  // namespace render::detail