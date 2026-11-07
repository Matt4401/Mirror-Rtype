#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <variant>
#include <vector>

#include "core/services/IWindowService.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "events/UiEvents.hpp"
#include "hierarchy/Hierarchy.hpp"
#include "hierarchy/HierarchyHooks.hpp"
#include "ui/components/FontId.hpp"
#include "ui/components/UiAnchor.hpp"
#include "ui/components/UiButton.hpp"
#include "ui/components/UiProgressBar.hpp"
#include "ui/components/UiRect.hpp"
#include "ui/components/UiText.hpp"
#include "ui/system/UiButtonSystem.hpp"
#include "ui/system/UiDrawItem.hpp"
#include "ui/system/UiLayout.hpp"

namespace {

struct FakeWindow : engine::IWindowService {
    int w = 1920;
    int h = 1080;
    float mx = 0.0F;
    float my = 0.0F;
    bool leftDown = false;
    bool leftPressed = false;
    bool leftReleased = false;

    /** @brief Simulates the frame the left button goes down. */
    void press() {
        leftDown = true;
        leftPressed = true;
        leftReleased = false;
    }

    /** @brief Simulates the frame the left button goes up. */
    void release() {
        leftDown = false;
        leftPressed = false;
        leftReleased = true;
    }

    [[nodiscard]] bool shouldClose() const noexcept override { return false; }
    void beginFrame() override {}
    void endFrame() override {}
    [[nodiscard]] int width() const noexcept override { return w; }
    [[nodiscard]] int height() const noexcept override { return h; }
    [[nodiscard]] bool isKeyDown(engine::Key) const noexcept override { return false; }
    [[nodiscard]] bool isGamepadAvailable(int) const noexcept override { return false; }
    [[nodiscard]] bool isGamepadButtonDown(int, engine::GamepadButton) const noexcept override { return false; }
    [[nodiscard]] float gamepadAxis(int, engine::GamepadAxis) const noexcept override { return 0.0F; }
    [[nodiscard]] bool isMouseButtonDown(engine::MouseButton btn) const noexcept override {
        return btn == engine::MouseButton::Left && leftDown;
    }
    [[nodiscard]] bool isMouseButtonPressed(engine::MouseButton btn) const noexcept override {
        return btn == engine::MouseButton::Left && leftPressed;
    }
    [[nodiscard]] bool isMouseButtonReleased(engine::MouseButton btn) const noexcept override {
        return btn == engine::MouseButton::Left && leftReleased;
    }
    [[nodiscard]] engine::MousePosition mousePosition() const noexcept override { return {.x = mx, .y = my}; }
    [[nodiscard]] float mouseWheelDelta() const noexcept override { return 0.0F; }
};

static ecs::Entity makeButton(ecs::Registry& reg, float ancX, float ancY, float w = 120.0F, float h = 36.0F,
                              const std::string& action = "click") {
    const ecs::Entity e = reg.spawnEntity();
    reg.addComponent<ui::UiAnchor>(e, ui::UiAnchor{.anchorX = ancX, .anchorY = ancY});
    reg.addComponent<ui::UiButton>(e, ui::UiButton{.width = w, .height = h, .action = action});
    return e;
}

/** @brief Fixed text size, so layout tests do not need a window or a font. */
constexpr ui::UiSize FAKE_TEXT_SIZE = {.width = 50.0F, .height = 10.0F};

ui::UiSize fakeMeasure(const ui::UiText& /*text*/) { return FAKE_TEXT_SIZE; }

std::vector<ui::UiLayoutNode> layoutOf(ecs::Registry& reg, const FakeWindow& win) {
    std::vector<ui::UiLayoutNode> nodes;
    ui::computeLayout(reg, ui::UiScreenRect{.width = static_cast<float>(win.w), .height = static_cast<float>(win.h)},
                      fakeMeasure, nodes);
    return nodes;
}

void updateButtons(ecs::Registry& reg, const FakeWindow& win, ecs::EventBus& bus) {
    ui::UiButtonSystem::update(reg, layoutOf(reg, win), win, bus);
}

const ui::UiLayoutNode* findNode(const std::vector<ui::UiLayoutNode>& nodes, ecs::Entity e) {
    const auto it = std::ranges::find(nodes, e, &ui::UiLayoutNode::entity);
    return it == nodes.end() ? nullptr : &*it;
}

std::ptrdiff_t indexOf(const std::vector<ui::UiLayoutNode>& nodes, ecs::Entity e) {
    return std::ranges::find(nodes, e, &ui::UiLayoutNode::entity) - nodes.begin();
}

}  // namespace

