#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <deque>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "components/Renderable.hpp"
#include "core/Engine.hpp"
#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "core/services/IInputCaptureService.hpp"
#include "core/services/IScreenDrawService.hpp"
#include "core/services/IWindowService.hpp"
#include "core/services/InputCodes.hpp"
#include "tools/common/TextUtils.hpp"
#include "tools/console/BuiltinCommands.hpp"
#include "tools/console/Console.hpp"
#include "tools/console/ConsolePlugin.hpp"
#include "tools/console/ConsoleTypes.hpp"
#include "tools/cvars/CVarManager.hpp"
#include "tools/devconsole/DevConsole.hpp"
#include "tools/devconsole/DevConsolePlugin.hpp"
#include "tools/devconsole/DevConsoleTypes.hpp"
#include "tools/devconsole/DevConsoleView.hpp"
#include "tools/devconsole/InputLine.hpp"
#include "tools/devconsole/KeyRepeater.hpp"

namespace {

using engine::Key;
using tools::DevConsole;
using tools::EditAction;
using tools::InputLine;
using tools::KeyRepeater;

// --- UTF-8 helpers ----------------------------------------------------------------------------------------------

TEST(TextUtils, EncodesUtf8AndRefusesInvalidCodePoints) {
    EXPECT_EQ(tools::detail::encodeUtf8(U'a'), "a");
    EXPECT_EQ(tools::detail::encodeUtf8(U'é'), "\xC3\xA9");
    EXPECT_EQ(tools::detail::encodeUtf8(U'€'), "\xE2\x82\xAC");
    EXPECT_EQ(tools::detail::encodeUtf8(U'\U0001F600'), "\xF0\x9F\x98\x80");
    EXPECT_TRUE(tools::detail::encodeUtf8(static_cast<char32_t>(0xD800)).empty());
    EXPECT_TRUE(tools::detail::encodeUtf8(static_cast<char32_t>(0x110000)).empty());
}

TEST(TextUtils, StepsOverWholeCodePoints) {
    const std::string text = "a\xC3\xA9z";  // a, é (2 bytes), z
    EXPECT_EQ(tools::detail::nextCodePoint(text, 1), 3U);
    EXPECT_EQ(tools::detail::previousCodePoint(text, 3), 1U);
    EXPECT_EQ(tools::detail::previousCodePoint(text, 0), 0U);
    EXPECT_EQ(tools::detail::nextCodePoint(text, text.size()), text.size());
}

// --- InputLine --------------------------------------------------------------------------------------------------

TEST(InputLine, InsertsAtTheCursorAndMovesOverMultiByteCharacters) {
    InputLine line(64);
    EXPECT_TRUE(line.insert(U'a'));
    EXPECT_TRUE(line.insert(U'é'));
    EXPECT_TRUE(line.insert(U'z'));
    EXPECT_EQ(line.text(), "a\xC3\xA9z");

    line.moveLeft();
    line.moveLeft();
    EXPECT_EQ(line.cursor(), 1U);
    EXPECT_TRUE(line.insert(U'-'));
    EXPECT_EQ(line.text(), "a-\xC3\xA9z");

    line.eraseForward();  // erases é, both bytes
    EXPECT_EQ(line.text(), "a-z");
    line.eraseBackward();
    EXPECT_EQ(line.text(), "az");
    EXPECT_EQ(line.cursor(), 1U);

    line.moveEnd();
    EXPECT_EQ(line.cursor(), 2U);
    line.moveHome();
    line.eraseBackward();  // nothing before the cursor
    EXPECT_EQ(line.text(), "az");
}

TEST(InputLine, RefusesControlCharactersAndOverflow) {
    InputLine line(3);
    EXPECT_FALSE(line.insert(U'\n'));
    EXPECT_FALSE(line.insert(U'\x1b'));
    EXPECT_FALSE(line.insert(U'\x7f'));
    EXPECT_TRUE(line.insert(U'a'));
    EXPECT_TRUE(line.insert(U'é'));
    EXPECT_FALSE(line.insert(U'b'));  // 4 bytes > 3
    EXPECT_EQ(line.text(), "a\xC3\xA9");
}

TEST(InputLine, AssignCutsOnACodePointBoundary) {
    InputLine line(2);
    line.assign("a\xC3\xA9");  // the é would cross the limit
    EXPECT_EQ(line.text(), "a");
    EXPECT_EQ(line.cursor(), 1U);
    line.clear();
    EXPECT_TRUE(line.text().empty());
    EXPECT_EQ(line.cursor(), 0U);
}

// --- KeyRepeater ------------------------------------------------------------------------------------------------

TEST(KeyRepeater, PressesOnceThenRepeatsAfterTheDelay) {
    KeyRepeater key(true, 0.5F, 0.1F);
    EXPECT_TRUE(key.update(true, 0.016F));
    EXPECT_FALSE(key.update(true, 0.3F));
    EXPECT_TRUE(key.update(true, 0.3F));  // 0.6 s held
    EXPECT_FALSE(key.update(true, 0.05F));
    EXPECT_TRUE(key.update(true, 0.06F));
    EXPECT_FALSE(key.update(false, 0.016F));
    EXPECT_TRUE(key.update(true, 0.016F));
}

TEST(KeyRepeater, NonRepeatingKeysPressOncePerHold) {
    KeyRepeater key(false, 0.1F, 0.1F);
    EXPECT_TRUE(key.update(true, 0.016F));
    EXPECT_FALSE(key.update(true, 5.0F));
    EXPECT_FALSE(key.update(false, 0.016F));
    EXPECT_TRUE(key.update(true, 0.016F));
}

TEST(KeyRepeater, ResetMakesAHeldKeyWaitForItsRelease) {
    KeyRepeater key(false, 0.1F, 0.1F);
    key.reset(true);
    EXPECT_FALSE(key.update(true, 0.016F));
    EXPECT_FALSE(key.update(false, 0.016F));
    EXPECT_TRUE(key.update(true, 0.016F));
}

// --- DevConsole -------------------------------------------------------------------------------------------------

class DevConsoleTest : public ::testing::Test {
  protected:
    DevConsoleTest() {
        tools::registerBuiltinCommands(console);
        (void)cvars.add("r_wireframe", false);
        (void)cvars.add("r_draw_colliders", false);
        dev.open();
    }

