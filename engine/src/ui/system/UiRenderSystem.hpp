#pragma once

#include <vector>

#include "ecs/Registry.hpp"
#include "ui/font/FontStore.hpp"
#include "ui/system/UiDrawItem.hpp"

namespace ui {

/**
 * @brief Draws every HUD entity that has a `UiAnchor` and a drawable UI component.
 *
 * Mirrors `render::RenderSystem`: `collect` resolves anchors and fills the draw
 * queue; `draw` issues the actual raylib calls. The queue is reused across frames
 * to avoid per-frame heap allocation.
 */
class UiRenderSystem {
  public:
    /**
     * @brief Creates the system.
     *
     * @param fonts Store used to resolve font identifiers; must outlive this system.
     */
    explicit UiRenderSystem(const FontStore& fonts);

    /**
     * @brief Gathers HUD entities and resolves their screen positions.
     *
     * @param registry Registry to read from.
     */
    void collect(ecs::Registry& registry);

    /**
     * @brief Draws the items gathered by the last `collect`.
     *
     * Must be called between `Window::beginFrame` and `Window::endFrame`.
     */
    void draw() const;

    /**
     * @brief Items gathered by the last `collect`.
     * @return The draw queue.
     */
    [[nodiscard]] const std::vector<UiDrawItem>& drawQueue() const noexcept { return _queue; }

  private:
    /**
     * @brief Draws a single `UiRect` at the given screen position.
     * @param x Resolved screen X in pixels.
     * @param y Resolved screen Y in pixels.
     * @param rect Component to draw.
     */
    static void drawRect(float x, float y, const UiRect& rect);

    /**
     * @brief Draws a single `UiText` at the given screen position.
     * @param x Resolved screen X in pixels.
     * @param y Resolved screen Y in pixels.
     * @param text Component to draw.
     */
    void drawText(float x, float y, const UiText& text) const;

    /**
     * @brief Draws a single `UiProgressBar` at the given screen position.
     * @param x Resolved screen X in pixels.
     * @param y Resolved screen Y in pixels.
     * @param bar Component to draw.
     */
    static void drawProgressBar(float x, float y, const UiProgressBar& bar);

    /** @brief Resolves font identifiers. */
    const FontStore* _fonts;

    /** @brief Items gathered for the current frame; capacity is kept between frames. */
    std::vector<UiDrawItem> _queue;
};

}  // namespace ui