TEST(FontId, NoFontSentinelIsZero) { EXPECT_EQ(ui::NO_FONT, 0U); }

TEST(UiAnchor, DefaultValues) {
    const ui::UiAnchor anchor;

    EXPECT_EQ(anchor.anchorX, 0.0F);
    EXPECT_EQ(anchor.anchorY, 0.0F);
    EXPECT_EQ(anchor.offsetX, 0.0F);
    EXPECT_EQ(anchor.offsetY, 0.0F);
}

TEST(UiAnchor, DesignatedInitializersKeepOtherDefaults) {
    const ui::UiAnchor anchor{.anchorX = 0.5F, .anchorY = 1.0F};

    EXPECT_EQ(anchor.anchorX, 0.5F);
    EXPECT_EQ(anchor.anchorY, 1.0F);
    EXPECT_EQ(anchor.offsetX, 0.0F);
    EXPECT_EQ(anchor.offsetY, 0.0F);
}

TEST(UiAnchor, WorksAsEcsComponent) {
    ecs::Registry registry;
    const ecs::Entity entity = registry.spawnEntity();

    registry.addComponent<ui::UiAnchor>(entity, ui::UiAnchor{.anchorX = 0.5F, .offsetY = -8.0F});

    ASSERT_TRUE(registry.hasComponent<ui::UiAnchor>(entity));
    EXPECT_EQ(registry.getComponents<ui::UiAnchor>().get(entity).anchorX, 0.5F);
    EXPECT_EQ(registry.getComponents<ui::UiAnchor>().get(entity).offsetY, -8.0F);
}

static std::pair<float, float> resolveAnchor(const ui::UiAnchor& anchor, float screenW, float screenH,
                                             float elemW = 0.0F, float elemH = 0.0F) {
    const ui::UiScreenRect rect = ui::resolveAnchor(anchor, ui::UiScreenRect{.width = screenW, .height = screenH},
                                                    ui::UiSize{.width = elemW, .height = elemH});
    return {rect.x, rect.y};
}

TEST(UiAnchorResolution, TopLeftCorner) {
    const ui::UiAnchor anchor{.anchorX = 0.0F, .anchorY = 0.0F};
    auto [x, y] = resolveAnchor(anchor, 1920.0F, 1080.0F);

    EXPECT_FLOAT_EQ(x, 0.0F);
    EXPECT_FLOAT_EQ(y, 0.0F);
}

TEST(UiAnchorResolution, BottomRightCornerNoPivot) {
    const ui::UiAnchor anchor{.anchorX = 1.0F, .anchorY = 1.0F};
    auto [x, y] = resolveAnchor(anchor, 1920.0F, 1080.0F);

    EXPECT_FLOAT_EQ(x, 1920.0F);
    EXPECT_FLOAT_EQ(y, 1080.0F);
}

TEST(UiAnchorResolution, BottomRightCornerWithPivot) {
    const ui::UiAnchor anchor{.anchorX = 1.0F, .anchorY = 1.0F, .pivotX = 1.0F, .pivotY = 1.0F};
    auto [x, y] = resolveAnchor(anchor, 1920.0F, 1080.0F, 200.0F, 40.0F);

    EXPECT_FLOAT_EQ(x, 1920.0F - 200.0F);
    EXPECT_FLOAT_EQ(y, 1080.0F - 40.0F);
}

TEST(UiAnchorResolution, CenterWithOffset) {
    const ui::UiAnchor anchor{.anchorX = 0.5F, .anchorY = 0.5F, .offsetX = -50.0F, .offsetY = 10.0F};
    auto [x, y] = resolveAnchor(anchor, 1920.0F, 1080.0F);

    EXPECT_FLOAT_EQ(x, 910.0F);
    EXPECT_FLOAT_EQ(y, 550.0F);
}

TEST(UiAnchorResolution, CenterPivotCentersElement) {
    const ui::UiAnchor anchor{.anchorX = 0.5F, .anchorY = 0.5F, .pivotX = 0.5F, .pivotY = 0.5F};
    auto [x, y] = resolveAnchor(anchor, 1920.0F, 1080.0F, 200.0F, 40.0F);

    EXPECT_FLOAT_EQ(x, 960.0F - 100.0F);
    EXPECT_FLOAT_EQ(y, 540.0F - 20.0F);
}

