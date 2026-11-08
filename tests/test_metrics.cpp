#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "components/Renderable.hpp"
#include "core/Engine.hpp"
#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "core/services/IScreenDrawService.hpp"
#include "core/services/IWindowService.hpp"
#include "ecs/Entity.hpp"
#include "hierarchy/HierarchyPlugin.hpp"
#include "metrics/MetricsBoard.hpp"
#include "metrics/MetricsPlugin.hpp"
#include "metrics/ProcessUsage.hpp"
#include "metrics/RollingStats.hpp"
#include "metrics/overlay/MetricsOverlayPlugin.hpp"
#include "ui/UiPlugin.hpp"
#include "ui/components/UiProgressBar.hpp"
#include "ui/components/UiRect.hpp"
#include "ui/components/UiText.hpp"

namespace {

using metrics::MetricLevel;
using metrics::MetricsBoard;
using metrics::RollingStats;

// A power of two: exact in binary, so that a refresh period is exactly 32 frames.
constexpr double DT_64_FPS = 1.0 / 64.0;
constexpr double DT_32_FPS = 1.0 / 32.0;
constexpr double DT_16_FPS = 1.0 / 16.0;
constexpr int WINDOW_HEIGHT = 720;

// --- Headless engine -------------------------------------------------------------------------------------------------

/** @brief Engine with the metrics plugin, on a fake clock. */
struct HeadlessSide {
    HeadlessSide() {
        engine.addPlugin<metrics::MetricsPlugin>();
        engine.init();
    }

    /** @brief Runs frames of a given duration for at least `seconds` seconds. */
    void run(const double seconds, const double frame) {
        for (double elapsed = 0.0; elapsed < seconds; elapsed += frame) {
            now += frame;
            engine.advance();
        }
    }

    [[nodiscard]] const MetricsBoard& board() { return engine.registry().getResource<MetricsBoard>(); }

    double now = 0.0;
    engine::Engine engine{[this]() -> double { return now; }};
};

// --- Engine with the HUD, on fake services ---------------------------------------------------------------------------

struct FakeWindow final : engine::IWindowService {
    [[nodiscard]] bool shouldClose() const noexcept override { return false; }
    void beginFrame() override {}
    void endFrame() override {}
    [[nodiscard]] int width() const noexcept override { return 1280; }
    [[nodiscard]] int height() const noexcept override { return WINDOW_HEIGHT; }
    [[nodiscard]] bool isKeyDown(engine::Key /*key*/) const noexcept override { return false; }
    [[nodiscard]] bool isGamepadAvailable(int /*gamepad*/) const noexcept override { return false; }
    [[nodiscard]] bool isGamepadButtonDown(int /*gamepad*/, engine::GamepadButton /*button*/) const noexcept override {
        return false;
    }
    [[nodiscard]] float gamepadAxis(int /*gamepad*/, engine::GamepadAxis /*axis*/) const noexcept override {
        return 0.0F;
    }
    [[nodiscard]] bool isMouseButtonDown(engine::MouseButton /*button*/) const noexcept override { return false; }
    [[nodiscard]] bool isMouseButtonPressed(engine::MouseButton /*button*/) const noexcept override { return false; }
    [[nodiscard]] bool isMouseButtonReleased(engine::MouseButton /*button*/) const noexcept override { return false; }
    [[nodiscard]] engine::MousePosition mousePosition() const noexcept override { return {.x = 0.0F, .y = 0.0F}; }
    [[nodiscard]] float mouseWheelDelta() const noexcept override { return 0.0F; }
    [[nodiscard]] char32_t nextTypedCharacter() noexcept override { return 0; }
};

/** @brief Records what is drawn, to check the overlay reaches the screen. */
struct FakeScreenDraw final : engine::IScreenDrawService {
    engine::FontId loadFont(const std::string& /*path*/) override { return engine::NO_FONT; }
    [[nodiscard]] engine::TextSize measureText(std::string_view /*text*/, float /*fontSize*/, float /*spacing*/,
                                               engine::FontId /*font*/) const override {
        return {.width = 50.0F, .height = 10.0F};
    }
    void fillRect(const engine::ScreenRect& rect, const ecs::Rgba& /*color*/) override { rects.push_back(rect); }
    void strokeRect(const engine::ScreenRect& /*rect*/, float /*thickness*/, const ecs::Rgba& /*color*/) override {}
    void drawText(std::string_view text, float /*x*/, float /*y*/, float /*fontSize*/, float /*spacing*/,
                  const ecs::Rgba& /*color*/, engine::FontId /*font*/) override {
        texts.emplace_back(text);
    }
    void drawTexture(ecs::TextureId /*texture*/, const ecs::TextureRect& /*source*/, const engine::ScreenRect& /*dest*/,
                     float /*rotation*/, const ecs::Rgba& /*tint*/) override {}

