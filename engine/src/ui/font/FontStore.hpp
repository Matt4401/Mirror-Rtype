#pragma once

#include <raylib.h>

#include <string>
#include <vector>

#include "ui/components/FontId.hpp"

namespace ui {

/**
 * @brief Loads fonts and hands them out by `ui::FontId`.
 *
 * Mirrors `render::TextureStore`: fonts can only be loaded once a `Window` exists,
 * and the store must be destroyed before that window.
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
     *
     * @param path Font file path (.ttf, .otf, …).
     * @return The new identifier, or `NO_FONT` if the file could not be loaded.
     */
    FontId load(const std::string& path);

    /**
     * @brief Looks a font up.
     *
     * @param id Identifier returned by `load`.
     * @return The font, or nullptr if `id` is unknown.
     */
    [[nodiscard]] const Font* get(FontId id) const;

  private:
    /** @brief Loaded fonts; identifier `n` is stored at index `n - 1`. */
    std::vector<Font> _fonts;
};

}  // namespace ui
