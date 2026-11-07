#include "ui/system/UiLayout.hpp"

#include <optional>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "hierarchy/Hierarchy.hpp"
#include "ui/components/UiAnchor.hpp"
#include "ui/components/UiButton.hpp"
#include "ui/components/UiProgressBar.hpp"
#include "ui/components/UiRect.hpp"
#include "ui/components/UiText.hpp"

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
    if (registry.hasComponent<UiText>(entity) && measure) {
        return measure(registry.getComponents<UiText>().get(entity));
    }
    return {};
}

void computeLayout(ecs::Registry& registry, const UiScreenRect& screen, const UiTextMeasure& measure,
                   std::vector<UiLayoutNode>& out) {
    out.clear();

    std::vector<ecs::Entity> roots;
    registry.runSystem<UiAnchor>([&registry, &roots](ecs::Entity entity, const UiAnchor& /*anchor*/) {
        if (isLayoutRoot(registry, entity)) {
            roots.push_back(entity);
        }
    });

    // Each pending element is paired with the rectangle its anchor refers to.
    std::vector<std::pair<ecs::Entity, UiScreenRect>> stack;
    for (const ecs::Entity root : roots) {
        stack.emplace_back(root, screen);
        while (!stack.empty()) {
            const auto [entity, container] = stack.back();
            stack.pop_back();

            const UiAnchor& anchor = registry.getComponents<UiAnchor>().get(entity);
            const UiScreenRect rect = resolveAnchor(anchor, container, elementSize(registry, entity, measure));
            out.push_back(UiLayoutNode{.entity = entity, .rect = rect});

            // Pushed in reverse so that the first child is popped, hence emitted and drawn, first.
            const std::span<const ecs::Entity> children = hierarchy::childrenOf(registry, entity);
            for (const ecs::Entity child : std::views::reverse(children)) {
                if (registry.isAlive(child) && registry.hasComponent<UiAnchor>(child)) {
                    stack.emplace_back(child, rect);
                }
            }
        }
    }
}

}  // namespace ui
