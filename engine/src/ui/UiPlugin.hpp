/**
 * @file UiPlugin.hpp
 * @brief Plugin driving HUD layout, button interaction and HUD drawing.
 */

#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/time/Time.hpp"
#include "ui/system/UiLayout.hpp"
#include "ui/system/UiRenderSystem.hpp"

namespace ui {

/**
 * @brief Engine plugin driving HUD layout, button interaction and HUD drawing.
 *
 * - `update`: lays the HUD out and runs `UiButtonSystem` (hover, press, click events).
 * - `lateUpdate`: lays the HUD out again, after every plugin's `update`, and submits it to
 *   `engine::IScreenDrawService`, which draws it on top of the world in the same frame.
 *
 * Depends only on services (`engine::IWindowService`, `engine::IScreenDrawService`), never on the render backend,
 * so this module contains no graphics-library code. Fonts are loaded through `IScreenDrawService::loadFont`.
 *
 * HUD elements can be nested with `hierarchy::setParent`: a child element is anchored inside its parent's rectangle
 * (see `computeLayout`). `hierarchy::HierarchyPlugin` is required so that destroying a panel destroys its widgets.
 */
class UiPlugin final : public engine::IPlugin {
  public:
    /** @brief Constructs UiPlugin. */
    UiPlugin() = default;

    /** @brief Destructor. */
    ~UiPlugin() override = default;

    /** @brief Deleted copy constructor. */
    UiPlugin(const UiPlugin&) = delete;

    /** @brief Deleted copy assignment. */
    UiPlugin& operator=(const UiPlugin&) = delete;

    /** @brief Deleted move constructor. */
    UiPlugin(UiPlugin&&) = delete;

    /** @brief Deleted move assignment. */
    UiPlugin& operator=(UiPlugin&&) = delete;

    /**
     * @brief Gets plugin name.
     * @return String view "UiPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "UiPlugin"; }

    /**
     * @brief Declares the window and screen-draw services and the hierarchy plugin as dependencies.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Creates the UI render system.
     * @param engine Reference to host Engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Lays the HUD out and updates button interaction states.
     * @param engine Reference to host Engine.
     * @param time   Current frame time (unused).
     */
    void update(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Submits the HUD to the screen-draw service for this frame.
     * @param engine Reference to host Engine.
     * @param time   Current frame time (unused).
     */
    void lateUpdate(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Releases the UI render system and the layout buffer.
     * @param engine Reference to host Engine (unused).
     */
    void shutdown(engine::Engine& engine) override;

  private:
    /**
     * @brief Gets the screen rectangle from the window service.
     * @param engine Reference to host Engine.
     * @return `{0, 0, windowWidth, windowHeight}`.
     */
    [[nodiscard]] static UiScreenRect screenOf(engine::Engine& engine);

    /** @brief Owned UI render system; valid between init() and shutdown(). */
    std::unique_ptr<UiRenderSystem> _uiRenderSystem;
    /** @brief Layout computed in update() for button hit-testing; capacity is kept between frames. */
    std::vector<UiLayoutNode> _layout;
};

}  // namespace ui
