/**
 * @file UiRenderSystem.hpp
 * @brief Draws the HUD: lays it out, then submits every widget to the screen-draw service.
 */

#pragma once

#include <vector>

#include "core/services/IScreenDrawService.hpp"
#include "ecs/Registry.hpp"
#include "ui/components/UiButton.hpp"
#include "ui/components/UiProgressBar.hpp"
#include "ui/components/UiRect.hpp"
#include "ui/components/UiSlider.hpp"
#include "ui/components/UiText.hpp"
#include "ui/components/UiTextInput.hpp"
#include "ui/components/UiToggle.hpp"
#include "ui/system/UiLayout.hpp"

namespace ui {

/**
 * @brief Draws the HUD in screen space.
 *
 * Each frame, the HUD is laid out (`computeLayout`) and every node is drawn straight from its components, in layout
 * order (parents first). One entity may carry several drawables; they are stacked bottom to top in a fixed order:
 * `UiRect`, `UiImage`, `UiProgressBar`, `UiButton`, `UiToggle`, `UiSlider`, `UiTextInput`, `UiText`, then the focus
 * outline.
 *
 * Nothing is copied from the components and the layout buffer keeps its capacity, so a steady HUD draws without
 * allocating.
 */
class UiRenderSystem {
  public:
    /** @brief Letter spacing as a fraction of the font size, for every text drawn by the HUD. */
    static constexpr float SPACING_RATIO = 0.1F;

    /** @brief Horizontal padding of the text inside a text input, in pixels. */
    static constexpr float TEXT_INPUT_PADDING = 8.0F;

    /** @brief Gap between a focused widget and its outline, in pixels. */
    static constexpr float FOCUS_GAP = 3.0F;

    /** @brief Reserves the layout buffer. */
    UiRenderSystem();

    /**
     * @brief Lays out the HUD for this frame.
     * @param registry Registry holding the HUD.
     * @param screen Screen rectangle.
     * @param measure Text measuring function.
     */
    void collect(ecs::Registry& registry, const UiScreenRect& screen, const UiTextMeasure& measure);

    /**
     * @brief Draws the nodes laid out by the last `collect`.
     * @param registry Registry holding the HUD.
     * @param draw Screen-draw service.
     */
    void submit(const ecs::Registry& registry, engine::IScreenDrawService& draw) const;

    /**
     * @brief Lays out then draws the HUD.
     * @param registry Registry holding the HUD.
     * @param screen Screen rectangle.
     * @param draw Screen-draw service, also used to measure text.
     */
    void update(ecs::Registry& registry, const UiScreenRect& screen, engine::IScreenDrawService& draw);

    /**
     * @brief Builds a text measuring function backed by the draw service.
     * @param draw Screen-draw service; must outlive the returned function.
     * @return Function measuring a `UiText` with the HUD's spacing.
     */
    [[nodiscard]] static UiTextMeasure measureWith(const engine::IScreenDrawService& draw);

    /**
     * @brief Gets the layout of the last `collect`.
     * @return Nodes in draw order.
     */
    [[nodiscard]] const std::vector<UiLayoutNode>& layout() const noexcept { return _layout; }

  private:
    /**
     * @brief Draws a rectangle.
     * @param draw Screen-draw service.
     * @param area Where to draw.
     * @param rect Component.
     */
    static void drawRect(engine::IScreenDrawService& draw, const UiScreenRect& area, const UiRect& rect);
    /**
     * @brief Draws a text.
     * @param draw Screen-draw service.
     * @param area Where to draw.
     * @param text Component.
     */
    static void drawText(engine::IScreenDrawService& draw, const UiScreenRect& area, const UiText& text);
    /**
     * @brief Draws a progress bar.
     * @param draw Screen-draw service.
     * @param area Where to draw.
     * @param bar Component.
     */
    static void drawProgressBar(engine::IScreenDrawService& draw, const UiScreenRect& area, const UiProgressBar& bar);
    /**
     * @brief Draws a button and its centred label.
     * @param draw Screen-draw service.
     * @param area Where to draw.
     * @param btn Component.
     */
    static void drawButton(engine::IScreenDrawService& draw, const UiScreenRect& area, const UiButton& btn);
    /**
     * @brief Draws a toggle box, with its mark while on.
     * @param draw Screen-draw service.
     * @param area Where to draw.
     * @param toggle Component.
     */
    static void drawToggle(engine::IScreenDrawService& draw, const UiScreenRect& area, const UiToggle& toggle);
    /**
     * @brief Draws a slider track, its filled part and its knob.
     * @param draw Screen-draw service.
     * @param area Where to draw.
     * @param slider Component.
     */
    static void drawSlider(engine::IScreenDrawService& draw, const UiScreenRect& area, const UiSlider& slider);
    /**
     * @brief Draws a text input, its text or placeholder, and its caret.
     * @param draw Screen-draw service.
     * @param area Where to draw.
     * @param input Component.
     * @param caret `true` to draw the caret.
     */
    static void drawTextInput(engine::IScreenDrawService& draw, const UiScreenRect& area, const UiTextInput& input,
                              bool caret);

    /** @brief Layout of the current frame; capacity kept between frames. */
    std::vector<UiLayoutNode> _layout;
};

}  // namespace ui
