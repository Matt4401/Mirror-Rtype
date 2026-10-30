/**
 * @file MediatorError.hpp
 * @brief Error definitions and exception class for the engine Mediator service registry.
 */

#pragma once

#include <cstdint>
#include <exception>
#include <ostream>
#include <string_view>

namespace engine {

/**
 * @brief Error and exception type representing Mediator service resolution and registration failures.
 */
class MediatorError : public std::exception {
  public:
    /**
     * @brief Discriminator for the specific mediator failure kind.
     */
    enum class Type : std::uint8_t {
        ServiceNotFound,    ///< The requested service interface has not been registered.
        AlreadyRegistered,  ///< A service for this interface type has already been registered.
        TypeMismatch        ///< The retrieved service does not match the requested type.
    };

    /** @brief Shorthand constant for Type::ServiceNotFound. */
    static constexpr Type ServiceNotFound = Type::ServiceNotFound;
    /** @brief Shorthand constant for Type::AlreadyRegistered. */
    static constexpr Type AlreadyRegistered = Type::AlreadyRegistered;
    /** @brief Shorthand constant for Type::TypeMismatch. */
    static constexpr Type TypeMismatch = Type::TypeMismatch;

    /**
     * @brief Constructs a MediatorError from an error type code.
     * @param type Error type code.
     */
    constexpr MediatorError(Type type) noexcept : _type(type) {}

    /**
     * @brief Gets the error type code.
     * @return Underlying error code.
     */
    [[nodiscard]] constexpr Type type() const noexcept { return _type; }

    /**
     * @brief Returns a descriptive error message.
     * @return Null-terminated explanatory string.
     */
    [[nodiscard]] const char* what() const noexcept override {
        switch (_type) {
            case Type::ServiceNotFound:
                return "Service not registered in Mediator";
            case Type::AlreadyRegistered:
                return "Service already registered in Mediator";
            case Type::TypeMismatch:
                return "Service type mismatch";
        }
        return "Unknown Mediator error";
    }

    /**
     * @brief Equality comparison with another MediatorError.
     * @param other Another error instance to compare.
     * @return True if both errors have the same type code.
     */
    [[nodiscard]] constexpr bool operator==(const MediatorError& other) const noexcept { return _type == other._type; }

    /**
     * @brief Equality comparison with a Type code.
     * @param code Type code to compare against.
     * @return True if this error matches the given code.
     */
    [[nodiscard]] constexpr bool operator==(Type code) const noexcept { return _type == code; }

    /**
     * @brief Equality comparison between Type code and MediatorError.
     * @param code Type code to compare.
     * @param err MediatorError instance to compare against.
     * @return True if they match.
     */
    friend constexpr bool operator==(Type code, const MediatorError& err) noexcept { return err._type == code; }

    /**
     * @brief Stream output operator for MediatorError.
     * @param os Output stream.
     * @param err Error to print.
     * @return Reference to the output stream.
     */
    friend std::ostream& operator<<(std::ostream& os, const MediatorError& err) { return os << err.what(); }

  private:
    /** @brief Underlying error discriminator. */
    Type _type;
};

/**
 * @brief Converts a MediatorError into a descriptive string_view.
 * @param err The error object.
 * @return Human-readable description of the error.
 */
[[nodiscard]] inline std::string_view toString(const MediatorError& err) noexcept { return err.what(); }

}  // namespace engine