    void typeText(std::u32string_view text) {
        for (const char32_t cp : text) {
            dev.type(cp);
        }
    }

    [[nodiscard]] std::string lastOutput() const { return console.output().back().text; }

    tools::CVarManager cvars;
    tools::Console console{cvars};
    DevConsole dev{console};
};

TEST_F(DevConsoleTest, IgnoresInputWhileClosed) {
    dev.close();
    dev.type(U'a');
    dev.apply(EditAction::Submit);
    EXPECT_TRUE(dev.input().text().empty());
    EXPECT_TRUE(console.output().empty());
    dev.toggle();
    EXPECT_TRUE(dev.isOpen());
}

TEST_F(DevConsoleTest, SubmitRunsTheLineAndEmptiesTheInput) {
    typeText(U"r_wireframe 1");
    dev.apply(EditAction::Submit);
    EXPECT_TRUE(cvars.find("r_wireframe")->get<bool>());
    EXPECT_TRUE(dev.input().text().empty());
    EXPECT_EQ(lastOutput(), "r_wireframe = true");
}

TEST_F(DevConsoleTest, EditsWithTheCursorKeys) {
    typeText(U"ecoh");
    dev.apply(EditAction::Backspace);
    dev.apply(EditAction::Backspace);
    typeText(U"ho hi");
    dev.apply(EditAction::Home);
    dev.apply(EditAction::Delete);
    typeText(U"e");
    EXPECT_EQ(dev.input().text(), "echo hi");
    dev.apply(EditAction::End);
    dev.apply(EditAction::Left);
    EXPECT_EQ(dev.input().cursor(), 6U);
    dev.apply(EditAction::Right);
    EXPECT_EQ(dev.input().cursor(), 7U);
}

TEST_F(DevConsoleTest, BrowsesTheHistoryAndRestoresTheDraft) {
    typeText(U"echo one");
    dev.apply(EditAction::Submit);
    typeText(U"echo two");
    dev.apply(EditAction::Submit);
    typeText(U"draft");

    dev.apply(EditAction::HistoryPrevious);
    EXPECT_EQ(dev.input().text(), "echo two");
    dev.apply(EditAction::HistoryPrevious);
    EXPECT_EQ(dev.input().text(), "echo one");
    dev.apply(EditAction::HistoryPrevious);  // stays on the oldest
    EXPECT_EQ(dev.input().text(), "echo one");
    dev.apply(EditAction::HistoryNext);
    EXPECT_EQ(dev.input().text(), "echo two");
    dev.apply(EditAction::HistoryNext);
    EXPECT_EQ(dev.input().text(), "draft");
    dev.apply(EditAction::HistoryNext);  // nothing newer
    EXPECT_EQ(dev.input().text(), "draft");
}

TEST_F(DevConsoleTest, CompletesAUniqueNameAndListsSeveralCandidates) {
    typeText(U"toggle r_w");
    dev.apply(EditAction::Complete);
    EXPECT_EQ(dev.input().text(), "toggle r_wireframe ");

    dev.apply(EditAction::Submit);
    typeText(U"r_");
    dev.apply(EditAction::Complete);
    EXPECT_EQ(dev.input().text(), "r_");
    EXPECT_EQ(lastOutput(), "r_draw_colliders  r_wireframe");
}

TEST_F(DevConsoleTest, CompletesTheWordBeforeTheCursorAndKeepsTheRest) {
    typeText(U"set r_w 1");
    dev.apply(EditAction::Left);
    dev.apply(EditAction::Left);
    dev.apply(EditAction::Complete);
    EXPECT_EQ(dev.input().text(), "set r_wireframe  1");
    EXPECT_EQ(dev.input().cursor(), std::string("set r_wireframe ").size());
}

TEST_F(DevConsoleTest, ScrollsWithinTheOutput) {
    for (int i = 0; i < 20; ++i) {
        console.print(tools::OutputLevel::Info, "line");
    }
    dev.setPageSize(5);
    dev.apply(EditAction::PageUp);
    EXPECT_EQ(dev.scrollOffset(), 5U);
    dev.scroll(100);
    EXPECT_EQ(dev.scrollOffset(), 15U);  // the view stays full
    dev.apply(EditAction::PageDown);
    EXPECT_EQ(dev.scrollOffset(), 10U);
    dev.scroll(-100);
    EXPECT_EQ(dev.scrollOffset(), 0U);

    dev.scroll(3);
    typeText(U"echo back");
    dev.apply(EditAction::Submit);
    EXPECT_EQ(dev.scrollOffset(), 0U);  // running a line shows the newest output
}

TEST_F(DevConsoleTest, CloseActionClosesAndKeepsTheLine) {
    typeText(U"half");
    dev.apply(EditAction::Close);
    EXPECT_FALSE(dev.isOpen());
    dev.open();
    EXPECT_EQ(dev.input().text(), "half");
}

// --- DevConsoleView ---------------------------------------------------------------------------------------------

/** @brief Records the draw calls; every character is 10 px wide. */
class RecordingScreen final : public engine::IScreenDrawService {
  public:
    struct Text {
        std::string text;
        float x;
        float y;
        ecs::Rgba color;
    };

