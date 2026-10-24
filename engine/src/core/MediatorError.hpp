/**
 * @file MediatorError.hpp
 * @brief Error definitions for the engine Mediator service registry.
 */

#pragma once

#include <cstdint>
#include <string_view>

namespace engine {

/**
 * @brief Error codes returned by the Mediator when resolving services.
 */
enum class MediatorError : std::uint8_t {
    ServiceNotFound,    ///< The requested service interface has not been registered.
    AlreadyRegistered,  ///< A service for this interface type has already been registered.
    TypeMismatch        ///< The retrieved service does not match the requested type.
};

/**
 * @brief Converts a MediatorError enum into a descriptive string.
 * @param err The error code.
 * @return Human-readable description of the error.
 */
[[nodiscard]] constexpr std::string_view toString(MediatorError err) noexcept {
    switch (err) {
        case MediatorError::ServiceNotFound:
            return "Service not registered in Mediator";
        case MediatorError::AlreadyRegistered:
            return "Service already registered in Mediator";
        case MediatorError::TypeMismatch:
            return "Service type mismatch";
    }
    return "Unknown Mediator error";
}

}  // namespace engine
