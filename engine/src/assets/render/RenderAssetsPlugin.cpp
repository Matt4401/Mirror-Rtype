#include "assets/render/RenderAssetsPlugin.hpp"

#include <string>

#include "assets/AssetKinds.hpp"
#include "assets/AssetManager.hpp"
#include "assets/AssetPlugin.hpp"
#include "components/Renderable.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "core/services/IScreenDrawService.hpp"
#include "ecs/Registry.hpp"
#include "render/RenderPlugin.hpp"
#include "render/font/FontStore.hpp"
#include "render/screen/ScreenDrawService.hpp"
#include "render/texture/TextureStore.hpp"

namespace assets {

void RenderAssetsPlugin::build(engine::PluginBuilder& builder) {
    builder.require<render::RenderPlugin>();
    builder.require<AssetPlugin>();
}

void RenderAssetsPlugin::init(engine::Engine& engine) {
    auto& renderPlugin = engine.getPlugin<render::RenderPlugin>();
    render::TextureStore* textures = renderPlugin.textures();
    render::ScreenDrawService* screen = renderPlugin.screenDraw();
    if (textures == nullptr || screen == nullptr) {
        return;
    }
    auto& manager = engine.registry().getResource<AssetManager>();
    manager.setLoader<Texture>(AssetLoader<Texture>{
        .load =
            [textures](const std::string& path, const LoadOptions& options, const ecs::TextureId preferred) {
                const render::TextureFilter filter = options.filter == TextureFilter::Smooth
                                                         ? render::TextureFilter::Smooth
                                                         : render::TextureFilter::Nearest;
                return textures->load(path, filter, preferred);
            },
        .unload = [textures](const ecs::TextureId id) { textures->unload(id); },
        .fallback = [textures]() { return textures->placeholder(); },
        .reserve = [textures](const ecs::TextureId last) { textures->reserve(last); },
    });
    render::FontStore* fonts = &screen->fonts();
    manager.setLoader<Font>(AssetLoader<Font>{
        .load = [fonts](const std::string& path, const LoadOptions& /*options*/,
                        const engine::FontId preferred) { return fonts->load(path, preferred); },
        .unload = [fonts](const engine::FontId id) { fonts->unload(id); },
        // No fallback: `NO_FONT` already draws with the backend's default font.
        .fallback = {},
        .reserve = [fonts](const engine::FontId last) { fonts->reserve(last); },
    });
    _attached = true;
}

void RenderAssetsPlugin::shutdown(engine::Engine& engine) {
    if (!_attached) {
        return;
    }
    _attached = false;
    if (!engine.registry().hasResource<AssetManager>()) {
        return;
    }
    auto& manager = engine.registry().getResource<AssetManager>();
    manager.detachLoader<Texture>();
    manager.detachLoader<Font>();
}

}  // namespace assets
