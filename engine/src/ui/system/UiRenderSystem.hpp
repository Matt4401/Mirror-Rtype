/**
 * @file UiRenderSystem.hpp
 * @brief Turns HUD components into screen-space draw calls.
 */

#pragma once

#include <vector>

#include "core/services/IScreenDrawService.hpp"
#include "ecs/Registry.hpp"
#include "ui/components/UiButton.hpp"
#include "ui/components/UiProgressBar.hpp"
#include "ui/components/UiRect.hpp"
#include "ui/components/UiText.hpp"
#include "ui/system/UiDrawItem.hpp"
#include "ui/system/UiLayout.hpp"

namespace ui {

/**
 * @brief Draws every HUD entity that has a `UiAnchor` and a drawable UI component.
 *
 * `collect` resolves positions with `computeLayout` (so children of a HUD element are placed inside it) and fills the
 * draw queue in layout order, parents below their children; `submit` sends the queue to an
 * `engine::IScreenDrawService`, which draws it on top of the world. No graphics library is used here. The queues are
 * reused across frames to avoid per-frame heap allocation.
 */
class UiRenderSystem {
  public:
    /** @brief Letter spacing as a fraction of the font size, shared by measuring and drawing. */
    static constexpr float SPACING_RATIO = 0.1F;

    /** @brief Creates the system. */
    UiRenderSystem();

    /**
     * @brief Gathers HUD entities and resolves their screen positions.
     * @param registry Registry to read from.
     * @param screen   Screen rectangle, usually `{0, 0, windowWidth, windowHeight}`.
     * @param measure  Text measuring function.
     */
    void collect(ecs::Registry& registry, const UiScreenRect& screen, const UiTextMeasure& measure);

    /**
     * @brief Sends the items gathered by the last `collect` to a draw service.
     * @param draw Service receiving the draw calls.
     */
    void submit(engine::IScreenDrawService& draw) const;

    /**
     * @brief Collects with the service's text measurement, then submits.
     * @param registry Registry to read from.
     * @param screen   Screen rectangle.
     * @param draw     Service measuring text and receiving the draw calls.
     */
    void update(ecs::Registry& registry, const UiScreenRect& screen, engine::IScreenDrawService& draw);

    /**
     * @brief Builds a text measuring function backed by a draw service.
     * @param draw Service measuring the text; must outlive the returned function.
     * @return Measuring function using `SPACING_RATIO`.
     */
    [[nodiscard]] static UiTextMeasure measureWith(const engine::IScreenDrawService& draw);

    /**
     * @brief Items gathered by the last `collect`.
     * @return The draw queue.
     */
    [[nodiscard]] const std::vector<UiDrawItem>& drawQueue() const noexcept { return _queue; }

  private:
    /**
     * @brief Submits a `UiRect`: its fill, then its border.
     * @param draw Target service.
     * @param x Screen X in pixels.
     * @param y Screen Y in pixels.
     * @param rect Component to draw.
     */
    static void drawRect(engine::IScreenDrawService& draw, float x, float y, const UiRect& rect);

    /**
     * @brief Submits a `UiText`.
     * @param draw Target service.
     * @param x Screen X in pixels.
     * @param y Screen Y in pixels.
     * @param text Component to draw.
     */
    static void drawText(engine::IScreenDrawService& draw, float x, float y, const UiText& text);

    /**
     * @brief Submits a `UiProgressBar`: its background, then its filled part.
     * @param draw Target service.
     * @param x Screen X in pixels.
     * @param y Screen Y in pixels.
     * @param bar Component to draw.
     */
    static void drawProgressBar(engine::IScreenDrawService& draw, float x, float y, const UiProgressBar& bar);

    /**
     * @brief Submits a `UiButton`: its background for the current state, then its centred label.
     * @param draw Target service.
     * @param x Screen X in pixels.
     * @param y Screen Y in pixels.
     * @param btn Component to draw.
     */
    static void drawButton(engine::IScreenDrawService& draw, float x, float y, const UiButton& btn);

    /** @brief Items gathered for the current frame; capacity is kept between frames. */
    std::vector<UiDrawItem> _queue;

    /** @brief Layout computed by the last `collect`; capacity is kept between frames. */
    std::vector<UiLayoutNode> _layout;
};

}  // namespace ui
