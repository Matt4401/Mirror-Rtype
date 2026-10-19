#include "render/window/Window.hpp"

#include <raylib.h>

#include <stdexcept>

#include "render/window/WindowConfig.hpp"

namespace render {

Window::Window(const WindowConfig& config) {
    if (config.width <= 0 || config.height <= 0) {
        throw std::invalid_argument("Window dimensions must be positive");
    }
    if (config.targetFps <= 0) {
        throw std::invalid_argument("Target FPS must be positive");
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
    SetTargetFPS(config.targetFps);
}

Window::~Window() {
    if (IsWindowReady()) {
        CloseWindow();
    }
}

bool Window::shouldClose() { return WindowShouldClose(); }

void Window::beginFrame() {
    BeginDrawing();
    ClearBackground(BLACK);
}

void Window::endFrame() { EndDrawing(); }

}  // namespace render