    std::vector<engine::ScreenRect> rects;
    std::vector<std::string> texts;
};

/** @brief Registers the fake services, as `render::RenderPlugin` does the real ones. */
class FakeServicesPlugin final : public engine::IPlugin {
  public:
    explicit FakeServicesPlugin(std::shared_ptr<FakeScreenDraw> draw) : _draw(std::move(draw)) {}

    [[nodiscard]] std::string_view name() const noexcept override { return "FakeServicesPlugin"; }
    void build(engine::PluginBuilder& builder) override {
        builder.provides<engine::IWindowService>();
        builder.provides<engine::IScreenDrawService>();
    }
    void init(engine::Engine& engine) override {
        if (!engine.mediator().registerService<engine::IWindowService>(std::make_shared<FakeWindow>()) ||
            !engine.mediator().registerService<engine::IScreenDrawService>(_draw)) {
            throw std::runtime_error("cannot register the fake services");
        }
    }

  private:
    std::shared_ptr<FakeScreenDraw> _draw;
};

struct OverlaySide {
    /** @param visible What `setVisible` receives before the engine starts. */
    explicit OverlaySide(const bool visible = true) {
        engine.addPlugin<FakeServicesPlugin>(draw);
        engine.addPlugin<hierarchy::HierarchyPlugin>();
        engine.addPlugin<ui::UiPlugin>();
        engine.addPlugin<metrics::MetricsPlugin>();
        engine.addPlugin<metrics::MetricsOverlayPlugin>();
        overlay().setVisible(visible);
        engine.init();
    }

    /** @brief Lets one refresh period elapse, in 64 FPS frames. */
    void refresh() {
        for (int frame = 0; frame < 32; ++frame) {
            now += DT_64_FPS;
            engine.advance();
        }
    }

    [[nodiscard]] MetricsBoard& board() { return engine.registry().getResource<MetricsBoard>(); }
    [[nodiscard]] metrics::MetricsOverlayPlugin& overlay() { return engine.getPlugin<metrics::MetricsOverlayPlugin>(); }
    [[nodiscard]] const ui::UiText& line(const std::size_t index) {
        return engine.registry().getComponents<ui::UiText>().get(overlay().lines().at(index));
    }
    /** @brief Index of the line of a metric of the board. */
    [[nodiscard]] std::size_t indexOf(const std::string_view name) {
        const auto& entries = board().entries();
        const auto entry = std::ranges::find(entries, name, &metrics::Metric::name);
        return static_cast<std::size_t>(entry - entries.begin());
    }

    double now = 0.0;
    std::shared_ptr<FakeScreenDraw> draw = std::make_shared<FakeScreenDraw>();
    engine::Engine engine{[this]() -> double { return now; }};
};

}  // namespace

// --- RollingStats ----------------------------------------------------------------------------------------------------

TEST(RollingStats, EmptyGivesZero) {
    const RollingStats stats;

    EXPECT_TRUE(stats.empty());
    EXPECT_EQ(stats.average(), 0.0F);
    EXPECT_EQ(stats.min(), 0.0F);
    EXPECT_EQ(stats.max(), 0.0F);
}