    engine::FontId loadFont(const std::string& /*path*/) override { return 7; }
    [[nodiscard]] engine::TextSize measureText(std::string_view text, float fontSize, float /*spacing*/,
                                               engine::FontId /*font*/) const override {
        return {.width = 10.0F * static_cast<float>(text.size()), .height = fontSize};
    }
    void fillRect(const engine::ScreenRect& rect, const ecs::Rgba& /*color*/) override { rects.push_back(rect); }
    void strokeRect(const engine::ScreenRect& /*rect*/, float /*thickness*/, const ecs::Rgba& /*color*/) override {}
    void drawText(std::string_view text, float x, float y, float /*fontSize*/, float /*spacing*/,
                  const ecs::Rgba& color, engine::FontId font) override {
        texts.push_back({.text = std::string(text), .x = x, .y = y, .color = color});
        lastFont = font;
    }
    void drawTexture(ecs::TextureId /*texture*/, const ecs::TextureRect& /*source*/, const engine::ScreenRect& /*dest*/,
                     float /*rotation*/, const ecs::Rgba& /*tint*/) override {}

    [[nodiscard]] const Text* find(std::string_view text) const {
        const auto it = std::ranges::find_if(texts, [text](const Text& t) { return t.text == text; });
        return it == texts.end() ? nullptr : &*it;
    }