TEST(UiAnchor, DefaultPivotIsZero) {
    const ui::UiAnchor anchor;

    EXPECT_EQ(anchor.pivotX, 0.0F);
    EXPECT_EQ(anchor.pivotY, 0.0F);
}

TEST(UiRect, DefaultValues) {
    const ui::UiRect rect;

    EXPECT_EQ(rect.width, 0.0F);
    EXPECT_EQ(rect.height, 0.0F);
    EXPECT_EQ(rect.borderThickness, 0.0F);
    EXPECT_EQ(rect.borderColor.a, 0);
}

TEST(UiRect, DesignatedInitializersKeepOtherDefaults) {
    const ui::UiRect rect{
        .width = 200.0F,
        .height = 40.0F,
        .fillColor = {.r = 255, .g = 0, .b = 0, .a = 255},
    };

    EXPECT_EQ(rect.width, 200.0F);
    EXPECT_EQ(rect.height, 40.0F);
    EXPECT_EQ(rect.fillColor.r, 255);
    EXPECT_EQ(rect.borderThickness, 0.0F);
}

TEST(UiText, DefaultValues) {
    const ui::UiText text;

    EXPECT_TRUE(text.text.empty());
    EXPECT_EQ(text.fontSize, 20.0F);
    EXPECT_EQ(text.fontId, ui::NO_FONT);
}

TEST(UiText, DesignatedInitializersKeepOtherDefaults) {
    const ui::UiText text{.text = "score: 0", .fontSize = 32.0F};

    EXPECT_EQ(text.text, "score: 0");
    EXPECT_EQ(text.fontSize, 32.0F);
    EXPECT_EQ(text.fontId, ui::NO_FONT);
}

TEST(UiProgressBar, DefaultValues) {
    const ui::UiProgressBar bar;

    EXPECT_EQ(bar.value, 0.0F);
    EXPECT_EQ(bar.min, 0.0F);
    EXPECT_EQ(bar.max, 1.0F);
    EXPECT_EQ(bar.width, 100.0F);
    EXPECT_EQ(bar.height, 16.0F);
}

TEST(UiProgressBar, DesignatedInitializersKeepOtherDefaults) {
    const ui::UiProgressBar bar{.value = 50.0F, .min = 0.0F, .max = 100.0F};

    EXPECT_EQ(bar.value, 50.0F);
    EXPECT_EQ(bar.max, 100.0F);
    EXPECT_EQ(bar.width, 100.0F);
}

static float computeRatio(const ui::UiProgressBar& bar) {
    const float range = bar.max - bar.min;
    if (range <= 0.0F) {
        return 0.0F;
    }
    return std::clamp((bar.value - bar.min) / range, 0.0F, 1.0F);
}

TEST(UiProgressBarRatio, MidRange) {
    const ui::UiProgressBar bar{.value = 50.0F, .min = 0.0F, .max = 100.0F};
    EXPECT_FLOAT_EQ(computeRatio(bar), 0.5F);
}

TEST(UiProgressBarRatio, ClampsBelowMin) {
    const ui::UiProgressBar bar{.value = -10.0F, .min = 0.0F, .max = 100.0F};
    EXPECT_FLOAT_EQ(computeRatio(bar), 0.0F);
}

TEST(UiProgressBarRatio, ClampsAboveMax) {
    const ui::UiProgressBar bar{.value = 150.0F, .min = 0.0F, .max = 100.0F};
    EXPECT_FLOAT_EQ(computeRatio(bar), 1.0F);
}

TEST(UiProgressBarRatio, ZeroRangeReturnsZero) {
    const ui::UiProgressBar bar{.value = 5.0F, .min = 5.0F, .max = 5.0F};
    EXPECT_FLOAT_EQ(computeRatio(bar), 0.0F);
}

TEST(UiDrawItem, HoldsUiRect) {
    const ui::UiDrawItem item{
        .entity = 1,
        .screenX = 100.0F,
        .screenY = 200.0F,
        .drawable = ui::UiRect{.width = 50.0F, .height = 20.0F},
    };

    ASSERT_TRUE(std::holds_alternative<ui::UiRect>(item.drawable));
    EXPECT_EQ(std::get<ui::UiRect>(item.drawable).width, 50.0F);
}

