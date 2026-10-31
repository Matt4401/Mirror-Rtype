/**
 * @file RenderPlugin.cpp
 * @brief Implementation of RenderPlugin and its abstract service adapters.
 */

#include "render/RenderPlugin.hpp"

#include <memory>
#include <utility>

#include "core/Engine.hpp"
#include "core/Mediator.hpp"
#include "core/MediatorError.hpp"
#include "core/PluginBuilder.hpp"
#include "core/plugins/CorePlugin.hpp"
#include "core/services/IRenderService.hpp"
#include "core/services/IWindowService.hpp"
#include "render/texture/TextureStore.hpp"
#include "render/window/Window.hpp"
#include "render/window/WindowConfig.hpp"
#include "systems/render/RenderSystem.hpp"

namespace render {

/**
 * @brief Concrete adapter implementing IWindowService via Raylib Window.
 */
class RenderPlugin::WindowServiceImpl final : public engine::IWindowService {
  public:
    /**
     * @brief Constructs WindowServiceImpl with specified window configuration.
     * @param config Window dimension and display configuration.
     */
    explicit WindowServiceImpl(WindowConfig config) : _config(std::move(config)) {}

    [[nodiscard]] bool shouldClose() const noexcept override { return Window::shouldClose(); }

    void beginFrame() override { Window::beginFrame(); }

    void endFrame() override { Window::endFrame(); }

    [[nodiscard]] int width() const noexcept override { return _config.width; }

    [[nodiscard]] int height() const noexcept override { return _config.height; }

  private:
    /** @brief Stored window configuration. */
    WindowConfig _config;
};

/**
 * @brief Concrete adapter implementing IRenderService via RenderSystem.
 */
class RenderPlugin::RenderServiceImpl final : public engine::IRenderService {
  public:
    /**
     * @brief Constructs RenderServiceImpl wrapping a RenderSystem reference.
     * @param system Reference to the active RenderSystem.
     */
    explicit RenderServiceImpl(RenderSystem& system) : _system(&system) {}

    void collect(ecs::Registry& registry) override {
        if (_system != nullptr) {
            _system->collect(registry);
        }
    }

    void draw() const override {
        if (_system != nullptr) {
            _system->draw();
        }
    }

    void update(ecs::Registry& registry) override {
        if (_system != nullptr) {
            _system->update(registry);
        }
    }

    /**
     * @brief Sets or updates the wrapped RenderSystem pointer.
     * @param system Pointer to RenderSystem instance.
     */
    void setSystem(RenderSystem* system) noexcept { _system = system; }

  private:
    /** @brief Non-owning pointer to the underlying RenderSystem. */
    RenderSystem* _system = nullptr;
};

RenderPlugin::RenderPlugin(WindowConfig config) : _config(std::move(config)) {}

RenderPlugin::~RenderPlugin() {
    _renderService.reset();
    _windowService.reset();
    _renderSystem.reset();
    _textures.reset();
    _window.reset();
}

void RenderPlugin::build(engine::PluginBuilder& builder) {
    builder.require<engine::CorePlugin>();
    builder.provides<engine::IWindowService>();
    builder.provides<engine::IRenderService>();
}

void RenderPlugin::init(engine::Engine& engine) {
    _window = std::make_unique<Window>(_config);
    _textures = std::make_unique<TextureStore>();
    _renderSystem = std::make_unique<RenderSystem>(*_textures);

    _windowService = std::make_shared<WindowServiceImpl>(_config);
    _renderService = std::make_shared<RenderServiceImpl>(*_renderSystem);

    if (const auto res = engine.mediator().registerService<engine::IWindowService>(_windowService); !res) {
        throw engine::MediatorError(res.error());
    }
    if (const auto res = engine.mediator().registerService<engine::IRenderService>(_renderService); !res) {
        engine.mediator().removeService<engine::IWindowService>();
        throw engine::MediatorError(res.error());
    }
}

void RenderPlugin::render(engine::Engine& engine) {
    Window::beginFrame();
    if (_renderSystem != nullptr) {
        _renderSystem->update(engine.registry());
    }
    Window::endFrame();
}

void RenderPlugin::shutdown(engine::Engine& engine) {
    if (engine.mediator().hasService<engine::IRenderService>()) {
        engine.mediator().removeService<engine::IRenderService>();
    }
    if (engine.mediator().hasService<engine::IWindowService>()) {
        engine.mediator().removeService<engine::IWindowService>();
    }

    if (_renderService != nullptr) {
        _renderService->setSystem(nullptr);
    }
    _renderService.reset();
    _windowService.reset();
    _renderSystem.reset();
    _textures.reset();
    _window.reset();
}

}  // namespace render
