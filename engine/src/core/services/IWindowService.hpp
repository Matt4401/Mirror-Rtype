/**
 * @file IWindowService.hpp
 * @brief Abstract service interface for window management and display lifecycle.
 */

#pragma once

namespace engine {

/**
 * @brief Abstract window service exposed through the Mediator.
 *
 * Allows systems and other plugins (e.g. Input) to query window state,
 * close conditions, and dimensions without directly depending on Raylib.
 */
class IWindowService {
  public:
    /** @brief Default constructor. */
    IWindowService() = default;

    /** @brief Virtual destructor for polymorphic cleanup. */
    virtual ~IWindowService() = default;

    /** @brief Default copy constructor. */
    IWindowService(const IWindowService&) = default;

    /** @brief Default copy assignment operator. */
    IWindowService& operator=(const IWindowService&) = default;

    /** @brief Default move constructor. */
    IWindowService(IWindowService&&) noexcept = default;

    /** @brief Default move assignment operator. */
    IWindowService& operator=(IWindowService&&) noexcept = default;

    /**
     * @brief Queries whether a window close event has been requested.
     * @return True if the window close button or escape key was pressed.
     */
    [[nodiscard]] virtual bool shouldClose() const noexcept = 0;

    /**
     * @brief Prepares the window and rendering context for a new frame.
     */
    virtual void beginFrame() = 0;

    /**
     * @brief Presents the drawn frame buffer to the display.
     */
    virtual void endFrame() = 0;

    /**
     * @brief Gets current window width in pixels.
     * @return Window width.
     */
    [[nodiscard]] virtual int width() const noexcept = 0;

    /**
     * @brief Gets current window height in pixels.
     * @return Window height.
     */
    [[nodiscard]] virtual int height() const noexcept = 0;

    /**
     * @brief Queries whether a keyboard key is held down.
     * @param key Backend key code.
     * @return True if the key is held down.
     */
    [[nodiscard]] virtual bool isKeyDown(int key) const noexcept = 0;

    /**
     * @brief Queries whether a gamepad is connected.
     * @param gamepad Hardware gamepad index.
     * @return True if the gamepad is available.
     */
    [[nodiscard]] virtual bool isGamepadAvailable(int gamepad) const noexcept = 0;

    /**
     * @brief Queries whether a gamepad button is held down.
     * @param gamepad Hardware gamepad index.
     * @param button Backend button code.
     * @return True if the button is held down.
     */
    [[nodiscard]] virtual bool isGamepadButtonDown(int gamepad, int button) const noexcept = 0;

    /**
     * @brief Samples a gamepad axis.
     * @param gamepad Hardware gamepad index.
     * @param axis Backend axis code.
     * @return Axis position.
     */
    [[nodiscard]] virtual float gamepadAxis(int gamepad, int axis) const noexcept = 0;
};

}  // namespace engine
