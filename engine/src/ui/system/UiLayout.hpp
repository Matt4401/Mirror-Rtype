/**
 * @file UiLayout.hpp
 * @brief Resolves the screen rectangle of every HUD element, following entity hierarchies.
 */

#pragma once

#include <functional>
#include <vector>

#include "core/services/IScreenDrawService.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ui/components/UiAnchor.hpp"
#include "ui/components/UiText.hpp"

namespace ui {

/** @brief Axis-aligned rectangle in screen pixels, origin at the top-left corner (same type the draw service takes). */
using UiScreenRect = engine::ScreenRect;

/** @brief Size of an element in pixels. */
struct UiSize {
    /** @brief Width in pixels. */
    float width = 0.0F;

    /** @brief Height in pixels. */
    float height = 0.0F;
};

/** @brief One HUD element placed on screen by `computeLayout`. */
struct UiLayoutNode {
    /** @brief Element entity. */
    ecs::Entity entity = ecs::NULL_ENTITY;

    /** @brief Resolved screen rectangle of the element. */
    UiScreenRect rect;
};

/**
 * @brief Measures the on-screen size of a text element.
 *
 * Provided by the caller so the layout stays free of any graphics library: the render system measures with the
 * loaded fonts, tests can pass a fixed size.
 */
using UiTextMeasure = std::function<UiSize(const UiText&)>;

/**
 * @brief Places an element of a given size inside a container rectangle.
 *
 * The anchor point is `container.pos + anchor * container.size + offset`; the element is then shifted by
 * `pivot * size` so that its pivot lands on the anchor point.
 *
 * @param anchor Placement of the element.
 * @param container Rectangle the anchor fractions refer to (the screen, or the parent element).
 * @param size Size of the element.
 * @return Screen rectangle of the element.
 */
[[nodiscard]] UiScreenRect resolveAnchor(const UiAnchor& anchor, const UiScreenRect& container, UiSize size) noexcept;

/**
 * @brief Gets the size of a HUD element from its drawable component.
 *
 * Reads, in this order, `UiRect`, `UiProgressBar`, `UiButton`, then `UiText` (measured with `measure`). An element
 * with none of them has a zero size: it only groups and positions its children.
 *
 * @param registry Registry owning the element.
 * @param entity Element to measure.
 * @param measure Text measuring function.
 * @return Size of the element in pixels.
 */
[[nodiscard]] UiSize elementSize(const ecs::Registry& registry, ecs::Entity entity, const UiTextMeasure& measure);

/**
 * @brief Resolves the screen rectangle of every entity with a `UiAnchor`.
 *
 * - An element whose `hierarchy::Parent` is itself an element (has a `UiAnchor`) is anchored inside its parent's
 *   rectangle, so moving or resizing a panel moves its widgets.
 * - Any other element (no parent, or a parent that is not a HUD element) is anchored against the screen.
 * - Entities without a `UiAnchor` are not laid out; HUD elements below them start new screen-anchored trees.
 *
 * Nodes are emitted depth-first: a parent always comes before its children, and siblings follow the order of the
 * parent's `hierarchy::Children`. This is the draw order (later nodes are drawn on top) and the order used for
 * hit-testing. The walk uses an explicit stack, so deep trees cannot overflow the call stack.
 *
 * @param registry Registry to read.
 * @param screen Screen rectangle, usually `{0, 0, windowWidth, windowHeight}`.
 * @param measure Text measuring function.
 * @param out Receives the nodes; cleared first, its capacity is reused.
 */
void computeLayout(ecs::Registry& registry, const UiScreenRect& screen, const UiTextMeasure& measure,
                   std::vector<UiLayoutNode>& out);

}  // namespace ui
