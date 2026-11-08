#include "render/font/FontStore.hpp"

#include <raylib.h>

#include <cstddef>
#include <string>

#include "core/services/IScreenDrawService.hpp"
#include "utils/StableSlots.hpp"

namespace render {

FontStore::~FontStore() {
    if (!IsWindowReady()) {
        return;
    }
    for (const Slot& slot : _fonts) {
        if (slot.loaded) {
            UnloadFont(slot.font);
        }
    }
}

namespace {

/**
 * @brief Pixel size the glyphs are rasterized at: big enough for large titles, smoothly scaled down for small text
 * (raylib's default, 32, turns blurry as soon as a text is drawn bigger).
 */
constexpr int GLYPH_RASTER_SIZE = 128;

}  // namespace

engine::FontId FontStore::load(const std::string& path, const engine::FontId preferred) {
    Font font = LoadFontEx(path.c_str(), GLYPH_RASTER_SIZE, nullptr, 0);
    // raylib falls back to its default font when the file cannot be read: that one is not ours to unload.
    if (font.texture.id == 0 || font.texture.id == GetFontDefault().texture.id) {
        return engine::NO_FONT;
    }
    GenTextureMipmaps(&font.texture);
    SetTextureFilter(font.texture, TEXTURE_FILTER_TRILINEAR);
    return ecs::placeInSlot(_fonts, Slot{.font = font, .loaded = true}, preferred);
}

void FontStore::reserve(const engine::FontId last) { ecs::reserveSlots(_fonts, last); }

const Font* FontStore::get(engine::FontId id) const {
    if (id == engine::NO_FONT || static_cast<std::size_t>(id) > _fonts.size()) {
        return nullptr;
    }
    const Slot& slot = _fonts.at(id - 1);
    return slot.loaded ? &slot.font : nullptr;
}

void FontStore::unload(const engine::FontId id) {
    if (id == engine::NO_FONT || static_cast<std::size_t>(id) > _fonts.size()) {
        return;
    }
    Slot& slot = _fonts.at(id - 1);
    if (!slot.loaded) {
        return;
    }
    if (IsWindowReady()) {
        UnloadFont(slot.font);
    }
    slot = Slot{};
}

}  // namespace render
