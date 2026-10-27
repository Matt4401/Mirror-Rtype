/**
 * @file Mediator.tpp
 * @brief Template implementations for the engine Mediator service registry.
 */

#pragma once

#include <expected>
#include <functional>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <utility>

#include "core/Mediator.hpp"  // NOLINT(misc-header-include-cycle)
#include "core/MediatorError.hpp"

namespace engine {

template <typename Service>
std::expected<void, MediatorError> Mediator::registerService(const std::shared_ptr<Service>& service) {
    if (!service) {
        throw std::invalid_argument("Cannot register a null service in Mediator");
    }
    if (hasService<Service>()) {
        return std::unexpected(MediatorError::AlreadyRegistered);
    }
    const std::type_index key(typeid(Service));
    _services[key] = std::static_pointer_cast<void>(service);
    return {};
}

template <typename Service, typename Impl, typename... Args>
std::expected<std::reference_wrapper<Service>, MediatorError> Mediator::emplaceService(Args&&... args) {
    if (hasService<Service>()) {
        return std::unexpected(MediatorError::AlreadyRegistered);
    }
    auto instance = std::make_shared<Impl>(std::forward<Args>(args)...);
    const std::shared_ptr<Service> servicePtr = instance;
    const std::type_index key(typeid(Service));
    _services[key] = std::static_pointer_cast<void>(servicePtr);
    return std::ref(*instance);
}

template <typename Service>
std::expected<std::reference_wrapper<Service>, MediatorError> Mediator::getService() const noexcept {
    const std::type_index key(typeid(Service));
    auto it = _services.find(key);
    if (it == _services.end()) {
        return std::unexpected(MediatorError::ServiceNotFound);
    }
    auto typedPtr = std::static_pointer_cast<Service>(it->second);
    if (!typedPtr) {
        return std::unexpected(MediatorError::TypeMismatch);
    }
    return std::ref(*typedPtr);
}

template <typename Service>
Service& Mediator::service() const {
    auto result = getService<Service>();
    if (!result) {
        throw MediatorError(result.error());
    }
    return result->get();
}

template <typename Service>
bool Mediator::hasService() const noexcept {
    const std::type_index key(typeid(Service));
    return _services.contains(key);
}

template <typename Service>
void Mediator::removeService() {
    const std::type_index key(typeid(Service));
    _services.erase(key);
}

}  // namespace engine
