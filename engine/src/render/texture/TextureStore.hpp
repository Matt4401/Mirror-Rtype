#pragma once

#include <raylib.h>

#include <cstdint>
#include <string>
#include <vector>

#include "components/Renderable.hpp"

namespace render {

/** @brief How a texture is sampled when drawn bigger or smaller than its pixel size. */
enum class TextureFilter : std::uint8_t {
    /** @brief Nearest pixel: crisp pixel art. */
    Nearest,
    /** @brief Mipmapped trilinear filtering, clamped edges: smooth artwork that stays clean when zoomed in or out. */
    Smooth,
};

/**
 * @brief Loads textures and hands them out by `ecs::TextureId`.
 *
 * Minimal owner of GPU textures for the rendering module. Textures can only be loaded once a `Window`
 * exists, and the store must be destroyed before that window.
 */
class TextureStore {
  public:
    /** @brief Creates an empty store. */
    TextureStore() = default;

    /** @brief Unloads every texture still held. */
    ~TextureStore();

    /** @brief Deleted copy constructor: textures are unique GPU resources. */
    TextureStore(const TextureStore&) = delete;
    /** @brief Deleted copy assignment operator. */
    TextureStore& operator=(const TextureStore&) = delete;
    /** @brief Deleted move constructor. */
    TextureStore(TextureStore&&) = delete;
    /** @brief Deleted move assignment operator. */
    TextureStore& operator=(TextureStore&&) = delete;

    /**
     * @brief Loads a texture from disk.
     *
     * @param path Image file path.
     * @param filter Sampling used when the texture is scaled.
     * @return The new identifier, or `ecs::NO_TEXTURE` if the file could not be loaded.
     */
    ecs::TextureId load(const std::string& path, TextureFilter filter = TextureFilter::Nearest);

    /**
     * @brief Looks a texture up.
     *
     * @param id Identifier returned by `load`.
     * @return The texture, or nullptr if `id` is unknown.
     */
    [[nodiscard]] const Texture2D* get(ecs::TextureId id) const;

  private:
    /** @brief Loaded textures; identifier `n` is stored at index `n - 1`. */
    std::vector<Texture2D> _textures;
};

}  // namespace render
