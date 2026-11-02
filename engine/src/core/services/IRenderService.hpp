/**
 * @file IRenderService.hpp
 * @brief Abstract service interface for 2D entity rendering and camera views.
 */

#pragma once

namespace ecs {
class Registry;
}

namespace engine {

/**
 * @brief Abstract rendering service exposed through the Mediator.
 */
class IRenderService {
  public:
    /** @brief Default constructor. */
    IRenderService() = default;

    /** @brief Virtual destructor. */
    virtual ~IRenderService() = default;

    /** @brief Default copy constructor. */
    IRenderService(const IRenderService&) = default;

    /** @brief Default copy assignment operator. */
    IRenderService& operator=(const IRenderService&) = default;

    /** @brief Default move constructor. */
    IRenderService(IRenderService&&) noexcept = default;

    /** @brief Default move assignment operator. */
    IRenderService& operator=(IRenderService&&) noexcept = default;

    /**
     * @brief Collects renderable entities from the ECS registry without drawing.
     * @param registry Target registry holding transforms and renderables.
     */
    virtual void collect(ecs::Registry& registry) = 0;

    /**
     * @brief Draws the collected entities through the camera.
     */
    virtual void draw() const = 0;

    /**
     * @brief Performs collection and drawing in a single pass.
     * @param registry Target registry.
     */
    virtual void update(ecs::Registry& registry) = 0;
};

}  // namespace engine
