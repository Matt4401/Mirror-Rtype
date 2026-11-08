#pragma once

#include <raylib.h>

#include <cstddef>
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

    /**
     * @brief Releases a texture; its identifier is never given to another texture, and now resolves to nothing.
     *
     * Unknown identifiers, `ecs::NO_TEXTURE` and the placeholder are ignored.
     * @param id Identifier returned by `load`.
     */
    void unload(ecs::TextureId id);

    /**
     * @brief Gets the texture drawn in place of a missing file: a magenta and black checkerboard, made on first use.
     * @return Its identifier, or `ecs::NO_TEXTURE` if it cannot be created (no window).
     */
    ecs::TextureId placeholder();

    /**
     * @brief Counts the textures held, the placeholder included.
     * @return Number of loaded textures.
     */
    [[nodiscard]] std::size_t loadedCount() const noexcept;

  private:
    /** @brief One texture. */
    struct Slot {
        /** @brief GPU texture. */
        Texture2D texture{};
        /** @brief False once unloaded; the slot is kept so its identifier is never reused. */
        bool loaded = false;
    };

    /** @brief Textures; identifier `n` is stored at index `n - 1`. */
    std::vector<Slot> _textures;
    /** @brief Identifier of the placeholder once made. */
    ecs::TextureId _placeholder = ecs::NO_TEXTURE;
};

}  // namespace render
