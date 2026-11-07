#pragma once
#include <cstdint>

namespace ecs {

/**
 * @brief Generational handle identifying an entity in the Entity Component System.
 *
 * An entity is still a single integer, but it packs two fields:
 * - bits 0-31: the **index** of the slot the entity occupies (used to address component storage);
 * - bits 32-51: the **generation** of that slot, bumped every time an entity in the slot is killed.
 *
 * Bits 52-63 are always zero, so every handle is exactly representable as a `double` (53-bit mantissa). This lets
 * handles cross the scripting boundary (Wren numbers are doubles) without losing precision.
 *
 * When a slot is recycled, the new entity gets a different generation, so an old handle kept somewhere
 * (a script, a component referencing another entity, ...) is detected as dead instead of silently pointing at the
 * new occupant.
 */
using Entity = std::uint64_t;

/** @brief Number of bits used by the slot index of an `Entity`. */
inline constexpr unsigned ENTITY_INDEX_BITS = 32;
/** @brief Number of bits used by the generation of an `Entity`. */
inline constexpr unsigned ENTITY_GENERATION_BITS = 20;
/** @brief Largest slot index an `Entity` can address. */
inline constexpr std::uint32_t MAX_ENTITY_INDEX = 0xFFFF'FFFFU;
/** @brief Largest generation before it wraps back to zero. */
inline constexpr std::uint32_t MAX_ENTITY_GENERATION = (1U << ENTITY_GENERATION_BITS) - 1U;

/**
 * @brief Extracts the slot index of an entity handle.
 * @param e Entity handle.
 * @return Slot index (low 32 bits).
 */
[[nodiscard]] constexpr std::uint32_t entityIndex(Entity e) noexcept {
    return static_cast<std::uint32_t>(e & MAX_ENTITY_INDEX);
}

/**
 * @brief Extracts the generation of an entity handle.
 * @param e Entity handle.
 * @return Generation (bits 32-51).
 */
[[nodiscard]] constexpr std::uint32_t entityGeneration(Entity e) noexcept {
    return static_cast<std::uint32_t>(e >> ENTITY_INDEX_BITS) & MAX_ENTITY_GENERATION;
}

/**
 * @brief Builds an entity handle from a slot index and a generation.
 * @param index Slot index.
 * @param generation Generation, masked to `ENTITY_GENERATION_BITS` bits.
 * @return Packed entity handle.
 */
[[nodiscard]] constexpr Entity makeEntity(std::uint32_t index, std::uint32_t generation) noexcept {
    return (static_cast<Entity>(generation & MAX_ENTITY_GENERATION) << ENTITY_INDEX_BITS) | index;
}

/**
 * @brief Handle that never refers to a live entity.
 *
 * `Registry::spawnEntity` never hands out the last slot index, so this value can be used as a "no entity" sentinel.
 */
inline constexpr Entity NULL_ENTITY = makeEntity(MAX_ENTITY_INDEX, MAX_ENTITY_GENERATION);

}  // namespace ecs