    std::vector<engine::ScreenRect> rects;
    std::vector<Text> texts;
    engine::FontId lastFont = engine::NO_FONT;
};

TEST_F(DevConsoleTest, ViewDrawsTheOutputNewestAtTheBottomAndTheInputWithItsCursor) {
    console.print(tools::OutputLevel::Info, "older");
    console.print(tools::OutputLevel::Error, "newer");
    typeText(U"abc");
    dev.apply(EditAction::Left);

    const tools::DevConsoleConfig config;
    tools::DevConsoleView view(config);
    RecordingScreen screen;
    view.draw(screen, dev, 800.0F, 600.0F, true);

    const auto* older = screen.find("older");
    const auto* newer = screen.find("newer");
    const auto* input = screen.find("> abc");
    ASSERT_NE(older, nullptr);
    ASSERT_NE(newer, nullptr);
    ASSERT_NE(input, nullptr);
    EXPECT_LT(older->y, newer->y);
    EXPECT_LT(newer->y, input->y);
    EXPECT_EQ(newer->color.r, config.errorColor.r);
    EXPECT_EQ(newer->color.g, config.errorColor.g);
    EXPECT_EQ(newer->color.b, config.errorColor.b);
    EXPECT_LE(input->y + config.fontSize, 600.0F * config.heightRatio);

    // Cursor after "> ab": padding + 4 characters.
    const bool caretDrawn = std::ranges::any_of(screen.rects, [&config](const engine::ScreenRect& r) {
        return r.width == 2.0F && r.x == config.padding + 40.0F;
    });
    EXPECT_TRUE(caretDrawn);
}

TEST_F(DevConsoleTest, ViewShowsAHintWhenScrolledAndUsesTheFont) {
    for (int i = 0; i < 100; ++i) {
        console.print(tools::OutputLevel::Info, "line " + std::to_string(i));
    }
    tools::DevConsoleView view{tools::DevConsoleConfig{}};
    view.setFont(7);
    dev.setPageSize(view.visibleLines(600.0F));
    dev.scroll(2);

    RecordingScreen screen;
    view.draw(screen, dev, 800.0F, 600.0F, false);
    EXPECT_NE(screen.find("[2 newer line(s) below]"), nullptr);
    EXPECT_NE(screen.find("line 97"), nullptr);
    EXPECT_EQ(screen.find("line 98"), nullptr);
    EXPECT_EQ(screen.lastFont, 7U);
    EXPECT_EQ(screen.texts.size(), view.visibleLines(600.0F) + 2);  // output rows, input, hint
}

// --- DevConsolePlugin -------------------------------------------------------------------------------------------

/** @brief Keyboard state shared by the fake window and the fake capture. */
struct FakeDevice {
    std::set<Key> held;
    std::deque<char32_t> typed;
    float wheel = 0.0F;
    bool captured = false;
};

class FakeWindow final : public engine::IWindowService {
  public:
    explicit FakeWindow(std::shared_ptr<FakeDevice> device) : _device(std::move(device)) {}
    [[nodiscard]] bool shouldClose() const noexcept override { return false; }
    void beginFrame() override {}
    void endFrame() override {}
    [[nodiscard]] int width() const noexcept override { return 800; }
    [[nodiscard]] int height() const noexcept override { return 600; }
    [[nodiscard]] bool isKeyDown(Key key) const noexcept override {
        return !_device->captured && _device->held.contains(key);
    }
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
    [[nodiscard]] engine::MousePosition mousePosition() const noexcept override { return {}; }
    [[nodiscard]] float mouseWheelDelta() const noexcept override { return 0.0F; }
    [[nodiscard]] char32_t nextTypedCharacter() noexcept override { return U'\0'; }

