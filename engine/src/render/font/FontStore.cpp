#include "render/font/FontStore.hpp"

#include <raylib.h>

#include <cstddef>
#include <string>

#include "core/services/IScreenDrawService.hpp"

namespace render {

FontStore::~FontStore() {
    if (!IsWindowReady()) {
        return;
    }
    for (const Font& font : _fonts) {
        UnloadFont(font);
    }
}

namespace {

/**
 * @brief Pixel size the glyphs are rasterized at: big enough for large titles, smoothly scaled down for small text
 * (raylib's default, 32, turns blurry as soon as a text is drawn bigger).
 */
constexpr int GLYPH_RASTER_SIZE = 128;

}  // namespace

engine::FontId FontStore::load(const std::string& path) {
    Font font = LoadFontEx(path.c_str(), GLYPH_RASTER_SIZE, nullptr, 0);
    // raylib falls back to its default font when the file cannot be read: that one is not ours to unload.
    if (font.texture.id == 0 || font.texture.id == GetFontDefault().texture.id) {
        return engine::NO_FONT;
    }
    GenTextureMipmaps(&font.texture);
    SetTextureFilter(font.texture, TEXTURE_FILTER_TRILINEAR);
    _fonts.emplace_back(font);
    return static_cast<engine::FontId>(_fonts.size());
}

const Font* FontStore::get(engine::FontId id) const {
    if (id == engine::NO_FONT || static_cast<std::size_t>(id) > _fonts.size()) {
        return nullptr;
    }
    return &_fonts.at(id - 1);
}

}  // namespace render
