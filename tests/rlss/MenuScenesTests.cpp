#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "ServerLink.hpp"
#include "core/Engine.hpp"
#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "core/services/IScreenDrawService.hpp"
#include "core/services/IWindowService.hpp"
#include "ecs/Registry.hpp"
#include "events/UiEvents.hpp"
#include "game/ReplicationCodec.hpp"
#include "hierarchy/HierarchyPlugin.hpp"
#include "menu/MenuScenesPlugin.hpp"
#include "net/ClientReplicationPlugin.hpp"
#include "net/INetworkClient.hpp"
#include "net/NetworkPlugin.hpp"
#include "protocol/Protocol.hpp"
#include "scene/ScenePlugin.hpp"
#include "settings/SettingsPlugin.hpp"
#include "ui/UiPlugin.hpp"
#include "ui/components/UiButton.hpp"
#include "ui/components/UiNavigation.hpp"

// The RLSS menus without a window: fake window, draw service and transport, the real scenes and menu plugin.
namespace {

using ecs::net::Bytes;
using ecs::net::Channel;
using ecs::net::EventType;
namespace protocol = rlss::protocol;
using rlss::client::MenuState;

constexpr float DT = 1.0F / 64.0F;
constexpr std::uint16_t MATCH = 3;

/** @brief Window with no input. */
struct QuietWindow final : engine::IWindowService {
    [[nodiscard]] bool shouldClose() const noexcept override { return false; }
    void beginFrame() override {}
    void endFrame() override {}
    [[nodiscard]] int width() const noexcept override { return 1280; }
    [[nodiscard]] int height() const noexcept override { return 720; }
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
    [[nodiscard]] engine::MousePosition mousePosition() const noexcept override { return {.x = -1.0F, .y = -1.0F}; }
    [[nodiscard]] float mouseWheelDelta() const noexcept override { return 0.0F; }
    [[nodiscard]] char32_t nextTypedCharacter() noexcept override { return U'\0'; }
};

/** @brief Draw service that draws nothing. */
struct NullDraw final : engine::IScreenDrawService {
    engine::FontId loadFont(const std::string& /*path*/) override { return engine::NO_FONT; }
    [[nodiscard]] engine::TextSize measureText(std::string_view /*text*/, float size, float /*spacing*/,
                                               engine::FontId /*font*/) const override {
        return {.width = size, .height = size};
    }
    void fillRect(const engine::ScreenRect& /*rect*/, const ecs::Rgba& /*color*/) override {}
    void strokeRect(const engine::ScreenRect& /*rect*/, float /*thickness*/, const ecs::Rgba& /*color*/) override {}
    void drawText(std::string_view /*text*/, float /*x*/, float /*y*/, float /*size*/, float /*spacing*/,
                  const ecs::Rgba& /*color*/, engine::FontId /*font*/) override {}
    void drawTexture(ecs::TextureId /*texture*/, const ecs::TextureRect& /*source*/, const engine::ScreenRect& /*dest*/,
                     float /*rotation*/, const ecs::Rgba& /*tint*/) override {}
};

/** @brief Provides the window and draw services the UI needs, in place of the render plugin. */
class FakeServicesPlugin final : public engine::IPlugin {
  public:
    [[nodiscard]] std::string_view name() const noexcept override { return "FakeServicesPlugin"; }
    void build(engine::PluginBuilder& builder) override {
        builder.provides<engine::IWindowService>();
        builder.provides<engine::IScreenDrawService>();
    }
    void init(engine::Engine& engine) override {
        (void)engine.mediator().registerService<engine::IWindowService>(std::make_shared<QuietWindow>());
        (void)engine.mediator().registerService<engine::IScreenDrawService>(std::make_shared<NullDraw>());
    }
};

/** @brief Client transport the test talks through, in place of the server. */
class FakeClient final : public ecs::net::INetworkClient {
  public:
    bool connect(const std::string& /*host*/, std::uint16_t /*port*/) override { return true; }
    [[nodiscard]] ecs::net::DisconnectReason disconnectReason() const override {
        return ecs::net::DisconnectReason::ConnectionLost;
    }
    bool openUdp(std::uint16_t /*port*/) override { return true; }
    void closeUdp() override {}
    void send(const Bytes& data, Channel /*channel*/) override {
        if (auto message = protocol::decode(data)) {
            sent.push_back(std::move(*message));
        }
    }
    std::vector<ecs::net::NetworkEvent> poll() override { return std::exchange(next, {}); }
    void stop() override {}
    [[nodiscard]] bool isRunning() const override { return true; }