TEST(RollingStats, ComputesAverageMinAndMax) {
    RollingStats stats;
    stats.push(1.0F);
    stats.push(2.0F);
    stats.push(6.0F);

    EXPECT_FALSE(stats.empty());
    EXPECT_FLOAT_EQ(stats.average(), 3.0F);
    EXPECT_FLOAT_EQ(stats.min(), 1.0F);
    EXPECT_FLOAT_EQ(stats.max(), 6.0F);
}

TEST(RollingStats, OldSamplesAreForgotten) {
    RollingStats stats;
    stats.push(100.0F);
    for (std::size_t i = 0; i < RollingStats::CAPACITY; ++i) {
        stats.push(1.0F);
    }

    EXPECT_FLOAT_EQ(stats.max(), 1.0F);
    EXPECT_FLOAT_EQ(stats.average(), 1.0F);
}

// --- MetricsBoard ----------------------------------------------------------------------------------------------------

TEST(MetricsBoard, LinesKeepTheOrderTheyWereFirstSetIn) {
    MetricsBoard board;
    board.set("b", "1");
    board.set("a", "2");
    board.set("b", "3", MetricLevel::Critical);

    ASSERT_EQ(board.entries().size(), 2U);
    EXPECT_EQ(board.entries().at(0).name, "b");
    EXPECT_EQ(board.entries().at(0).value, "3");
    EXPECT_EQ(board.entries().at(0).level, MetricLevel::Critical);
    EXPECT_EQ(board.entries().at(1).name, "a");
}

TEST(MetricsBoard, VersionOnlyChangesWithTheContent) {
    MetricsBoard board;
    board.set("a", "1");
    const auto version = board.version();

    board.set("a", "1");
    EXPECT_EQ(board.version(), version);

    board.set("a", "2");
    EXPECT_NE(board.version(), version);

    const auto afterValue = board.version();
    board.set("a", "2", MetricLevel::Warning);
    EXPECT_NE(board.version(), afterValue);
}

TEST(MetricsBoard, FindGivesNothingForAnUnknownName) {
    MetricsBoard board;
    board.set("a", "1");

    EXPECT_EQ(board.find("b"), nullptr);
    ASSERT_NE(board.find("a"), nullptr);
    EXPECT_EQ(board.find("a")->value, "1");
}

TEST(RefreshTimer, FiresOncePerPeriodWithTheTimeElapsed) {
    metrics::RefreshTimer timer;

    EXPECT_FALSE(timer.tick(metrics::REFRESH_PERIOD / 2.0F));
    const auto elapsed = timer.tick(metrics::REFRESH_PERIOD);
    ASSERT_TRUE(elapsed.has_value());
    EXPECT_FLOAT_EQ(*elapsed, metrics::REFRESH_PERIOD * 1.5F);
    EXPECT_FALSE(timer.tick(0.0F));
}

// --- Process ---------------------------------------------------------------------------------------------------------

// macOS has no /proc, so sampleProcess() has nothing to read there and returns no sample.
#ifndef __APPLE__
TEST(ProcessUsage, ReportsThisProcess) {
    const auto sample = metrics::sampleProcess();

    ASSERT_TRUE(sample.has_value());
    EXPECT_GT(sample->residentBytes, 0U);
    EXPECT_GE(sample->cpuSeconds, 0.0);
}
#endif

// --- MetricsPlugin ---------------------------------------------------------------------------------------------------

TEST(FrameLevel, FollowsTheFrameRate) {
    EXPECT_EQ(metrics::frameLevel(1.0F / 60.0F), MetricLevel::Normal);
    EXPECT_EQ(metrics::frameLevel(1.0F / 59.0F), MetricLevel::Normal);  // a game capped at 60 FPS is fine
    EXPECT_EQ(metrics::frameLevel(1.0F / 40.0F), MetricLevel::Warning);
    EXPECT_EQ(metrics::frameLevel(1.0F / 20.0F), MetricLevel::Critical);
}

