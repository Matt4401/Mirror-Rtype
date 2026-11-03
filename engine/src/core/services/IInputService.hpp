/**
 * @file IInputService.hpp
 * @brief Abstract service interface for action mappings and input configuration.
 */

#pragma once

namespace input {
class ActionMapping;
}

namespace engine {

/**
 * @brief Abstract input service exposed through the Mediator.
 */
class IInputService {
  public:
    /** @brief Default constructor. */
    IInputService() = default;

    /** @brief Virtual destructor. */
    virtual ~IInputService() = default;

    /** @brief Default copy constructor. */
    IInputService(const IInputService&) = default;

    /** @brief Default copy assignment operator. */
    IInputService& operator=(const IInputService&) = default;

    /** @brief Default move constructor. */
    IInputService(IInputService&&) noexcept = default;

    /** @brief Default move assignment operator. */
    IInputService& operator=(IInputService&&) noexcept = default;

    /**
     * @brief Gets mutable reference to the active action mapping configuration.
     * @return Reference to ActionMapping.
     */
    [[nodiscard]] virtual input::ActionMapping& mapping() noexcept = 0;

    /**
     * @brief Gets const reference to the active action mapping configuration.
     * @return Const reference to ActionMapping.
     */
    [[nodiscard]] virtual const input::ActionMapping& mapping() const noexcept = 0;

    /**
     * @brief Gets the logical player index handled by this input service.
     * @return Player index.
     */
    [[nodiscard]] virtual int playerIndex() const noexcept = 0;

    /**
     * @brief Gets the hardware gamepad device index.
     * @return Gamepad index.
     */
    [[nodiscard]] virtual int gamepadIndex() const noexcept = 0;
};

}  // namespace engine
