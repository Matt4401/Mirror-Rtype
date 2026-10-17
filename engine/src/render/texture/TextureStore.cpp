#include "render/texture/TextureStore.hpp"

#include <raylib.h>

#include <cstddef>
#include <string>

#include "components/Renderable.hpp"

namespace render {

TextureStore::~TextureStore() {
    for (const Texture2D& texture : _textures) {
        UnloadTexture(texture);
    }
}

ecs::TextureId TextureStore::load(const std::string& path) {
    const Texture2D texture = LoadTexture(path.c_str());
    if (texture.id == 0) {
        return ecs::NO_TEXTURE;
    }
    _textures.push_back(texture);
    return static_cast<ecs::TextureId>(_textures.size());
}

const Texture2D* TextureStore::get(ecs::TextureId id) const {
    if (id == ecs::NO_TEXTURE || static_cast<std::size_t>(id) > _textures.size()) {
        return nullptr;
    }
    return &_textures.at(id - 1);
}

}  // namespace render