    template <typename Message>
    [[nodiscard]] std::size_t countOf() const {
        std::size_t count = 0;
        for (const auto& message : sent) {
            count += std::holds_alternative<Message>(message) ? 1U : 0U;
        }
        return count;
    }

    std::vector<protocol::Message> sent;
    std::vector<ecs::net::NetworkEvent> next;
};

class RlssMenuTest : public ::testing::Test {
  protected:
    RlssMenuTest() {
        engine.setFixedDeltaTime(DT);
        const auto link = std::make_shared<rlss::client::ServerLink>(transport, std::vector<ecs::net::NetworkEvent>{});
        engine.addPlugin<FakeServicesPlugin>()
            .addPlugin<hierarchy::HierarchyPlugin>()
            .addPlugin<ui::UiPlugin>()
            .addPlugin<scene::ScenePlugin>()
            .addPlugin<settings::SettingsPlugin>(std::filesystem::path{})
            .addPlugin<ecs::net::NetworkPlugin>(link, link)
            .addPlugin<ecs::net::ClientReplicationPlugin>(std::make_shared<rlss::game::ReplicationCodec>())
            .addPlugin<rlss::client::MenuScenesPlugin>(link);
        engine.init();
        frame();
        EXPECT_TRUE(rlss::client::MenuScenesPlugin::confirmName(engine, "TESTER"));
        frame();
    }

    void frame() {
        now += DT;
        engine.advance();
    }

    [[nodiscard]] std::string top() {
        const auto& scenes = engine.getPlugin<scene::ScenePlugin>().manager();
        return scenes.top() == scene::INVALID_SCENE ? "" : std::string(scenes.scene(scenes.top()).name());
    }

    /** @brief Publishes a click, without a frame: several clicks can land in the same frame. */
    void click(std::string action) {
        engine.eventBus().publish(ui::UiButtonClickedEvent{.entity = 0, .action = std::move(action)});
    }

    /** @brief Queues a message of the server, received at the next frame. */
    void queue(const protocol::Message& message) {
        transport->next.push_back(
            {.type = EventType::Data,
             .peer = 0,
             .data = std::visit([](const auto& typed) { return protocol::encode(typed); }, message),
             .channel = Channel::Reliable});
    }

    [[nodiscard]] MenuState& state() { return engine.registry().getResource<MenuState>(); }

    [[nodiscard]] std::string focusedAction() {
        const ecs::Entity focused = engine.registry().getResource<ui::UiFocus>().focused;
        return engine.registry().hasComponent<ui::UiButton>(focused)
                   ? engine.registry().getComponents<ui::UiButton>().get(focused).action
                   : std::string{};
    }

    static protocol::MatchInfo match(std::uint16_t id, bool full = false) {
        // A full match has both teams full; the codec refuses more than `PLAYERS_PER_TEAM` in a team.
        return protocol::MatchInfo{.id = id,
                                   .bluePlayers = full ? protocol::PLAYERS_PER_TEAM : std::uint8_t{1},
                                   .orangePlayers = full ? protocol::PLAYERS_PER_TEAM : std::uint8_t{0}};
    }

    double now = 0.0;
    std::shared_ptr<FakeClient> transport = std::make_shared<FakeClient>();
    engine::Engine engine{[this]() -> double { return now; }};
};

}  // namespace

