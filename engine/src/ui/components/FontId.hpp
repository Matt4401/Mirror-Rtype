#pragma once

#include <cstdint>

namespace ui {

/** @brief Opaque handle to a font loaded in `FontStore`. */
using FontId = std::uint32_t;

/** @brief Sentinel value meaning "no font / use the default font". */
inline constexpr FontId NO_FONT = 0;

}  // namespace ui