TEST(UiDrawItem, HoldsUiText) {
    const ui::UiDrawItem item{
        .entity = 2,
        .screenX = 0.0F,
        .screenY = 0.0F,
        .drawable = ui::UiText{.text = "HP"},
    };

    ASSERT_TRUE(std::holds_alternative<ui::UiText>(item.drawable));
    EXPECT_EQ(std::get<ui::UiText>(item.drawable).text, "HP");
}

TEST(UiDrawItem, HoldsUiProgressBar) {
    const ui::UiDrawItem item{
        .entity = 3,
        .screenX = 0.0F,
        .screenY = 0.0F,
        .drawable = ui::UiProgressBar{.value = 75.0F, .max = 100.0F},
    };

    ASSERT_TRUE(std::holds_alternative<ui::UiProgressBar>(item.drawable));
    EXPECT_EQ(std::get<ui::UiProgressBar>(item.drawable).value, 75.0F);
}

TEST(UiButton, DefaultValues) {
    const ui::UiButton btn;

    EXPECT_EQ(btn.state, ui::UiButtonState::Idle);
    EXPECT_EQ(btn.width, 120.0F);
    EXPECT_EQ(btn.height, 36.0F);
    EXPECT_TRUE(btn.label.empty());
    EXPECT_TRUE(btn.action.empty());
}

TEST(UiButton, WorksAsEcsComponent) {
    ecs::Registry registry;
    const ecs::Entity entity = registry.spawnEntity();

    registry.addComponent<ui::UiButton>(entity, ui::UiButton{.action = "fire"});

    ASSERT_TRUE(registry.hasComponent<ui::UiButton>(entity));
    EXPECT_EQ(registry.getComponents<ui::UiButton>().get(entity).action, "fire");
}

TEST(UiButtonSystem, IdleToHoveredWhenMouseEnters) {
    ecs::Registry reg;
    ecs::EventBus bus;
    FakeWindow win;
    const ecs::Entity e = makeButton(reg, 0.0F, 0.0F);

    win.mx = 60.0F;
    win.my = 18.0F;
    updateButtons(reg, win, bus);

    EXPECT_EQ(reg.getComponents<ui::UiButton>().get(e).state, ui::UiButtonState::Hovered);
}

TEST(UiButtonSystem, HoveredToIdleWhenMouseLeaves) {
    ecs::Registry reg;
    ecs::EventBus bus;
    FakeWindow win;
    const ecs::Entity e = makeButton(reg, 0.0F, 0.0F);
    reg.getComponents<ui::UiButton>().get(e).state = ui::UiButtonState::Hovered;

    win.mx = 999.0F;
    win.my = 999.0F;
    updateButtons(reg, win, bus);

    EXPECT_EQ(reg.getComponents<ui::UiButton>().get(e).state, ui::UiButtonState::Idle);
}

TEST(UiButtonSystem, HoveredToPressedOnMouseDown) {
    ecs::Registry reg;
    ecs::EventBus bus;
    FakeWindow win;
    const ecs::Entity e = makeButton(reg, 0.0F, 0.0F);
    reg.getComponents<ui::UiButton>().get(e).state = ui::UiButtonState::Hovered;

    win.mx = 60.0F;
    win.my = 18.0F;
    win.press();
    updateButtons(reg, win, bus);

    EXPECT_EQ(reg.getComponents<ui::UiButton>().get(e).state, ui::UiButtonState::Pressed);
}

TEST(UiButtonSystem, IdleToPressedWhenEnteringAndPressingSameFrame) {
    ecs::Registry reg;
    ecs::EventBus bus;
    FakeWindow win;
    const ecs::Entity e = makeButton(reg, 0.0F, 0.0F);

    win.mx = 60.0F;
    win.my = 18.0F;
    win.press();
    updateButtons(reg, win, bus);

    EXPECT_EQ(reg.getComponents<ui::UiButton>().get(e).state, ui::UiButtonState::Pressed);
}

TEST(UiButtonSystem, HeldMouseDraggedOntoButtonDoesNotPress) {
    ecs::Registry reg;
    ecs::EventBus bus;
    FakeWindow win;
    const ecs::Entity e = makeButton(reg, 0.0F, 0.0F);
    reg.getComponents<ui::UiButton>().get(e).state = ui::UiButtonState::Hovered;

    win.mx = 60.0F;
    win.my = 18.0F;
    win.leftDown = true;
    updateButtons(reg, win, bus);

    EXPECT_EQ(reg.getComponents<ui::UiButton>().get(e).state, ui::UiButtonState::Hovered);
}