  private:
    std::shared_ptr<FakeDevice> _device;
};

class FakeCapture final : public engine::IInputCaptureService {
  public:
    explicit FakeCapture(std::shared_ptr<FakeDevice> device) : _device(std::move(device)) {}
    void setCaptured(bool captured) override { _device->captured = captured; }
    [[nodiscard]] bool isCaptured() const noexcept override { return _device->captured; }
    [[nodiscard]] bool isKeyDown(Key key) const noexcept override { return _device->held.contains(key); }
    [[nodiscard]] char32_t nextTypedCharacter() noexcept override {
        if (_device->typed.empty()) {
            return U'\0';
        }
        const char32_t cp = _device->typed.front();
        _device->typed.pop_front();
        return cp;
    }
    [[nodiscard]] float mouseWheelDelta() const noexcept override { return _device->wheel; }

  private:
    std::shared_ptr<FakeDevice> _device;
};

class FakeServicesPlugin final : public engine::IPlugin {
  public:
    FakeServicesPlugin(std::shared_ptr<FakeDevice> device, std::shared_ptr<RecordingScreen> screen)
        : _device(std::move(device)), _screen(std::move(screen)) {}
    [[nodiscard]] std::string_view name() const noexcept override { return "FakeServicesPlugin"; }
    void build(engine::PluginBuilder& builder) override {
        builder.provides<engine::IWindowService>();
        builder.provides<engine::IInputCaptureService>();
        builder.provides<engine::IScreenDrawService>();
    }
    void init(engine::Engine& engine) override {
        if (!engine.mediator().registerService<engine::IWindowService>(std::make_shared<FakeWindow>(_device)) ||
            !engine.mediator().registerService<engine::IInputCaptureService>(std::make_shared<FakeCapture>(_device)) ||
            !engine.mediator().registerService<engine::IScreenDrawService>(_screen)) {
            throw std::logic_error("fake services already registered");
        }
    }

  private:
    std::shared_ptr<FakeDevice> _device;
    std::shared_ptr<RecordingScreen> _screen;
};

class DevConsolePluginTest : public ::testing::Test {
  protected:
    DevConsolePluginTest() {
        engine.addPlugin<FakeServicesPlugin>(device, screen);
        engine.addPlugin<tools::ConsolePlugin>();
        engine.addPlugin<tools::DevConsolePlugin>();
        engine.init();
    }

    ~DevConsolePluginTest() override { engine.shutdown(); }
    DevConsolePluginTest(const DevConsolePluginTest&) = delete;
    DevConsolePluginTest& operator=(const DevConsolePluginTest&) = delete;
    DevConsolePluginTest(DevConsolePluginTest&&) = delete;
    DevConsolePluginTest& operator=(DevConsolePluginTest&&) = delete;

    /** @brief Runs one frame with the given keys held. */
    void frame(std::initializer_list<Key> keys = {}) {
        device->held = keys;
        screen->texts.clear();
        engine.advance();
    }

