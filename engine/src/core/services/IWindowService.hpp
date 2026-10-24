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
};

}  // namespace engine
