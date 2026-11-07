#pragma once

#include <vector>

#include "ecs/Registry.hpp"
#include "ui/components/UiButton.hpp"
#include "ui/components/UiProgressBar.hpp"
#include "ui/components/UiRect.hpp"
#include "ui/components/UiText.hpp"
#include "ui/font/FontStore.hpp"
#include "ui/system/UiDrawItem.hpp"
#include "ui/system/UiLayout.hpp"

namespace ui {

/**
 * @brief Draws every HUD entity that has a `UiAnchor` and a drawable UI component.
 *
 * Mirrors `render::RenderSystem`: `collect` resolves positions with `computeLayout` (so children of a HUD element
 * are placed inside it) and fills the draw queue in layout order, parents below their children; `draw` issues the
 * actual raylib calls. The queues are reused across frames to avoid per-frame heap allocation.
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
     * @param screenW  Current window width in pixels.
     * @param screenH  Current window height in pixels.
     */
    void collect(ecs::Registry& registry, int screenW, int screenH);

    /**
     * @brief Draws the items gathered by the last `collect`.
     *
     * Must be called between `Window::beginFrame` and `Window::endFrame`.
     */
    void draw() const;

    /**
     * @brief Collects then draws in one call.
     *
     * @param registry Registry to read from.
     * @param screenW  Current window width in pixels.
     * @param screenH  Current window height in pixels.
     */
    void update(ecs::Registry& registry, int screenW, int screenH);

    /**
     * @brief Measures a text element with the font it references.
     * @param text Text to measure.
     * @return Size of the rendered text in pixels.
     */
    [[nodiscard]] UiSize measureText(const UiText& text) const;

    /**
     * @brief Wraps `measureText` for `computeLayout`.
     * @return Measuring function bound to this system; must not outlive it.
     */
    [[nodiscard]] UiTextMeasure measureFunction() const;

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

    /**
     * @brief Draws a single `UiButton` at the given screen position.
     * @param x Resolved screen X in pixels.
     * @param y Resolved screen Y in pixels.
     * @param btn Component to draw.
     */
    void drawButton(float x, float y, const UiButton& btn) const;

    /** @brief Resolves font identifiers. */
    const FontStore* _fonts;

    /** @brief Items gathered for the current frame; capacity is kept between frames. */
    std::vector<UiDrawItem> _queue;

    /** @brief Layout computed by the last `collect`; capacity is kept between frames. */
    std::vector<UiLayoutNode> _layout;
};

}  // namespace ui