    [[nodiscard]] tools::DevConsole& dev() { return *engine.getPlugin<tools::DevConsolePlugin>().devConsole(); }

    std::shared_ptr<FakeDevice> device = std::make_shared<FakeDevice>();
    std::shared_ptr<RecordingScreen> screen = std::make_shared<RecordingScreen>();
    engine::Engine engine;
};

TEST_F(DevConsolePluginTest, TheToggleKeyOpensTheConsoleAndCapturesTheInput) {
    frame();
    EXPECT_FALSE(dev().isOpen());
    EXPECT_TRUE(screen->texts.empty());

    device->typed.push_back(U'²');  // the character typed by the toggle key itself
    frame({Key::Grave});
    EXPECT_TRUE(dev().isOpen());
    EXPECT_TRUE(device->captured);
    EXPECT_TRUE(dev().input().text().empty());
    EXPECT_NE(screen->find("> "), nullptr);

    frame({Key::Grave});  // still held: no second toggle
    EXPECT_TRUE(dev().isOpen());
    frame();
    frame({Key::Grave});
    EXPECT_FALSE(dev().isOpen());
    EXPECT_FALSE(device->captured);
}

TEST_F(DevConsolePluginTest, TypingAndEnterRunTheLine) {
    frame({Key::Grave});
    frame();
    for (const char32_t cp : std::u32string_view(U"echo hello")) {
        device->typed.push_back(cp);
    }
    frame();
    EXPECT_EQ(dev().input().text(), "echo hello");
    EXPECT_NE(screen->find("> echo hello"), nullptr);

    frame({Key::Enter});
    EXPECT_TRUE(dev().input().text().empty());
    EXPECT_EQ(engine.service<tools::Console>().output().back().text, "hello");
    frame({Key::Enter});  // held: runs once
    EXPECT_EQ(engine.service<tools::Console>().history().size(), 1U);
}

TEST_F(DevConsolePluginTest, EscapeClosesTheConsoleAndKeysHeldWhenOpeningDoNotFire) {
    frame({Key::Enter, Key::Grave});  // Enter held while opening: ignored until released
    EXPECT_TRUE(dev().isOpen());
    frame({Key::Enter});
    EXPECT_TRUE(engine.service<tools::Console>().output().empty());

    frame({Key::Escape});
    EXPECT_FALSE(dev().isOpen());
    EXPECT_FALSE(device->captured);
}

TEST_F(DevConsolePluginTest, TheMouseWheelScrollsTheOutput) {
    for (int i = 0; i < 200; ++i) {
        engine.service<tools::Console>().print(tools::OutputLevel::Info, "line");
    }
    frame({Key::Grave});
    frame();  // the first draw sets the page size
    device->wheel = 1.0F;
    frame();
    EXPECT_EQ(dev().scrollOffset(), 3U);
}

/** @brief A game plugin drawing its own overlay, independent of the console. */
class OverlayPlugin final : public engine::IPlugin {
  public:
    [[nodiscard]] std::string_view name() const noexcept override { return "OverlayPlugin"; }
};

TEST(DevConsolePluginOrder, RunsAfterTheIndependentGamePluginsEvenWhenAddedFirst) {
    engine::Engine engine;
    engine.addPlugin<FakeServicesPlugin>(std::make_shared<FakeDevice>(), std::make_shared<RecordingScreen>());
    engine.addPlugin<tools::ConsolePlugin>();
    engine.addPlugin<tools::DevConsolePlugin>();
    engine.addPlugin<OverlayPlugin>();
    engine.init();
    EXPECT_EQ(engine.executionOrder().back()->name(), "DevConsolePlugin");
    engine.shutdown();
}

TEST_F(DevConsolePluginTest, ShutdownReleasesTheCapture) {
    frame({Key::Grave});
    EXPECT_TRUE(device->captured);
    engine.shutdown();
    EXPECT_FALSE(device->captured);
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
