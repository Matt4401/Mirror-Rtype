#include "ui/font/FontStore.hpp"

#include <cstddef>

namespace ui {

FontStore::~FontStore() {
    if (!IsWindowReady()) {
        return;
    }
    for (std::size_t i = 0; i < _fonts.size(); ++i) {
        if (_owned.at(i)) {
            UnloadFont(_fonts.at(i));
        }
    }
}

FontId FontStore::load(const std::string& path) {
    Font font = LoadFont(path.c_str());
    if (font.texture.id == 0) {
        return NO_FONT;
    }
    _fonts.push_back(font);
    _owned.push_back(true);
    return static_cast<FontId>(_fonts.size());
}

FontId FontStore::loadDefault() {
    _fonts.push_back(GetFontDefault());
    _owned.push_back(false);
    return static_cast<FontId>(_fonts.size());
}

const Font* FontStore::get(FontId id) const {
    if (id == NO_FONT || static_cast<std::size_t>(id) > _fonts.size()) {
        return nullptr;
    }
    return &_fonts.at(id - 1);
}

}  // namespace ui
