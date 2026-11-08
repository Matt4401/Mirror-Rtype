#include "metrics/overlay/MetricsOverlayPlugin.hpp"

#include <cstddef>
#include <format>

#include "components/Renderable.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "hierarchy/Hierarchy.hpp"
#include "hierarchy/HierarchyPlugin.hpp"
#include "metrics/MetricsBoard.hpp"
#include "metrics/MetricsPlugin.hpp"
#include "metrics/RollingStats.hpp"
#include "ui/UiPlugin.hpp"
#include "ui/components/UiAnchor.hpp"
#include "ui/components/UiProgressBar.hpp"
#include "ui/components/UiRect.hpp"
#include "ui/components/UiText.hpp"

namespace metrics {

namespace {

constexpr float PANEL_MARGIN = 10.0F;
constexpr float PANEL_PADDING = 8.0F;
constexpr float PANEL_WIDTH = 300.0F;
constexpr float BAR_WIDTH = PANEL_WIDTH - (2.0F * PANEL_PADDING);
constexpr float BAR_HEIGHT = 8.0F;
constexpr float BAR_GAP = 6.0F;
constexpr float LINE_HEIGHT = 18.0F;
constexpr float FONT_SIZE = 16.0F;
constexpr float LINES_TOP = PANEL_PADDING + BAR_HEIGHT + BAR_GAP;
constexpr float MILLISECONDS_PER_SECOND = 1000.0F;
/** @brief Frame time filling the bar: one frame at 30 FPS. */
constexpr float BAR_FULL_MS = 1000.0F / 30.0F;

constexpr ecs::Rgba COLOR_PANEL = {.r = 0, .g = 0, .b = 0, .a = 170};
constexpr ecs::Rgba COLOR_TEXT = {.r = 255, .g = 255, .b = 255, .a = 255};
constexpr ecs::Rgba COLOR_GOOD = {.r = 0, .g = 200, .b = 0, .a = 255};
constexpr ecs::Rgba COLOR_WARNING = {.r = 255, .g = 180, .b = 0, .a = 255};
constexpr ecs::Rgba COLOR_CRITICAL = {.r = 255, .g = 70, .b = 70, .a = 255};

/** @brief Gets the colour of a metric or of the bar for a level. */
ecs::Rgba colorOf(const MetricLevel level, const ecs::Rgba& normal) {
    switch (level) {
        case MetricLevel::Warning:
            return COLOR_WARNING;
        case MetricLevel::Critical:
            return COLOR_CRITICAL;
        case MetricLevel::Normal:
            break;
    }
    return normal;
}

/** @brief Computes the height of a panel holding `lineCount` lines. */
float panelHeight(const std::size_t lineCount) {
    return LINES_TOP + (static_cast<float>(lineCount) * LINE_HEIGHT) + PANEL_PADDING;
}

}  // namespace

void MetricsOverlayPlugin::build(engine::PluginBuilder& builder) {
    builder.require<MetricsPlugin>();
    builder.require<ui::UiPlugin>();
    builder.require<hierarchy::HierarchyPlugin>();
}

void MetricsOverlayPlugin::init(engine::Engine& engine) {
    if (_visible) {
        spawnPanel(engine.registry());
    }
}

void MetricsOverlayPlugin::shutdown(engine::Engine& engine) { destroyPanel(engine.registry()); }

void MetricsOverlayPlugin::spawnPanel(ecs::Registry& registry) {
    _panel = registry.spawnEntity();
    registry.addComponent<ui::UiAnchor>(
        _panel, ui::UiAnchor{.anchorY = 1.0F, .offsetX = PANEL_MARGIN, .offsetY = -PANEL_MARGIN, .pivotY = 1.0F});
    registry.addComponent<ui::UiRect>(
        _panel, ui::UiRect{.width = PANEL_WIDTH, .height = panelHeight(0), .fillColor = COLOR_PANEL});

    _frameBar = registry.spawnEntity();
    registry.addComponent<ui::UiAnchor>(_frameBar, ui::UiAnchor{.offsetX = PANEL_PADDING, .offsetY = PANEL_PADDING});
    registry.addComponent<ui::UiProgressBar>(
        _frameBar,
        ui::UiProgressBar{.max = BAR_FULL_MS, .width = BAR_WIDTH, .height = BAR_HEIGHT, .foreground = COLOR_GOOD});
    hierarchy::setParent(registry, _frameBar, _panel);
}

void MetricsOverlayPlugin::destroyPanel(ecs::Registry& registry) {
    if (_panel != ecs::NULL_ENTITY && registry.isAlive(_panel)) {
        registry.killEntity(_panel);  // the hierarchy destroys the bar and the lines with it
    }
    _panel = ecs::NULL_ENTITY;
    _frameBar = ecs::NULL_ENTITY;
    _lines.clear();
    _shownVersion = 0;  // a panel built again shows the board at once
}

void MetricsOverlayPlugin::growTo(ecs::Registry& registry, const std::size_t count) {
    if (_lines.size() >= count) {
        return;
    }
    while (_lines.size() < count) {
        const ecs::Entity line = registry.spawnEntity();
        const float top = LINES_TOP + (static_cast<float>(_lines.size()) * LINE_HEIGHT);
        registry.addComponent<ui::UiAnchor>(line, ui::UiAnchor{.offsetX = PANEL_PADDING, .offsetY = top});
        registry.addComponent<ui::UiText>(line, ui::UiText{.fontSize = FONT_SIZE, .color = COLOR_TEXT});
        hierarchy::setParent(registry, line, _panel);
        _lines.push_back(line);
    }
    registry.getComponents<ui::UiRect>().get(_panel).height = panelHeight(count);
}

void MetricsOverlayPlugin::showFrameTime(ecs::Registry& registry, const RollingStats& frames) const {
    auto& bar = registry.getComponents<ui::UiProgressBar>().get(_frameBar);
    bar.value = frames.average() * MILLISECONDS_PER_SECOND;
    bar.foreground = colorOf(frameLevel(frames.average()), COLOR_GOOD);
}

void MetricsOverlayPlugin::update(engine::Engine& engine, const ecs::time::Time& /*time*/) {
    auto& registry = engine.registry();
    if (!_visible) {
        destroyPanel(registry);
        return;
    }
    if (_panel == ecs::NULL_ENTITY) {
        spawnPanel(registry);
    }
    const auto& board = registry.getResource<MetricsBoard>();
    if (board.version() == _shownVersion) {
        return;
    }
    _shownVersion = board.version();

    growTo(registry, board.entries().size());
    auto& texts = registry.getComponents<ui::UiText>();
    for (std::size_t i = 0; i < board.entries().size(); ++i) {
        const Metric& metric = board.entries().at(i);
        auto& text = texts.get(_lines.at(i));
        text.text = std::format("{}: {}", metric.name, metric.value);
        text.color = colorOf(metric.level, COLOR_TEXT);
    }
    showFrameTime(registry, engine.getPlugin<MetricsPlugin>().frames());
}

}  // namespace metrics
