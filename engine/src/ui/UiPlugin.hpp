/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** Plugin wrapping FontStore and UiRenderSystem into the engine lifecycle.
*/

#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/time/Time.hpp"
#include "ui/font/FontStore.hpp"
#include "ui/system/UiLayout.hpp"
#include "ui/system/UiRenderSystem.hpp"

namespace ui {

/**
 * @brief Engine plugin driving HUD rendering and button interaction.
 *
 * Owns a FontStore, a UiRenderSystem, and a UiButtonSystem. Runs
 * UiButtonSystem::update() in the update phase and UiRenderSystem::update()
 * in the render phase, after the standard RenderPlugin pass.
 * Requires RenderPlugin and IWindowService.
 *
 * HUD elements can be nested with `hierarchy::setParent`: a child element is anchored inside its parent's
 * rectangle (see `computeLayout`). Add `hierarchy::HierarchyPlugin` so that destroying a panel destroys its widgets.
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
     * @brief Declares dependency on RenderPlugin and IWindowService.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Initializes FontStore and UiRenderSystem.
     * @param engine Reference to host Engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Runs UiButtonSystem to update button interaction states.
     * @param engine Reference to host Engine.
     * @param time   Current frame time (unused).
     */
    void update(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Runs the UI render pass after the main render pass.
     * @param engine Reference to host Engine.
     */
    void render(engine::Engine& engine) override;

    /**
     * @brief Gets mutable pointer to the FontStore, or nullptr if uninitialized.
     * @return Pointer to the owned FontStore.
     */
    [[nodiscard]] FontStore* fonts() noexcept { return _fonts.get(); }

  private:
    /** @brief Owned font store; valid after init(). */
    std::unique_ptr<FontStore> _fonts;
    /** @brief Owned UI render system; valid after init(). */
    std::unique_ptr<UiRenderSystem> _uiRenderSystem;
    /** @brief Layout computed in update() for button hit-testing; capacity is kept between frames. */
    std::vector<UiLayoutNode> _layout;
};

}  // namespace ui
