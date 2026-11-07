#pragma once

#include "core/services/IScreenDrawService.hpp"

namespace ui {

/** @brief Handle to a font loaded with `engine::IScreenDrawService::loadFont`. */
using engine::FontId;

/** @brief Sentinel value meaning "no font / use the default font". */
using engine::NO_FONT;

}  // namespace ui
