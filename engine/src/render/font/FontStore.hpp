/**
 * @file FontStore.hpp
 * @brief Owns the fonts loaded through IScreenDrawService.
 */

#pragma once

#include <raylib.h>

#include <string>
#include <vector>

#include "core/services/IScreenDrawService.hpp"

namespace render {

/**
 * @brief Loads fonts and hands them out by `engine::FontId`.
 *
 * Mirrors `TextureStore`: fonts can only be loaded once a `Window` exists, and the store must be destroyed before
 * that window. `engine::NO_FONT` is never stored: callers fall back to raylib's default font for it.
 */
class FontStore {
  public:
    /** @brief Creates an empty store. */
    FontStore() = default;

    /** @brief Unloads every font still held. */
    ~FontStore();

    /** @brief Deleted copy constructor: fonts are unique GPU resources. */
    FontStore(const FontStore&) = delete;
    /** @brief Deleted copy assignment operator. */
    FontStore& operator=(const FontStore&) = delete;
    /** @brief Deleted move constructor. */
    FontStore(FontStore&&) = delete;
    /** @brief Deleted move assignment operator. */
    FontStore& operator=(FontStore&&) = delete;

    /**
     * @brief Loads a font from disk.
     * @param path Font file path (.ttf, .otf, …).
     * @param preferred Identifier to give back if it is free (the one this font had before it was released), or
     * `engine::NO_FONT` for a new one.
     * @return The identifier, or `engine::NO_FONT` if the file could not be loaded.
     */
    engine::FontId load(const std::string& path, engine::FontId preferred = engine::NO_FONT);

    /**
     * @brief Keeps every identifier up to `last` for loads that ask for it as `preferred`.
     * @param last Highest identifier to keep.
     */
    void reserve(engine::FontId last);

    /**
     * @brief Looks a font up.
     * @param id Identifier returned by `load`.
     * @return The font, or nullptr for `engine::NO_FONT` or an unknown id.
     */
    [[nodiscard]] const Font* get(engine::FontId id) const;

    /**
     * @brief Releases a font; its identifier is never given to another font, and now resolves to nothing.
     *
     * Unknown identifiers and `engine::NO_FONT` are ignored.
     * @param id Identifier returned by `load`.
     */
    void unload(engine::FontId id);

  private:
    /** @brief One font. */
    struct Slot {
        /** @brief raylib font. */
        Font font{};
        /** @brief False once unloaded; the slot is kept so its identifier is never reused. */
        bool loaded = false;
    };

    /** @brief Fonts; identifier `n` is stored at index `n - 1`. */
    std::vector<Slot> _fonts;
};

}  // namespace render
