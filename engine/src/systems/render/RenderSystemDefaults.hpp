#pragma once

#include <raylib.h>

#include <cstddef>
#include <numbers>

namespace render::detail {

inline constexpr std::size_t INITIAL_QUEUE_CAPACITY = 1024;
inline constexpr float PLACEHOLDER_SIZE = 32.0F;
inline constexpr float RADIANS_TO_DEGREES = 180.0F / std::numbers::pi_v<float>;

inline Camera2D neutralCamera() {
    return Camera2D{.offset = {.x = 0.0F, .y = 0.0F}, .target = {.x = 0.0F, .y = 0.0F}, .rotation = 0.0F, .zoom = 1.0F};
}

}  // namespace render::detail