TEST(UiButtonSystem, PressedToReleasedAndEventOnReleaseOverButton) {
    ecs::Registry reg;
    ecs::EventBus bus;
    FakeWindow win;
    const ecs::Entity e = makeButton(reg, 0.0F, 0.0F, 120.0F, 36.0F, "shoot");
    reg.getComponents<ui::UiButton>().get(e).state = ui::UiButtonState::Pressed;

    std::string capturedAction;
    bus.subscribe<ui::UiButtonClickedEvent>([&](const ui::UiButtonClickedEvent& ev) { capturedAction = ev.action; });

    win.mx = 60.0F;
    win.my = 18.0F;
    win.release();
    updateButtons(reg, win, bus);

    EXPECT_EQ(reg.getComponents<ui::UiButton>().get(e).state, ui::UiButtonState::Released);
    EXPECT_EQ(capturedAction, "shoot");
}

TEST(UiButtonSystem, ReleasedLastsOneFrame) {
    ecs::Registry reg;
    ecs::EventBus bus;
    FakeWindow win;
    const ecs::Entity e = makeButton(reg, 0.0F, 0.0F);
    reg.getComponents<ui::UiButton>().get(e).state = ui::UiButtonState::Released;

    int clicks = 0;
    bus.subscribe<ui::UiButtonClickedEvent>([&](const ui::UiButtonClickedEvent&) { ++clicks; });

    win.mx = 60.0F;
    win.my = 18.0F;
    updateButtons(reg, win, bus);

    EXPECT_EQ(reg.getComponents<ui::UiButton>().get(e).state, ui::UiButtonState::Hovered);
    EXPECT_EQ(clicks, 0);
}

TEST(UiButtonSystem, ReleaseOutsideCancelsClick) {
    ecs::Registry reg;
    ecs::EventBus bus;
    FakeWindow win;
    const ecs::Entity e = makeButton(reg, 0.0F, 0.0F, 120.0F, 36.0F, "quit");
    reg.getComponents<ui::UiButton>().get(e).state = ui::UiButtonState::Pressed;

    bool fired = false;
    bus.subscribe<ui::UiButtonClickedEvent>([&](const ui::UiButtonClickedEvent&) { fired = true; });

    win.mx = 999.0F;
    win.my = 999.0F;
    win.release();
    updateButtons(reg, win, bus);

    EXPECT_EQ(reg.getComponents<ui::UiButton>().get(e).state, ui::UiButtonState::Idle);
    EXPECT_FALSE(fired);
}

TEST(UiButtonSystem, NoEventWhenStillPressed) {
    ecs::Registry reg;
    ecs::EventBus bus;
    FakeWindow win;
    const ecs::Entity e = makeButton(reg, 0.0F, 0.0F);
    reg.getComponents<ui::UiButton>().get(e).state = ui::UiButtonState::Pressed;

    bool fired = false;
    bus.subscribe<ui::UiButtonClickedEvent>([&](const ui::UiButtonClickedEvent&) { fired = true; });

    win.mx = 60.0F;
    win.my = 18.0F;
    win.leftDown = true;
    updateButtons(reg, win, bus);

    EXPECT_FALSE(fired);
    EXPECT_EQ(reg.getComponents<ui::UiButton>().get(e).state, ui::UiButtonState::Pressed);
}

TEST(UiButtonSystem, FullClickSequence) {
    ecs::Registry reg;
    ecs::EventBus bus;
    FakeWindow win;
    const ecs::Entity e = makeButton(reg, 0.0F, 0.0F);
    int clicks = 0;
    bus.subscribe<ui::UiButtonClickedEvent>([&](const ui::UiButtonClickedEvent&) { ++clicks; });
    const auto state = [&] { return reg.getComponents<ui::UiButton>().get(e).state; };

    win.mx = 60.0F;
    win.my = 18.0F;
    updateButtons(reg, win, bus);
    EXPECT_EQ(state(), ui::UiButtonState::Hovered);

    win.press();
    updateButtons(reg, win, bus);
    EXPECT_EQ(state(), ui::UiButtonState::Pressed);

    win.leftPressed = false;  // still held on the next frame
    updateButtons(reg, win, bus);
    EXPECT_EQ(state(), ui::UiButtonState::Pressed);

    win.release();
    updateButtons(reg, win, bus);
    EXPECT_EQ(state(), ui::UiButtonState::Released);

    win.leftReleased = false;
    updateButtons(reg, win, bus);
    EXPECT_EQ(state(), ui::UiButtonState::Hovered);
    EXPECT_EQ(clicks, 1);
}

