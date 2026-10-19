#pragma once

#include "render/window/WindowConfig.hpp"

namespace render {

/**
 * @brief RAII owner of the raylib window and its OpenGL context.
 *
 * The window is closed when the object is destroyed, including when an exception unwinds the stack.
 * Anything holding GPU resources (such as `TextureStore`) must be destroyed before the window.
 */
class Window {
  public:
    /**
     * @brief Opens the window.
     *
     * @param config Dimensions, title, VSync and fullscreen settings.
     * @throws std::invalid_argument If the dimensions are not positive.
     * @throws std::runtime_error If the window could not be created.
     */
    explicit Window(const WindowConfig& config);

    /** @brief Closes the window. */
    ~Window();

    /** @brief Deleted copy constructor: the window is a unique resource. */
    Window(const Window&) = delete;
    /** @brief Deleted copy assignment operator. */
    Window& operator=(const Window&) = delete;
    /** @brief Deleted move constructor. */
    Window(Window&&) = delete;
    /** @brief Deleted move assignment operator. */
    Window& operator=(Window&&) = delete;

    /**
     * @brief Tells whether the user asked to close the window.
     * @return True once the close button or the escape key was pressed.
     */
    [[nodiscard]] static bool shouldClose();

    /** @brief Starts a frame and clears the screen to black. */
    static void beginFrame();

    /** @brief Ends the frame and presents it on screen. */
    static void endFrame();
};

}  // namespace render
