#include "render/texture/TextureStore.hpp"

#include <raylib.h>

#include <algorithm>
#include <cstddef>
#include <string>

#include "components/Renderable.hpp"
#include "utils/StableSlots.hpp"

namespace render {

namespace {

/** @brief Side of the placeholder texture, in pixels. */
constexpr int PLACEHOLDER_SIZE = 64;
/** @brief Side of one square of the placeholder checkerboard, in pixels. */
constexpr int PLACEHOLDER_SQUARE = 16;
/** @brief Magenta squares of the placeholder: a color no real asset uses, so a missing file is obvious. */
constexpr Color PLACEHOLDER_MAGENTA = {.r = 255, .g = 0, .b = 255, .a = 255};
/** @brief Dark squares of the placeholder. */
constexpr Color PLACEHOLDER_DARK = {.r = 0, .g = 0, .b = 0, .a = 255};

}  // namespace

TextureStore::~TextureStore() {
    if (!IsWindowReady()) {
        return;
    }

    for (const Slot& slot : _textures) {
        if (slot.loaded) {
            UnloadTexture(slot.texture);
        }
    }
}

ecs::TextureId TextureStore::load(const std::string& path, const TextureFilter filter, const ecs::TextureId preferred) {
    Texture2D texture = LoadTexture(path.c_str());
    if (texture.id == 0) {
        return ecs::NO_TEXTURE;
    }
    if (filter == TextureFilter::Smooth) {
        GenTextureMipmaps(&texture);
        SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);
        // Clamped: the filter must not blend an edge with the opposite one (seams between tiled pictures).
        SetTextureWrap(texture, TEXTURE_WRAP_CLAMP);
    }
    return ecs::placeInSlot(_textures, Slot{.texture = texture, .loaded = true}, preferred);
}

void TextureStore::reserve(const ecs::TextureId last) { ecs::reserveSlots(_textures, last); }

const Texture2D* TextureStore::get(ecs::TextureId id) const {
    if (id == ecs::NO_TEXTURE || static_cast<std::size_t>(id) > _textures.size()) {
        return nullptr;
    }
    const Slot& slot = _textures.at(id - 1);
    return slot.loaded ? &slot.texture : nullptr;
}

void TextureStore::unload(const ecs::TextureId id) {
    if (id == ecs::NO_TEXTURE || id == _placeholder || static_cast<std::size_t>(id) > _textures.size()) {
        return;
    }
    Slot& slot = _textures.at(id - 1);
    if (!slot.loaded) {
        return;
    }
    if (IsWindowReady()) {
        UnloadTexture(slot.texture);
    }
    slot = Slot{};
}

ecs::TextureId TextureStore::placeholder() {
    if (_placeholder != ecs::NO_TEXTURE || !IsWindowReady()) {
        return _placeholder;
    }
    const Image image = GenImageChecked(PLACEHOLDER_SIZE, PLACEHOLDER_SIZE, PLACEHOLDER_SQUARE, PLACEHOLDER_SQUARE,
                                        PLACEHOLDER_MAGENTA, PLACEHOLDER_DARK);
    const Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    if (texture.id == 0) {
        return ecs::NO_TEXTURE;
    }
    _textures.push_back(Slot{.texture = texture, .loaded = true});
    _placeholder = static_cast<ecs::TextureId>(_textures.size());
    return _placeholder;
}

std::size_t TextureStore::loadedCount() const noexcept {
    return static_cast<std::size_t>(std::ranges::count_if(_textures, [](const Slot& slot) { return slot.loaded; }));
}

}  // namespace render
