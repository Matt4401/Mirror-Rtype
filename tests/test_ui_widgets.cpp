#include <gtest/gtest.h>

#include <deque>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "components/Hidden.hpp"
#include "components/Renderable.hpp"
#include "core/services/IScreenDrawService.hpp"
#include "core/services/IWindowService.hpp"
#include "core/services/InputCodes.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "events/UiEvents.hpp"
#include "hierarchy/Hierarchy.hpp"
#include "hierarchy/HierarchyHooks.hpp"
#include "ui/components/UiAnchor.hpp"
#include "ui/components/UiButton.hpp"
#include "ui/components/UiFocusable.hpp"
#include "ui/components/UiNavigation.hpp"
#include "ui/components/UiRect.hpp"
#include "ui/components/UiScrollList.hpp"
#include "ui/components/UiSlider.hpp"
#include "ui/components/UiText.hpp"
#include "ui/components/UiTextInput.hpp"
#include "ui/components/UiToggle.hpp"
#include "ui/system/UiInteractionSystem.hpp"
#include "ui/system/UiLayout.hpp"
#include "ui/system/UiRenderSystem.hpp"

namespace {

/** @brief Window whose keys, mouse, wheel and typed characters are set by the test. */
struct FakeWindow final : engine::IWindowService {
    std::set<engine::Key> keys;
    std::set<engine::GamepadButton> pad;
    float mx = -1.0F;
    float my = -1.0F;
    bool leftDown = false;
    bool leftPressed = false;
    float wheel = 0.0F;
    std::deque<char32_t> typed;

    [[nodiscard]] bool shouldClose() const noexcept override { return false; }
    void beginFrame() override {}
    void endFrame() override {}
    [[nodiscard]] int width() const noexcept override { return 1000; }
    [[nodiscard]] int height() const noexcept override { return 1000; }
    [[nodiscard]] bool isKeyDown(engine::Key key) const noexcept override { return keys.contains(key); }
    [[nodiscard]] bool isGamepadAvailable(int /*gamepad*/) const noexcept override { return true; }
    [[nodiscard]] bool isGamepadButtonDown(int /*gamepad*/, engine::GamepadButton button) const noexcept override {
        return pad.contains(button);
    }
    [[nodiscard]] float gamepadAxis(int /*gamepad*/, engine::GamepadAxis /*axis*/) const noexcept override {
        return 0.0F;
    }
    [[nodiscard]] bool isMouseButtonDown(engine::MouseButton button) const noexcept override {
        return button == engine::MouseButton::Left && leftDown;
    }
    [[nodiscard]] bool isMouseButtonPressed(engine::MouseButton button) const noexcept override {
        return button == engine::MouseButton::Left && leftPressed;
    }
    [[nodiscard]] bool isMouseButtonReleased(engine::MouseButton /*button*/) const noexcept override { return false; }
    [[nodiscard]] engine::MousePosition mousePosition() const noexcept override { return {.x = mx, .y = my}; }
    [[nodiscard]] float mouseWheelDelta() const noexcept override { return wheel; }
    [[nodiscard]] char32_t nextTypedCharacter() noexcept override {
        if (typed.empty()) {
            return U'\0';
        }
        const char32_t cp = typed.front();
        typed.pop_front();
        return cp;
    }
};

/** @brief Draw service recording fills and strokes. */
struct RecordingDraw final : engine::IScreenDrawService {
    std::vector<std::string> calls;
    engine::FontId loadFont(const std::string& /*path*/) override { return engine::NO_FONT; }
    [[nodiscard]] engine::TextSize measureText(std::string_view text, float /*size*/, float /*spacing*/,
                                               engine::FontId /*font*/) const override {
        return {.width = 10.0F * static_cast<float>(text.size()), .height = 10.0F};
    }
    void fillRect(const engine::ScreenRect& /*rect*/, const ecs::Rgba& /*color*/) override {
        calls.emplace_back("fill");
    }
    void strokeRect(const engine::ScreenRect& /*rect*/, float /*thickness*/, const ecs::Rgba& /*color*/) override {
        calls.emplace_back("stroke");
    }
    void drawText(std::string_view text, float /*x*/, float /*y*/, float /*size*/, float /*spacing*/,
                  const ecs::Rgba& /*color*/, engine::FontId /*font*/) override {
        calls.push_back("text:" + std::string(text));
    }
    void drawTexture(ecs::TextureId /*texture*/, const ecs::TextureRect& /*source*/, const engine::ScreenRect& /*dest*/,
                     float /*rotation*/, const ecs::Rgba& /*tint*/) override {
        calls.emplace_back("texture");
    }
};

ui::UiSize fixedMeasure(const ui::UiText& /*text*/) { return {.width = 10.0F, .height = 10.0F}; }

/** @brief Registry with hierarchy hooks, a fake window, a bus recording every UI event, and the interaction system. */
class UiWidgetsTest : public ::testing::Test {
  protected:
    void SetUp() override {
        registry.emplaceResource<ui::UiFocus>();
        registry.emplaceResource<ui::UiNavigation>();
        (void)bus.subscribe<ui::UiButtonClickedEvent>(
            [this](const ui::UiButtonClickedEvent& e) { events.push_back("click:" + e.action); });
        (void)bus.subscribe<ui::UiToggleChangedEvent>([this](const ui::UiToggleChangedEvent& e) {
            events.push_back("toggle:" + e.action + (e.on ? ":on" : ":off"));
        });
        (void)bus.subscribe<ui::UiValueChangedEvent>([this](const ui::UiValueChangedEvent& e) {
            events.push_back("value:" + e.action + ":" + std::to_string(static_cast<int>(e.value * 100.0F)));
        });
        (void)bus.subscribe<ui::UiTextSubmittedEvent>(
            [this](const ui::UiTextSubmittedEvent& e) { events.push_back("submit:" + e.action + ":" + e.text); });
        (void)bus.subscribe<ui::UiBackEvent>([this](const ui::UiBackEvent& /*e*/) { events.emplace_back("back"); });
    }

