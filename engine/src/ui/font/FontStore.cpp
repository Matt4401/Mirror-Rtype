#include "ui/font/FontStore.hpp"

#include <cstddef>

namespace ui {

FontStore::~FontStore() {
    if (!IsWindowReady()) {
        return;
    }
    for (const Font& font : _fonts) {
        UnloadFont(font);
    }
}

FontId FontStore::load(const std::string& path) {
    Font font = LoadFont(path.c_str());
    if (font.texture.id == 0) {
        return NO_FONT;
    }
    _fonts.push_back(font);
    return static_cast<FontId>(_fonts.size());
}

const Font* FontStore::get(FontId id) const {
    if (id == NO_FONT || static_cast<std::size_t>(id) > _fonts.size()) {
        return nullptr;
    }
    return &_fonts.at(id - 1);
}

}  // namespace ui
