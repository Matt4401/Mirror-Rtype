/**
 * @file Hidden.hpp
 * @brief Tag component asking every renderer to skip an entity, with the reasons it is hidden.
 */

#pragma once

#include <cstdint>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"

namespace ecs {

/** @brief Why an entity is hidden; each owner sets and clears its own bit. */
enum class HiddenReason : std::uint8_t {
    /** @brief Hidden by game code. */
    Manual = 1U << 0U,
    /** @brief Its scene is covered by an exclusive scene. */
    Scene = 1U << 1U,
};

/**
 * @brief Present while an entity must not be drawn: UI layout and world rendering skip it.
 *
 * A UI element that is hidden takes its whole subtree with it. The component is removed as soon as no reason is left,
 * so "is it hidden" is a single `hasComponent` check.
 */
struct Hidden {
    /** @brief Bitwise OR of the `HiddenReason` values that apply. */
    std::uint8_t reasons = 0;
};

/**
 * @brief Hides an entity for a reason; does nothing for a dead entity.
 * @param registry Registry holding the entity.
 * @param entity Entity to hide.
 * @param reason Why it is hidden.
 */
inline void hide(Registry& registry, Entity entity, HiddenReason reason = HiddenReason::Manual) {
    if (!registry.isAlive(entity)) {
        return;
    }
    auto& hidden = registry.getComponents<Hidden>();
    const auto bit = static_cast<std::uint8_t>(reason);
    if (hidden.contains(entity)) {
        hidden.get(entity).reasons |= bit;
    } else {
        hidden.insertAt(entity, Hidden{.reasons = bit});
    }
}

/**
 * @brief Clears one reason an entity was hidden; it is shown again once no reason is left.
 * @param registry Registry holding the entity.
 * @param entity Entity to show.
 * @param reason Reason to clear.
 */
inline void show(Registry& registry, Entity entity, HiddenReason reason = HiddenReason::Manual) {
    auto& hidden = registry.getComponents<Hidden>();
    if (!hidden.contains(entity)) {
        return;
    }
    Hidden& state = hidden.get(entity);
    state.reasons &= static_cast<std::uint8_t>(~static_cast<std::uint8_t>(reason));
    if (state.reasons == 0) {
        registry.removeComponent<Hidden>(entity);
    }
}

/**
 * @brief Tells whether an entity is hidden for any reason.
 * @param registry Registry holding the entity.
 * @param entity Entity to check.
 * @return `true` if it carries a `Hidden` component.
 */
[[nodiscard]] inline bool isHidden(const Registry& registry, Entity entity) {
    const auto* hidden = registry.findComponents<Hidden>();
    return hidden != nullptr && hidden->contains(entity);
}

}  // namespace ecs
