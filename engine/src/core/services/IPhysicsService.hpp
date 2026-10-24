/**
 * @file IPhysicsService.hpp
 * @brief Abstract service interface for physics simulation and body management.
 */

#pragma once

#include <cstddef>

namespace engine {

/**
 * @brief Abstract physics service exposed through the Mediator.
 */
class IPhysicsService {
  public:
    /** @brief Default constructor. */
    IPhysicsService() = default;

    /** @brief Virtual destructor. */
    virtual ~IPhysicsService() = default;

    /** @brief Default copy constructor. */
    IPhysicsService(const IPhysicsService&) = default;

    /** @brief Default copy assignment operator. */
    IPhysicsService& operator=(const IPhysicsService&) = default;

    /** @brief Default move constructor. */
    IPhysicsService(IPhysicsService&&) noexcept = default;

    /** @brief Default move assignment operator. */
    IPhysicsService& operator=(IPhysicsService&&) noexcept = default;

    /**
     * @brief Steps the physics simulation forward by a fixed delta time.
     * @param fixedDt Duration of the simulation step in seconds.
     */
    virtual void step(float fixedDt) = 0;

    /**
     * @brief Returns the count of active physics bodies in the simulation.
     * @return Number of active bodies.
     */
    [[nodiscard]] virtual std::size_t bodyCount() const noexcept = 0;
};

}  // namespace engine
