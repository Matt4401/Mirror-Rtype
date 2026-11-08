/**
 * @file IInputCaptureService.hpp
 * @brief Abstract service letting one owner take the user input away from the rest of the program.
 */

#pragma once

#include "core/services/InputCodes.hpp"

namespace engine {

/**
 * @brief Takes the user input away from the rest of the program (a developer console, a modal text prompt...).
 *
 * While the input is captured, `IWindowService` reports no key, mouse button, mouse wheel, gamepad input or typed
 * character: gameplay, menus and every other reader see an idle device, without knowing about the capture. The owner
 * of the capture reads the real state through this service.
 *
 * When the capture is released, keys still held stay hidden from `IWindowService` until they are released, so the key
 * that closed the capturing owner (Escape...) does not also reach the game.
 */
class IInputCaptureService {
  public:
    /** @brief Default constructor. */
    IInputCaptureService() = default;
    /** @brief Virtual destructor. */
    virtual ~IInputCaptureService() = default;
    /** @brief Default copy constructor. */
    IInputCaptureService(const IInputCaptureService&) = default;
    /** @brief Default copy assignment operator. */
    IInputCaptureService& operator=(const IInputCaptureService&) = default;
    /** @brief Default move constructor. */
    IInputCaptureService(IInputCaptureService&&) noexcept = default;
    /** @brief Default move assignment operator. */
    IInputCaptureService& operator=(IInputCaptureService&&) noexcept = default;

    /**
     * @brief Captures or releases the input.
     * @param captured `true` to hide the input from `IWindowService`.
     */
    virtual void setCaptured(bool captured) = 0;

    /**
     * @brief Tells whether the input is captured.
     * @return `true` while the input is hidden from `IWindowService`.
     */
    [[nodiscard]] virtual bool isCaptured() const noexcept = 0;

    /**
     * @brief Reads a key, captured or not.
     * @param key Engine key.
     * @return `true` if the key is physically held.
     */
    [[nodiscard]] virtual bool isKeyDown(Key key) const noexcept = 0;

    /**
     * @brief Pops the next character typed this frame, captured or not.
     * @return Unicode code point, or `U'\0'` when none is left.
     */
    [[nodiscard]] virtual char32_t nextTypedCharacter() noexcept = 0;

    /**
     * @brief Reads the mouse wheel, captured or not.
     * @return Wheel movement this frame (positive away from the user).
     */
    [[nodiscard]] virtual float mouseWheelDelta() const noexcept = 0;
};

}  // namespace engine
