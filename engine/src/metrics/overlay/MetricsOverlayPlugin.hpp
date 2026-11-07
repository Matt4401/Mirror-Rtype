/**
 * @file MetricsOverlayPlugin.hpp
 * @brief Plugin showing the metrics board on a HUD panel.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "metrics/RollingStats.hpp"

namespace ecs::time {
struct Time;
}

namespace metrics {

/**
 * @brief Shows the `MetricsBoard` in the bottom-left corner of the screen, with `ui` components.
 *
 * A `ui::UiRect` panel holds a `ui::UiProgressBar` for the frame time (full at 30 FPS) and one `ui::UiText` per metric,
 * coloured by its `MetricLevel`. The text only changes when the board does: a frame without a refresh allocates
 * nothing.
 *
 * It is always shown for now: hiding it is left to the game settings.
 */
class MetricsOverlayPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Gets plugin name.
     * @return String view "MetricsOverlayPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "MetricsOverlayPlugin"; }

    /**
     * @brief Declares the dependencies on the board, the UI and the hierarchy holding the panel together.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Spawns the panel and the frame time bar.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Copies the board to the panel if it changed.
     * @param engine Host engine.
     * @param time Frame time (unused).
     */
    void update(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Gets the panel.
     * @return The entity of the panel, once initialized.
     */
    [[nodiscard]] ecs::Entity panel() const noexcept { return _panel; }

    /**
     * @brief Gets the frame time bar.
     * @return The entity of the bar, once initialized.
     */
    [[nodiscard]] ecs::Entity frameBar() const noexcept { return _frameBar; }

    /**
     * @brief Gets the text lines.
     * @return One entity per metric of the board, in the same order.
     */
    [[nodiscard]] const std::vector<ecs::Entity>& lines() const noexcept { return _lines; }

  private:
    /**
     * @brief Spawns the text lines missing for the board, and makes the panel tall enough for all of them.
     * @param registry Registry of the panel.
     * @param count Number of metrics on the board.
     */
    void growTo(ecs::Registry& registry, std::size_t count);
    /**
     * @brief Shows the average frame time on the bar.
     * @param registry Registry of the panel.
     * @param frames Last frame times, in seconds.
     */
    void showFrameTime(ecs::Registry& registry, const RollingStats& frames) const;

    /** @brief Background of the overlay, parent of everything else. */
    ecs::Entity _panel = ecs::NULL_ENTITY;
    /** @brief Frame time bar. */
    ecs::Entity _frameBar = ecs::NULL_ENTITY;
    /** @brief One text per metric of the board. */
    std::vector<ecs::Entity> _lines;
    /** @brief Version of the board currently shown. */
    std::uint64_t _shownVersion = 0;
};

}  // namespace metrics