// --- Layout with entity hierarchies ---

TEST(UiLayout, ResolveAnchorIsRelativeToContainer) {
    const ui::UiAnchor anchor{.anchorX = 0.5F, .anchorY = 1.0F, .offsetX = 5.0F, .pivotX = 0.5F, .pivotY = 1.0F};
    const ui::UiScreenRect rect =
        ui::resolveAnchor(anchor, ui::UiScreenRect{.x = 100.0F, .y = 50.0F, .width = 200.0F, .height = 80.0F},
                          ui::UiSize{.width = 40.0F, .height = 20.0F});

    EXPECT_FLOAT_EQ(rect.x, 100.0F + 100.0F + 5.0F - 20.0F);
    EXPECT_FLOAT_EQ(rect.y, 50.0F + 80.0F - 20.0F);
    EXPECT_FLOAT_EQ(rect.width, 40.0F);
    EXPECT_FLOAT_EQ(rect.height, 20.0F);
}

TEST(UiLayout, ElementSizeComesFromDrawable) {
    ecs::Registry reg;
    const ecs::Entity rect = reg.spawnEntity();
    reg.addComponent<ui::UiRect>(rect, ui::UiRect{.width = 30.0F, .height = 12.0F});
    const ecs::Entity text = reg.spawnEntity();
    reg.addComponent<ui::UiText>(text, ui::UiText{.text = "hi"});
    const ecs::Entity group = reg.spawnEntity();

    EXPECT_FLOAT_EQ(ui::elementSize(reg, rect, fakeMeasure).width, 30.0F);
    EXPECT_FLOAT_EQ(ui::elementSize(reg, text, fakeMeasure).height, FAKE_TEXT_SIZE.height);
    EXPECT_FLOAT_EQ(ui::elementSize(reg, group, fakeMeasure).width, 0.0F);
}

TEST(UiLayout, ChildIsAnchoredInsideParentRect) {
    ecs::Registry reg;
    const ecs::Entity panel = reg.spawnEntity();
    reg.addComponent<ui::UiAnchor>(panel, ui::UiAnchor{.anchorX = 0.5F, .anchorY = 0.5F});
    reg.addComponent<ui::UiRect>(panel, ui::UiRect{.width = 400.0F, .height = 200.0F});
    const ecs::Entity button = makeButton(reg, 1.0F, 1.0F, 100.0F, 40.0F);
    reg.getComponents<ui::UiAnchor>().get(button).pivotX = 1.0F;
    reg.getComponents<ui::UiAnchor>().get(button).pivotY = 1.0F;
    ASSERT_TRUE(hierarchy::setParent(reg, button, panel));

    const auto nodes = layoutOf(reg, FakeWindow{});
    const ui::UiLayoutNode* node = findNode(nodes, button);

    ASSERT_NE(node, nullptr);
    // Panel at (960, 540), 400x200: the button sits in its bottom-right corner.
    EXPECT_FLOAT_EQ(node->rect.x, 960.0F + 400.0F - 100.0F);
    EXPECT_FLOAT_EQ(node->rect.y, 540.0F + 200.0F - 40.0F);
}

TEST(UiLayout, NestingAccumulatesThroughSeveralLevels) {
    ecs::Registry reg;
    const ecs::Entity root = reg.spawnEntity();
    reg.addComponent<ui::UiAnchor>(root, ui::UiAnchor{.offsetX = 10.0F, .offsetY = 10.0F});
    const ecs::Entity mid = reg.spawnEntity();
    reg.addComponent<ui::UiAnchor>(mid, ui::UiAnchor{.offsetX = 20.0F});
    const ecs::Entity leaf = reg.spawnEntity();
    reg.addComponent<ui::UiAnchor>(leaf, ui::UiAnchor{.offsetY = 5.0F});
    ASSERT_TRUE(hierarchy::setParent(reg, mid, root));
    ASSERT_TRUE(hierarchy::setParent(reg, leaf, mid));

    const auto nodes = layoutOf(reg, FakeWindow{});
    const ui::UiLayoutNode* node = findNode(nodes, leaf);

    ASSERT_NE(node, nullptr);
    EXPECT_FLOAT_EQ(node->rect.x, 30.0F);
    EXPECT_FLOAT_EQ(node->rect.y, 15.0F);
}

