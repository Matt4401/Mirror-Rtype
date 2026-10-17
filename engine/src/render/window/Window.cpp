#include "render/window/Window.hpp"

#include <raylib.h>

#include <stdexcept>

#include "render/window/WindowConfig.hpp"

namespace render {

Window::Window(const WindowConfig& config) {
    if (config.width <= 0 || config.height <= 0) {
        throw std::invalid_argument("Window dimensions must be positive");
    }

    unsigned int flags = 0;
    if (config.vsync) {
        flags |= FLAG_VSYNC_HINT;
    }
    if (config.fullscreen) {
        flags |= FLAG_FULLSCREEN_MODE;
    }
    SetConfigFlags(flags);

    InitWindow(config.width, config.height, config.title.c_str());
    if (!IsWindowReady()) {
        throw std::runtime_error("Failed to create the game window");
    }
}

Window::~Window() {
    if (IsWindowReady()) {
        CloseWindow();
    }
}

// The window is a process-wide raylib resource; the instance only proves it is open.
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
bool Window::shouldClose() const { return WindowShouldClose(); }

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void Window::beginFrame() const {
    BeginDrawing();
    ClearBackground(BLACK);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void Window::endFrame() const { EndDrawing(); }

}  // namespace render