TEST(MetricsPlugin, BoardStaysEmptyUntilTheFirstRefresh) {
    HeadlessSide side;
    side.run(static_cast<double>(metrics::REFRESH_PERIOD) / 2.0, DT_64_FPS);

    EXPECT_TRUE(side.board().entries().empty());
}

TEST(MetricsPlugin, PublishesFrameRateAndEntities) {
    HeadlessSide side;
    side.engine.registry().spawnEntity();
    side.engine.registry().spawnEntity();
    side.run(static_cast<double>(metrics::REFRESH_PERIOD), DT_64_FPS);

    ASSERT_NE(side.board().find("FPS"), nullptr);
    EXPECT_EQ(side.board().find("FPS")->value, "64");
    EXPECT_EQ(side.board().find("FPS")->level, MetricLevel::Normal);
    EXPECT_EQ(side.board().find("Frame")->value, "15.6 ms avg, 15.6 ms max");
    EXPECT_EQ(side.board().find("Entities")->value, "2");
#ifndef __APPLE__  // no process sample on macOS, so no CPU and RAM lines
    EXPECT_NE(side.board().find("CPU"), nullptr);
    EXPECT_NE(side.board().find("RAM"), nullptr);
#endif
}

TEST(MetricsPlugin, SlowFramesRaiseTheLevel) {
    HeadlessSide warning;
    warning.run(static_cast<double>(metrics::REFRESH_PERIOD), DT_32_FPS);
    EXPECT_EQ(warning.board().find("FPS")->level, MetricLevel::Warning);

    HeadlessSide critical;
    critical.run(static_cast<double>(metrics::REFRESH_PERIOD), DT_16_FPS);
    EXPECT_EQ(critical.board().find("FPS")->level, MetricLevel::Critical);
}

TEST(MetricsPlugin, ShutdownRemovesTheBoardItCreated) {
    HeadlessSide side;
    side.run(static_cast<double>(metrics::REFRESH_PERIOD), DT_64_FPS);

    side.engine.shutdown();

    EXPECT_FALSE(side.engine.registry().hasResource<MetricsBoard>());
}

TEST(MetricsPlugin, ShutdownKeepsABoardItDidNotCreate) {
    engine::Engine engine;
    engine.registry().emplaceResource<MetricsBoard>().set("Bosses", "2");
    engine.addPlugin<metrics::MetricsPlugin>();
    engine.init();

    engine.shutdown();

    ASSERT_TRUE(engine.registry().hasResource<MetricsBoard>());
    EXPECT_NE(engine.registry().getResource<MetricsBoard>().find("Bosses"), nullptr);
}

// --- MetricsOverlayPlugin --------------------------------------------------------------------------------------------

TEST(MetricsOverlay, ShowsOneLinePerMetric) {
    OverlaySide side;
    side.refresh();

    const auto& entries = side.board().entries();
    ASSERT_FALSE(entries.empty());
    ASSERT_EQ(side.overlay().lines().size(), entries.size());
    EXPECT_EQ(side.line(side.indexOf("FPS")).text, "FPS: 64");
}

TEST(MetricsOverlay, MetricOfTheGameGetsItsLineAtTheEnd) {
    OverlaySide side;
    side.refresh();
    const float height = side.engine.registry().getComponents<ui::UiRect>().get(side.overlay().panel()).height;

    side.board().set("Position", "12, 34");
    side.engine.advance();

    EXPECT_EQ(side.line(side.overlay().lines().size() - 1).text, "Position: 12, 34");
    EXPECT_GT(side.engine.registry().getComponents<ui::UiRect>().get(side.overlay().panel()).height, height);
}

TEST(MetricsOverlay, LevelChangesTheColor) {
    OverlaySide side;
    side.refresh();
    side.board().set("Lag", "ok");
    side.engine.advance();
    const std::size_t lag = side.indexOf("Lag");
    const ecs::Rgba normal = side.line(lag).color;

    side.board().set("Lag", "bad", MetricLevel::Critical);
    side.engine.advance();

    EXPECT_EQ(side.line(lag).text, "Lag: bad");
    EXPECT_NE(side.line(lag).color.g, normal.g);
}