TEST(UiLayout, ParentComesBeforeChildrenInSiblingOrder) {
    ecs::Registry reg;
    // Children spawned before the parent, so storage order alone would put them first.
    const ecs::Entity second = makeButton(reg, 0.0F, 0.0F);
    const ecs::Entity first = makeButton(reg, 0.0F, 0.0F);
    const ecs::Entity panel = reg.spawnEntity();
    reg.addComponent<ui::UiAnchor>(panel, ui::UiAnchor{});
    ASSERT_TRUE(hierarchy::setParent(reg, first, panel));
    ASSERT_TRUE(hierarchy::setParent(reg, second, panel));

    const auto nodes = layoutOf(reg, FakeWindow{});

    ASSERT_EQ(nodes.size(), 3U);
    EXPECT_LT(indexOf(nodes, panel), indexOf(nodes, first));
    EXPECT_LT(indexOf(nodes, first), indexOf(nodes, second));
}

TEST(UiLayout, ChildOfNonUiEntityIsAnchoredToScreen) {
    ecs::Registry reg;
    const ecs::Entity ship = reg.spawnEntity();
    const ecs::Entity label = makeButton(reg, 0.5F, 0.0F);
    ASSERT_TRUE(hierarchy::setParent(reg, label, ship));

    const auto nodes = layoutOf(reg, FakeWindow{});
    const ui::UiLayoutNode* node = findNode(nodes, label);

    ASSERT_NE(node, nullptr);
    EXPECT_FLOAT_EQ(node->rect.x, 960.0F);
}

TEST(UiLayout, ElementUnderNonUiChildIsAnchoredToScreen) {
    ecs::Registry reg;
    const ecs::Entity panel = reg.spawnEntity();
    reg.addComponent<ui::UiAnchor>(panel, ui::UiAnchor{.offsetX = 300.0F});
    const ecs::Entity plain = reg.spawnEntity();
    const ecs::Entity deep = makeButton(reg, 0.0F, 0.0F);
    ASSERT_TRUE(hierarchy::setParent(reg, plain, panel));
    ASSERT_TRUE(hierarchy::setParent(reg, deep, plain));

    const auto nodes = layoutOf(reg, FakeWindow{});
    const ui::UiLayoutNode* node = findNode(nodes, deep);

    EXPECT_EQ(findNode(nodes, plain), nullptr);
    ASSERT_NE(node, nullptr);
    EXPECT_FLOAT_EQ(node->rect.x, 0.0F);
    EXPECT_EQ(std::ranges::count(nodes, deep, &ui::UiLayoutNode::entity), 1);
}

TEST(UiLayout, DestroyingPanelRemovesItsWidgets) {
    ecs::Registry reg;
    const hierarchy::HierarchyHooks hooks(reg);
    const ecs::Entity panel = reg.spawnEntity();
    reg.addComponent<ui::UiAnchor>(panel, ui::UiAnchor{});
    const ecs::Entity button = makeButton(reg, 0.0F, 0.0F);
    ASSERT_TRUE(hierarchy::setParent(reg, button, panel));

    reg.killEntity(panel);

    EXPECT_TRUE(layoutOf(reg, FakeWindow{}).empty());
}

TEST(UiButtonSystem, NestedButtonIsHitWhereItIsDrawn) {
    ecs::Registry reg;
    ecs::EventBus bus;
    FakeWindow win;
    const ecs::Entity panel = reg.spawnEntity();
    reg.addComponent<ui::UiAnchor>(panel, ui::UiAnchor{.offsetX = 500.0F, .offsetY = 300.0F});
    const ecs::Entity button = makeButton(reg, 0.0F, 0.0F);
    ASSERT_TRUE(hierarchy::setParent(reg, button, panel));

    win.mx = 10.0F;
    win.my = 10.0F;
    updateButtons(reg, win, bus);
    EXPECT_EQ(reg.getComponents<ui::UiButton>().get(button).state, ui::UiButtonState::Idle);

    win.mx = 510.0F;
    win.my = 310.0F;
    updateButtons(reg, win, bus);
    EXPECT_EQ(reg.getComponents<ui::UiButton>().get(button).state, ui::UiButtonState::Hovered);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
