/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** Holds a pool of world positions for a single shared sprite.
*/

#pragma once

#include <vector>

namespace ecs {

/**
 * @brief Renders one sprite at multiple world positions without spawning one entity per occurrence.
 *
 * Attach this alongside a Renderable and (optionally) a ParallaxLayer. The RenderSystem draws
 * the shared sprite once per instance; the ParallaxSystem scrolls and wraps every instance
 * position independently.
 */
struct SpriteInstances {
    /** @brief One world position for a single draw of the shared sprite. */
    struct Instance {
        /** @brief Horizontal world position. */
        float x = 0.0F;
        /** @brief Vertical world position. */
        float y = 0.0F;
    };

    /** @brief List of positions at which the sprite is drawn. */
    std::vector<Instance> instances;

    /** @brief Horizontal scale factor shared by all instances. */
    float scaleX = 1.0F;

    /** @brief Vertical scale factor shared by all instances. */
    float scaleY = 1.0F;

    /** @brief Z depth shared by all instances (used for draw ordering). */
    float z = 0.0F;

    /** @brief Rotation in radians shared by all instances. */
    float rotation = 0.0F;
};

}  // namespace ecs