    ecs::Entity widget(float y, int order = 0) {
        const ecs::Entity e = registry.spawnEntity();
        registry.addComponent(e, ui::UiAnchor{.offsetX = 10.0F, .offsetY = y});
        registry.addComponent(e, ui::UiFocusable{.order = order});
        return e;
    }

    ecs::Entity button(float y, const std::string& action, int order = 0) {
        const ecs::Entity e = widget(y, order);
        registry.addComponent(e, ui::UiButton{.width = 100.0F, .height = 30.0F, .action = action});
        return e;
    }

    std::vector<ui::UiLayoutNode> layout() {
        std::vector<ui::UiLayoutNode> nodes;
        ui::computeLayout(registry, ui::UiScreenRect{.width = 1000.0F, .height = 1000.0F}, fixedMeasure, nodes);
        return nodes;
    }

    /** @brief Runs one frame of interactions. */
    void frame(float dt = 0.016F) {
        const auto nodes = layout();
        system.update(registry, nodes, window, bus, dt);
        window.leftPressed = false;
        window.wheel = 0.0F;
    }

    /** @brief Presses then releases a key over two frames. */
    void tap(engine::Key key) {
        window.keys.insert(key);
        frame();
        window.keys.erase(key);
        frame();
    }

    void clickAt(float x, float y) {
        window.mx = x;
        window.my = y;
        window.leftDown = true;
        window.leftPressed = true;
        frame();
        window.leftDown = false;
        frame();
    }

    ecs::Entity focused() { return registry.getResource<ui::UiFocus>().focused; }

