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
    [[nodiscard]] bool shouldClose() const;

    /** @brief Starts a frame and clears the screen to black. */
    void beginFrame() const;

    /** @brief Ends the frame and presents it on screen. */
    void endFrame() const;
};

}  // namespace render