TEST_F(RlssMenuTest, ASavedOrConfirmedNameOpensTheMenu) { EXPECT_EQ(top(), "Menu"); }

TEST_F(RlssMenuTest, TwoConfirmsInOneFrameSendOneRequest) {
    click("menu.create");
    click("menu.create");
    EXPECT_NO_THROW(frame());
    frame();
    EXPECT_EQ(transport->countOf<protocol::CreateMatch>(), 1U);
    EXPECT_EQ(top(), "Joining");
    EXPECT_TRUE(state().requestPending);
}

TEST_F(RlssMenuTest, BackCancelsTheRequestAndLeavesTheMatchItJoined) {
    click("menu.create");
    frame();
    ASSERT_EQ(top(), "Joining");

    engine.eventBus().publish(ui::UiBackEvent{});
    frame();
    EXPECT_EQ(top(), "Menu");
    EXPECT_EQ(state().notice, "DEMANDE ANNULEE");
    EXPECT_FALSE(state().requestPending);

    // The server had already put the player in the match: the client leaves it instead of entering it.
    queue(protocol::JoinResult{.status = protocol::JoinStatus::Joined, .matchId = MATCH});
    frame();
    frame();
    EXPECT_EQ(top(), "Menu");
    EXPECT_FALSE(state().inMatch);
    EXPECT_EQ(transport->countOf<protocol::LeaveMatch>(), 1U);

    // A new request is answered normally.
    click("menu.create");
    frame();
    queue(protocol::JoinResult{.status = protocol::JoinStatus::Joined, .matchId = MATCH});
    frame();
    frame();
    EXPECT_EQ(top(), "Match");
}

TEST_F(RlssMenuTest, MessagesInTheSamePollAsTheJoinResultAreKept) {
    click("menu.create");
    frame();
    // Joined, the team and the end of the match, all in one poll, then the ended status once more.
    queue(protocol::JoinResult{.status = protocol::JoinStatus::Joined, .matchId = MATCH});
    queue(protocol::Welcome{.carId = 9, .team = protocol::Team::Orange});
    queue(protocol::MatchStatus{.blueScore = 1, .orangeScore = 2, .phase = protocol::MatchPhase::Ended});
    queue(protocol::MatchStatus{.blueScore = 1, .orangeScore = 2, .phase = protocol::MatchPhase::Ended});
    EXPECT_NO_THROW(frame());
    EXPECT_NO_THROW(frame());

    EXPECT_EQ(state().team, protocol::Team::Orange);
    EXPECT_EQ(state().final.orangeScore, 2);
    EXPECT_EQ(top(), "Ended");
    const auto& scenes = engine.getPlugin<scene::ScenePlugin>().manager();
    EXPECT_TRUE(scenes.isActive(*scenes.find("Match")));
}

TEST_F(RlssMenuTest, TheBrowserKeepsTheFocusOnTheSameMatch) {
    queue(protocol::MatchList{.online = 3, .matches = {match(1), match(2), match(MATCH)}});
    frame();
    click("menu.browse");
    frame();
    frame();
    ASSERT_EQ(top(), "Browser");
    EXPECT_EQ(focusedAction(), "browser.join.1");

    // Focus the second match, then the list changes order: the focus follows match 2.
    engine.registry().runSystem<ui::UiButton>([this](ecs::Entity entity, const ui::UiButton& button) {
        if (button.action == "browser.join.2") {
            engine.registry().getResource<ui::UiFocus>().focused = entity;
        }
    });
    queue(protocol::MatchList{.online = 3, .matches = {match(MATCH), match(2)}});
    frame();
    EXPECT_EQ(focusedAction(), "browser.join.2");

    // Match 2 is now full: not joinable, the focus goes to the first joinable match.
    queue(protocol::MatchList{.online = 6, .matches = {match(MATCH), match(2, true)}});
    frame();
    EXPECT_EQ(focusedAction(), "browser.join.3");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