    ecs::Registry registry;
    hierarchy::HierarchyHooks hooks{registry};
    ecs::EventBus bus;
    FakeWindow window;
    ui::UiInteractionSystem system;
    std::vector<std::string> events;
};

TEST_F(UiWidgetsTest, HiddenElementIsSkippedWithItsSubtree) {
    const ecs::Entity panel = widget(0.0F);
    const ecs::Entity child = widget(10.0F);
    hierarchy::setParent(registry, child, panel);
    EXPECT_EQ(layout().size(), 2U);

    ecs::hide(registry, panel);
    EXPECT_TRUE(layout().empty());
    ecs::show(registry, panel);
    EXPECT_EQ(layout().size(), 2U);
}

TEST_F(UiWidgetsTest, HiddenReasonsAreIndependent) {
    const ecs::Entity e = widget(0.0F);
    ecs::hide(registry, e, ecs::HiddenReason::Manual);
    ecs::hide(registry, e, ecs::HiddenReason::Scene);
    ecs::show(registry, e, ecs::HiddenReason::Scene);
    EXPECT_TRUE(ecs::isHidden(registry, e));
    ecs::show(registry, e, ecs::HiddenReason::Manual);
    EXPECT_FALSE(ecs::isHidden(registry, e));
}

TEST_F(UiWidgetsTest, ScrollListLaysOutOnlyTheRowsInViewInTheirSlots) {
    const ecs::Entity list = registry.spawnEntity();
    registry.addComponent(list, ui::UiAnchor{.offsetX = 0.0F, .offsetY = 100.0F});
    registry.addComponent(list, ui::UiRect{.width = 200.0F, .height = 90.0F});
    registry.addComponent(list, ui::UiScrollList{.first = 1, .visibleCount = 2, .itemHeight = 30.0F});
    std::vector<ecs::Entity> rows;
    for (int i = 0; i < 4; ++i) {
        const ecs::Entity row = button(0.0F, "row" + std::to_string(i));
        registry.getComponents<ui::UiAnchor>().get(row).offsetX = 0.0F;
        hierarchy::setParent(registry, row, list);
        rows.push_back(row);
    }

    const auto nodes = layout();
    ASSERT_EQ(nodes.size(), 3U);
    EXPECT_EQ(nodes[1].entity, rows[1]);
    EXPECT_FLOAT_EQ(nodes[1].rect.y, 100.0F);
    EXPECT_EQ(nodes[2].entity, rows[2]);
    EXPECT_FLOAT_EQ(nodes[2].rect.y, 130.0F);

    registry.getComponents<ui::UiScrollList>().get(list).first = 99;
    (void)layout();
    EXPECT_EQ(registry.getComponents<ui::UiScrollList>().get(list).first, 2U);
}

TEST_F(UiWidgetsTest, DownAndUpWalkTheFocusableWidgetsByOrderAndWrap) {
    const ecs::Entity second = button(100.0F, "b", 2);
    const ecs::Entity first = button(0.0F, "a", 1);
    tap(engine::Key::Down);
    EXPECT_EQ(focused(), first);
    tap(engine::Key::Down);
    EXPECT_EQ(focused(), second);
    tap(engine::Key::Down);
    EXPECT_EQ(focused(), first);
    tap(engine::Key::Up);
    EXPECT_EQ(focused(), second);
}

TEST_F(UiWidgetsTest, HoldingAKeyMovesTheFocusOnce) {
    (void)button(0.0F, "a");
    const ecs::Entity second = button(100.0F, "b");
    window.keys.insert(engine::Key::Down);
    frame();
    frame();
    frame();
    EXPECT_NE(focused(), second);
}

TEST_F(UiWidgetsTest, ConfirmClicksTheFocusedButtonFromKeyboardOrGamepad) {
    (void)button(0.0F, "play");
    tap(engine::Key::Down);
    tap(engine::Key::Enter);
    window.pad.insert(engine::GamepadButton::RightFaceDown);
    frame();
    EXPECT_EQ(events, (std::vector<std::string>{"click:play", "click:play"}));
}

TEST_F(UiWidgetsTest, BackPublishesABackEvent) {
    tap(engine::Key::Escape);
    EXPECT_EQ(events, (std::vector<std::string>{"back"}));
}

TEST_F(UiWidgetsTest, ToggleFlipsOnClickAndOnConfirm) {
    const ecs::Entity toggle = widget(0.0F);
    registry.addComponent(toggle, ui::UiToggle{.action = "metrics"});
    clickAt(15.0F, 5.0F);
    EXPECT_EQ(focused(), toggle);
    tap(engine::Key::Space);
    EXPECT_EQ(events, (std::vector<std::string>{"toggle:metrics:on", "toggle:metrics:off"}));
}

TEST_F(UiWidgetsTest, SliderMovesByStepAndStaysInRange) {
    const ecs::Entity slider = widget(0.0F);
    registry.addComponent(slider, ui::UiSlider{.value = 0.9F, .step = 0.25F, .action = "volume"});
    tap(engine::Key::Down);
    ASSERT_EQ(focused(), slider);
    tap(engine::Key::Right);
    tap(engine::Key::Right);
    tap(engine::Key::Left);
    EXPECT_EQ(events, (std::vector<std::string>{"value:volume:100", "value:volume:75"}));
}

TEST_F(UiWidgetsTest, SliderFollowsTheMouseWhileDragged) {
    const ecs::Entity slider = widget(0.0F);
    registry.addComponent(slider, ui::UiSlider{.action = "volume", .width = 100.0F});
    window.mx = 10.0F;
    window.my = 5.0F;
    window.leftDown = true;
    window.leftPressed = true;
    frame();
    window.mx = 60.0F;
    frame();
    EXPECT_FLOAT_EQ(registry.getComponents<ui::UiSlider>().get(slider).value, 0.5F);
    window.leftDown = false;
    window.mx = 110.0F;
    frame();
    EXPECT_FLOAT_EQ(registry.getComponents<ui::UiSlider>().get(slider).value, 0.5F);
}

TEST_F(UiWidgetsTest, TextInputTypesFiltersAndSubmits) {
    const ecs::Entity input = widget(0.0F);
    registry.addComponent(input, ui::UiTextInput{.action = "host", .allowed = "0123456789.", .maxLength = 9});
    clickAt(15.0F, 5.0F);
    ASSERT_EQ(focused(), input);

    window.typed = {U'1', U'2', U'x', U'7', U'.', U'0', U' ', U'.', U'0', U'.', U'1', U'9'};
    frame();
    EXPECT_EQ(registry.getComponents<ui::UiTextInput>().get(input).text, "127.0.0.1");

    tap(engine::Key::Space);  // types (filtered here), never submits
    tap(engine::Key::Enter);
    EXPECT_EQ(events, (std::vector<std::string>{"submit:host:127.0.0.1"}));
}

TEST_F(UiWidgetsTest, TextInputAcceptsUtf8AndErasesWholeCharacters) {
    const ecs::Entity input = widget(0.0F);
    registry.addComponent(input, ui::UiTextInput{.action = "name"});
    tap(engine::Key::Down);
    // Code points rather than U'\u...' literals, which MSVC rejects without /utf-8.
    window.typed = {U'J', static_cast<char32_t>(0xE9), static_cast<char32_t>(0x20AC)};
    frame();
    EXPECT_EQ(registry.getComponents<ui::UiTextInput>().get(input).text, "J\xc3\xa9\xe2\x82\xac");
    tap(engine::Key::Backspace);
    EXPECT_EQ(registry.getComponents<ui::UiTextInput>().get(input).text, "J\xc3\xa9");
}

TEST_F(UiWidgetsTest, BackspaceRepeatsWhileHeld) {
    const ecs::Entity input = widget(0.0F);
    registry.addComponent(input, ui::UiTextInput{.text = "abcdef"});
    tap(engine::Key::Down);
    window.keys.insert(engine::Key::Backspace);
    frame(0.01F);  // first erase on the press
    frame(0.30F);  // still in the delay
    frame(0.20F);  // delay over: repeat
    frame(0.05F);  // and again
    EXPECT_EQ(registry.getComponents<ui::UiTextInput>().get(input).text, "abc");
}

TEST_F(UiWidgetsTest, ClickingNothingClearsTheFocus) {
    const ecs::Entity b = button(0.0F, "a");
    clickAt(15.0F, 5.0F);
    EXPECT_EQ(focused(), b);
    clickAt(900.0F, 900.0F);
    EXPECT_EQ(focused(), ecs::NULL_ENTITY);
}

TEST_F(UiWidgetsTest, HiddenFocusedWidgetLosesTheFocus) {
    const ecs::Entity b = button(0.0F, "a");
    tap(engine::Key::Down);
    ASSERT_EQ(focused(), b);
    ecs::hide(registry, b);
    frame();
    EXPECT_EQ(focused(), ecs::NULL_ENTITY);
}

TEST_F(UiWidgetsTest, NavigationInsideAListScrollsIt) {
    const ecs::Entity list = registry.spawnEntity();
    registry.addComponent(list, ui::UiAnchor{});
    registry.addComponent(list, ui::UiRect{.width = 200.0F, .height = 60.0F});
    registry.addComponent(list, ui::UiScrollList{.visibleCount = 2, .itemHeight = 30.0F});
    std::vector<ecs::Entity> rows;
    for (int i = 0; i < 4; ++i) {
        const ecs::Entity row = button(0.0F, "row");
        hierarchy::setParent(registry, row, list);
        rows.push_back(row);
    }
    auto& scroll = registry.getComponents<ui::UiScrollList>().get(list);

    tap(engine::Key::Down);
    EXPECT_EQ(focused(), rows[0]);
    tap(engine::Key::Down);
    tap(engine::Key::Down);
    EXPECT_EQ(focused(), rows[2]);
    EXPECT_EQ(scroll.first, 1U);
    tap(engine::Key::Down);
    EXPECT_EQ(focused(), rows[3]);
    EXPECT_EQ(scroll.first, 2U);
    tap(engine::Key::Up);
    tap(engine::Key::Up);
    tap(engine::Key::Up);
    EXPECT_EQ(focused(), rows[0]);
    EXPECT_EQ(scroll.first, 0U);
}

TEST_F(UiWidgetsTest, WheelScrollsTheListUnderTheCursor) {
    const ecs::Entity list = registry.spawnEntity();
    registry.addComponent(list, ui::UiAnchor{});
    registry.addComponent(list, ui::UiRect{.width = 200.0F, .height = 60.0F});
    registry.addComponent(list, ui::UiScrollList{.visibleCount = 2, .itemHeight = 30.0F});
    for (int i = 0; i < 3; ++i) {
        hierarchy::setParent(registry, button(0.0F, "row"), list);
    }
    auto& scroll = registry.getComponents<ui::UiScrollList>().get(list);
    window.mx = 50.0F;
    window.my = 20.0F;
    window.wheel = -1.0F;
    frame();
    EXPECT_EQ(scroll.first, 1U);
    window.wheel = -1.0F;
    frame();
    EXPECT_EQ(scroll.first, 1U);
    window.wheel = 1.0F;
    frame();
    EXPECT_EQ(scroll.first, 0U);
}

TEST_F(UiWidgetsTest, RendererDrawsTheFocusOutlineAndSkipsHiddenWidgets) {
    const ecs::Entity b = button(0.0F, "a");
    const ecs::Entity hidden = registry.spawnEntity();
    registry.addComponent(hidden, ui::UiAnchor{});
    registry.addComponent(hidden, ui::UiText{.text = "secret"});
    ecs::hide(registry, hidden);
    registry.getResource<ui::UiFocus>().focused = b;

    RecordingDraw draw;
    ui::UiRenderSystem renderer;
    renderer.update(registry, ui::UiScreenRect{.width = 1000.0F, .height = 1000.0F}, draw);
    EXPECT_EQ(draw.calls, (std::vector<std::string>{"fill", "stroke"}));
}

TEST_F(UiWidgetsTest, RendererDrawsTextInputPlaceholderAndCaret) {
    const ecs::Entity input = widget(0.0F);
    registry.addComponent(input, ui::UiTextInput{.placeholder = "host"});
    auto& focus = registry.getResource<ui::UiFocus>();
    focus.focused = input;
    focus.caretVisible = true;
    registry.getResource<ui::UiNavigation>().focusThickness = 0.0F;

    RecordingDraw draw;
    ui::UiRenderSystem renderer;
    renderer.update(registry, ui::UiScreenRect{.width = 1000.0F, .height = 1000.0F}, draw);
    EXPECT_EQ(draw.calls, (std::vector<std::string>{"fill", "stroke", "text:host", "fill"}));
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
