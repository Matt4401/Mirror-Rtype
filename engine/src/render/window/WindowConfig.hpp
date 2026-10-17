#pragma once

#include <string>

namespace render {

/**
 * @brief Settings used to open the game window.
 */
struct WindowConfig {
    /** @brief Window width in pixels (must be positive). */
    int width = 1280;

    /** @brief Window height in pixels (must be positive). */
    int height = 720;

    /** @brief Title shown in the window decoration. */
    std::string title = "R-Type";

    /** @brief Synchronize frame presentation with the display refresh rate. */
    bool vsync = true;

    /** @brief Open the window in fullscreen mode. */
    bool fullscreen = false;
};

}  // namespace render