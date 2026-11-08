#include "ui/system/UiLayout.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include "components/Hidden.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "hierarchy/Hierarchy.hpp"
#include "ui/components/UiAnchor.hpp"
#include "ui/components/UiButton.hpp"
#include "ui/components/UiProgressBar.hpp"
#include "ui/components/UiRect.hpp"
#include "ui/components/UiScrollList.hpp"
#include "ui/components/UiSlider.hpp"
#include "ui/components/UiText.hpp"
#include "ui/components/UiTextInput.hpp"
#include "ui/components/UiToggle.hpp"

namespace ui {

namespace {

/**
 * @brief Checks whether an element is laid out against the screen rather than against a parent element.
 * @param registry Registry owning the element.
 * @param entity Element with a `UiAnchor`.
 * @return True if it has no parent, or a parent that is dead or not a HUD element.
 */
bool isLayoutRoot(const ecs::Registry& registry, ecs::Entity entity) {
    const std::optional<ecs::Entity> parent = hierarchy::parentOf(registry, entity);
    return !parent.has_value() || !registry.isAlive(*parent) || !registry.hasComponent<UiAnchor>(*parent);
}

/**
 * @brief Queues the rows of a scroll list that are in view, each in its slot, so the first is laid out first.
 * @tparam Push Callable taking a child and its container rectangle.
 * @param list Scroll list; `first` is clamped to the number of rows.
 * @param rows Children of the list.
 * @param rect Rectangle of the list.
 * @param push Queues one row.
 */
template <typename Push>
void pushRowsInView(UiScrollList& list, std::span<const ecs::Entity> rows, const UiScreenRect& rect, const Push& push) {
    const std::size_t count = rows.size();
    const std::size_t maxFirst = count > list.visibleCount ? count - list.visibleCount : 0;
    list.first = std::min(list.first, maxFirst);
    const std::size_t last = std::min(count, list.first + list.visibleCount);
    for (std::size_t i = last; i-- > list.first;) {
        const auto slot = static_cast<float>(i - list.first);
        push(rows[i],
             UiScreenRect{
                 .x = rect.x, .y = rect.y + (slot * list.itemHeight), .width = rect.width, .height = list.itemHeight});
    }
}

}  // namespace

UiScreenRect resolveAnchor(const UiAnchor& anchor, const UiScreenRect& container, UiSize size) noexcept {
    return UiScreenRect{
        .x = container.x + (anchor.anchorX * container.width) + anchor.offsetX - (anchor.pivotX * size.width),
        .y = container.y + (anchor.anchorY * container.height) + anchor.offsetY - (anchor.pivotY * size.height),
        .width = size.width,
        .height = size.height,
    };
}

UiSize elementSize(const ecs::Registry& registry, ecs::Entity entity, const UiTextMeasure& measure) {
    if (registry.hasComponent<UiRect>(entity)) {
        const auto& rect = registry.getComponents<UiRect>().get(entity);
        return {.width = rect.width, .height = rect.height};
    }
    if (registry.hasComponent<UiProgressBar>(entity)) {
        const auto& bar = registry.getComponents<UiProgressBar>().get(entity);
        return {.width = bar.width, .height = bar.height};
    }
    if (registry.hasComponent<UiButton>(entity)) {
        const auto& btn = registry.getComponents<UiButton>().get(entity);
        return {.width = btn.width, .height = btn.height};
    }
    if (registry.hasComponent<UiTextInput>(entity)) {
        const auto& input = registry.getComponents<UiTextInput>().get(entity);
        return {.width = input.width, .height = input.height};
    }
    if (registry.hasComponent<UiSlider>(entity)) {
        const auto& slider = registry.getComponents<UiSlider>().get(entity);
        return {.width = slider.width, .height = slider.height};
    }
    if (registry.hasComponent<UiToggle>(entity)) {
        const auto& toggle = registry.getComponents<UiToggle>().get(entity);
        return {.width = toggle.width, .height = toggle.height};
    }
    if (registry.hasComponent<UiText>(entity) && measure) {
        return measure(registry.getComponents<UiText>().get(entity));
    }
    return {};
}

void computeLayout(ecs::Registry& registry, const UiScreenRect& screen, const UiTextMeasure& measure,
                   std::vector<UiLayoutNode>& out) {
    out.clear();
    // Scratch buffers keep their capacity between frames: a steady UI lays out without allocating.
    thread_local std::vector<ecs::Entity> roots;
    thread_local std::vector<std::pair<ecs::Entity, UiScreenRect>> stack;
    roots.clear();
    stack.clear();

    const auto* hidden = static_cast<const ecs::Registry&>(registry).findComponents<ecs::Hidden>();
    const auto isHidden = [hidden](ecs::Entity entity) { return hidden != nullptr && hidden->contains(entity); };

    registry.runSystem<UiAnchor>([&registry, &isHidden](ecs::Entity entity, const UiAnchor& /*anchor*/) {
        if (!isHidden(entity) && isLayoutRoot(registry, entity)) {
            roots.push_back(entity);
        }
    });

    auto* lists =
        registry.findComponents<UiScrollList>() != nullptr ? &registry.getComponents<UiScrollList>() : nullptr;
    const auto pushChild = [&registry, &isHidden](ecs::Entity child, const UiScreenRect& container) {
        if (registry.isAlive(child) && registry.hasComponent<UiAnchor>(child) && !isHidden(child)) {
            stack.emplace_back(child, container);
        }
    };

    for (const ecs::Entity root : roots) {
        stack.emplace_back(root, screen);
        while (!stack.empty()) {
            const auto [entity, container] = stack.back();
            stack.pop_back();

            const UiAnchor& anchor = registry.getComponents<UiAnchor>().get(entity);
            const UiScreenRect rect = resolveAnchor(anchor, container, elementSize(registry, entity, measure));
            out.push_back(UiLayoutNode{.entity = entity, .rect = rect});

            const std::span<const ecs::Entity> children = hierarchy::childrenOf(registry, entity);
            if (lists != nullptr && lists->contains(entity)) {
                pushRowsInView(lists->get(entity), children, rect, pushChild);
                continue;
            }
            for (const ecs::Entity child : std::views::reverse(children)) {
                pushChild(child, rect);
            }
        }
    }
}

}  // namespace ui
