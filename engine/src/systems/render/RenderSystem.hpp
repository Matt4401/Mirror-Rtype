#pragma once

#include <raylib.h>

#include <vector>

#include "components/Renderable.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "render/texture/TextureStore.hpp"

namespace render {

/**
 * @brief One drawable entity captured for the current frame.
 */
struct DrawItem {
    /** @brief Entity being drawn, used as the last sort tie-breaker. */
    ecs::Entity entity = 0;

    /** @brief Draw layer copied from `ecs::Renderable`. */
    int layer = 0;

    /** @brief Texture to draw, or `ecs::NO_TEXTURE` for a flat colored placeholder. */
    ecs::TextureId textureId = ecs::NO_TEXTURE;

    /** @brief Texture region copied from `ecs::Renderable`. */
    ecs::TextureRect sourceRect;

    /** @brief Spatial state copied from `ecs::Transform`. */
    ecs::Transform transform;

    /** @brief Tint copied from `ecs::Renderable`. */
    ecs::Rgba tint;
};

/**
 * @brief Draws every entity that has both `ecs::Transform` and `ecs::Renderable`.
 *
 * Entities are sorted back to front by layer, then by `Transform::z`, then by texture identifier, and finally
 * by entity id. The draw queue is reused between frames: once it has grown to the number of visible entities,
 * no heap allocation happens per frame. Client-side only.
 */
class RenderSystem {
  public:
    /** @brief Size in pixels of the square drawn for entities without a texture, at scale 1. */
    static constexpr float PLACEHOLDER_SIZE = 32.0F;

    /**
     * @brief Creates the system.
     *
     * @param textures Store used to resolve texture identifiers; must outlive this system.
     */
    explicit RenderSystem(const TextureStore& textures);

    /**
     * @brief Gathers the drawable entities and the active camera, without drawing anything.
     *
     * @param registry Registry to read from.
     */
    void collect(ecs::Registry& registry);

    /**
     * @brief Draws the entities gathered by the last `collect`, through the camera.
     *
     * Must be called between `Window::beginFrame` and `Window::endFrame`.
     */
    void draw() const;

    /**
     * @brief Collects then draws in one call.
     *
     * @param registry Registry to read from.
     */
    void update(ecs::Registry& registry);

    /**
     * @brief Entities gathered by the last `collect`, sorted back to front.
     * @return The draw queue.
     */
    [[nodiscard]] const std::vector<DrawItem>& drawQueue() const noexcept { return _queue; }

    /**
     * @brief Camera selected by the last `collect`.
     * @return The first active `CameraComponent`'s camera, or a neutral camera if there is none.
     */
    [[nodiscard]] const Camera2D& camera() const noexcept { return _camera; }

  private:
    /**
     * @brief Draws a single item.
     * @param item Item to draw.
     */
    void drawItem(const DrawItem& item) const;

    /** @brief Resolves texture identifiers. */
    const TextureStore* _textures;

    /** @brief Items gathered for the current frame; its capacity is kept between frames. */
    std::vector<DrawItem> _queue;

    /** @brief Camera used by `draw`. */
    Camera2D _camera;
};

}  // namespace render
