/**
 * @file RenderPlugin.hpp
 * @brief Rendering lifecycle plugin managing Window, TextureStore, and RenderSystem.
 */

#pragma once

#include <memory>
#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "render/texture/TextureStore.hpp"
#include "render/window/Window.hpp"
#include "render/window/WindowConfig.hpp"
#include "systems/render/RenderSystem.hpp"

namespace render {

/**
 * @brief Plugin responsible for window lifecycle, texture management, and 2D ECS rendering.
 */
class RenderPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Constructs RenderPlugin with optional window configuration.
     * @param config Window configuration parameters.
     */
    explicit RenderPlugin(WindowConfig config = WindowConfig{});

    /** @brief Destructor releasing graphics resources in safe order. */
    ~RenderPlugin() override;

    /** @brief Deleted copy constructor. */
    RenderPlugin(const RenderPlugin&) = delete;

    /** @brief Deleted copy assignment operator. */
    RenderPlugin& operator=(const RenderPlugin&) = delete;

    /** @brief Move constructor. */
    RenderPlugin(RenderPlugin&&) noexcept;

    /** @brief Move assignment operator. */
    RenderPlugin& operator=(RenderPlugin&&) noexcept;

    /**
     * @brief Gets plugin name.
     * @return String view "RenderPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "RenderPlugin"; }

    /**
     * @brief Declares plugin dependencies.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Initializes Window, TextureStore, RenderSystem and registers services.
     * @param engine Reference to host Engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Frame rendering phase: collects visible entities and presents through window.
     * @param engine Reference to host Engine.
     */
    void render(engine::Engine& engine) override;

    /**
     * @brief Tears down rendering resources.
     * @param engine Reference to host Engine.
     */
    void shutdown(engine::Engine& engine) override;

    /** @brief Gets mutable pointer to the internal TextureStore, or nullptr if uninitialized. */
    [[nodiscard]] TextureStore* textures() noexcept { return _textures.get(); }

    /** @brief Gets const pointer to the internal TextureStore, or nullptr if uninitialized. */
    [[nodiscard]] const TextureStore* textures() const noexcept { return _textures.get(); }

    /** @brief Gets mutable pointer to the active RenderSystem, or nullptr if uninitialized. */
    [[nodiscard]] RenderSystem* renderSystem() noexcept { return _renderSystem.get(); }

  private:
    class WindowServiceImpl;
    class RenderServiceImpl;

    WindowConfig _config = {};
    std::unique_ptr<Window> _window = nullptr;
    std::unique_ptr<TextureStore> _textures = nullptr;
    std::unique_ptr<RenderSystem> _renderSystem = nullptr;
    std::shared_ptr<WindowServiceImpl> _windowService = nullptr;
    std::shared_ptr<RenderServiceImpl> _renderService = nullptr;
};

}  // namespace render