TEST(MetricsOverlay, TextIsLeftAloneWhileTheBoardDoesNotChange) {
    OverlaySide side;
    side.refresh();
    const std::size_t fps = side.indexOf("FPS");
    side.engine.registry().getComponents<ui::UiText>().get(side.overlay().lines().at(fps)).text = "untouched";

    side.engine.advance();

    EXPECT_EQ(side.line(fps).text, "untouched");
}

TEST(MetricsOverlay, BarShowsTheAverageFrameTime) {
    OverlaySide side;
    side.refresh();

    const auto& bar = side.engine.registry().getComponents<ui::UiProgressBar>().get(side.overlay().frameBar());
    EXPECT_NEAR(bar.value, 15.625F, 0.01F);
}

TEST(MetricsOverlay, IsDrawnInTheBottomLeftCorner) {
    OverlaySide side;
    side.refresh();
    side.draw->rects.clear();
    side.draw->texts.clear();

    side.engine.advance();

    EXPECT_NE(std::ranges::find(side.draw->texts, "FPS: 64"), side.draw->texts.end());
    ASSERT_FALSE(side.draw->rects.empty());
    const engine::ScreenRect& panel = side.draw->rects.front();
    EXPECT_GT(panel.x, 0.0F);
    EXPECT_LT(panel.y + panel.height, static_cast<float>(WINDOW_HEIGHT));
    EXPECT_GT(panel.y + panel.height, static_cast<float>(WINDOW_HEIGHT) / 2.0F);
}

TEST(MetricsOverlay, IsVisibleByDefault) {
    OverlaySide side;

    EXPECT_TRUE(side.overlay().visible());
}

TEST(MetricsOverlay, HiddenOverlayIsNotDrawn) {
    OverlaySide side;
    side.refresh();
    const ecs::Entity panel = side.overlay().panel();
    const ecs::Entity firstLine = side.overlay().lines().front();

    side.overlay().setVisible(false);
    side.draw->rects.clear();
    side.draw->texts.clear();
    side.engine.advance();

    EXPECT_FALSE(side.overlay().visible());
    EXPECT_EQ(side.overlay().panel(), ecs::NULL_ENTITY);
    EXPECT_TRUE(side.overlay().lines().empty());
    EXPECT_FALSE(side.engine.registry().isAlive(panel));
    EXPECT_FALSE(side.engine.registry().isAlive(firstLine)) << "the lines go with the panel";
    EXPECT_TRUE(side.draw->rects.empty());
    EXPECT_TRUE(side.draw->texts.empty());
}

TEST(MetricsOverlay, ShownAgainWithTheCurrentBoard) {
    OverlaySide side;
    side.refresh();
    side.overlay().setVisible(false);
    side.engine.advance();
    side.board().set("Position", "12, 34");

    side.overlay().setVisible(true);
    side.engine.advance();

    ASSERT_NE(side.overlay().panel(), ecs::NULL_ENTITY);
    ASSERT_EQ(side.overlay().lines().size(), side.board().entries().size());
    EXPECT_EQ(side.line(side.indexOf("Position")).text, "Position: 12, 34");
    EXPECT_EQ(side.line(side.indexOf("FPS")).text, "FPS: 64");
}

TEST(MetricsOverlay, HiddenBeforeInitSpawnsNothing) {
    OverlaySide side(false);
    side.refresh();

    EXPECT_EQ(side.overlay().panel(), ecs::NULL_ENTITY);
    EXPECT_TRUE(side.draw->texts.empty());
}

TEST(MetricsOverlay, ShutdownDestroysThePanel) {
    OverlaySide side;
    side.refresh();
    const ecs::Entity panel = side.overlay().panel();
    const ecs::Entity firstLine = side.overlay().lines().front();

    side.engine.shutdown();

    EXPECT_FALSE(side.engine.registry().isAlive(panel));
    EXPECT_FALSE(side.engine.registry().isAlive(firstLine));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
