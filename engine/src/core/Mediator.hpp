/**
 * @file Mediator.hpp
 * @brief Typed service registry enabling decoupled cross-plugin communication.
 */

#pragma once

#include <expected>
#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>

#include "core/MediatorError.hpp"

namespace engine {

/**
 * @brief Decoupled service locator/broker for plugin interoperability.
 *
 * Plugins publish abstract interfaces (e.g. `IWindowService`, `IRenderService`)
 * and consume services registered by other plugins without direct concrete coupling.
 */
class Mediator {
  public:
    /** @brief Constructs an empty Mediator registry. */
    Mediator() = default;

    /** @brief Destroys the Mediator and releases registered service instances. */
    ~Mediator() = default;

    /** @brief Deleted copy constructor. */
    Mediator(const Mediator&) = delete;

    /** @brief Deleted copy assignment operator. */
    Mediator& operator=(const Mediator&) = delete;

    /** @brief Defaulted move constructor. */
    Mediator(Mediator&&) noexcept = default;

    /** @brief Defaulted move assignment operator. */
    Mediator& operator=(Mediator&&) noexcept = default;

    /**
     * @brief Registers an existing service instance for a specific interface.
     * @tparam Service Interface or class type to register under.
     * @param service Shared pointer holding the service implementation.
     * @return Empty expected on success, or MediatorError::AlreadyRegistered if already present.
     */
    template <typename Service>
    std::expected<void, MediatorError> registerService(const std::shared_ptr<Service>& service);

    /**
     * @brief Constructs and registers a service implementation in-place.
     * @tparam Service Interface type under which the service is registered.
     * @tparam Impl Concrete implementation type inheriting from Service.
     * @tparam Args Constructor argument types for Impl.
     * @param args Arguments forwarded to Impl's constructor.
     * @return Reference wrapper to the newly constructed service, or MediatorError::AlreadyRegistered if already
     * present.
     */
    template <typename Service, typename Impl = Service, typename... Args>
    std::expected<std::reference_wrapper<Service>, MediatorError> emplaceService(Args&&... args);

    /**
     * @brief Attempts to retrieve a registered service interface.
     * @tparam Service Interface type to query.
     * @return Reference wrapper to the service on success, or MediatorError on failure.
     */
    template <typename Service>
    [[nodiscard]] std::expected<std::reference_wrapper<Service>, MediatorError> getService() const noexcept;

    /**
     * @brief Retrieves a registered service, throwing if absent.
     * @tparam Service Interface type to query.
     * @return Reference to the service.
     * @throws MediatorError If the requested service is not registered or type mismatch.
     */
    template <typename Service>
    [[nodiscard]] Service& service() const;

    /**
     * @brief Checks whether a service is registered for the specified interface.
     * @tparam Service Interface type to check.
     * @return True if a service is registered, false otherwise.
     */
    template <typename Service>
    [[nodiscard]] bool hasService() const noexcept;

    /**
     * @brief Unregisters a service for the specified interface.
     * @tparam Service Interface type to remove.
     */
    template <typename Service>
    void removeService();

  private:
    /** @brief Storage slot associating type index with type-erased service shared pointer. */
    std::unordered_map<std::type_index, std::shared_ptr<void>> _services;
};

}  // namespace engine

#include "core/Mediator.tpp"  // IWYU pragma: export
