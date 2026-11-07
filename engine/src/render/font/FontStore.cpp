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

engine::FontId FontStore::load(const std::string& path) {
    const Font font = LoadFont(path.c_str());
    if (font.texture.id == 0) {
        return engine::NO_FONT;
    }
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
