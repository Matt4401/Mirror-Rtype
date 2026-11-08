#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "core/Engine.hpp"
#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "core/services/IScreenDrawService.hpp"
#include "core/services/IWindowService.hpp"
#include "ecs/Registry.hpp"
#include "events/UiEvents.hpp"
#include "game/ReplicationCodec.hpp"
#include "hierarchy/HierarchyPlugin.hpp"
#include "net/ClientConnection.hpp"
#include "net/ClientReplicationPlugin.hpp"
#include "net/INetworkClient.hpp"
#include "net/NetworkPlugin.hpp"
#include "net/PlayerControlPlugin.hpp"
#include "net/RTypeNetworkPlugin.hpp"
#include "prefabs/PrefabPlugin.hpp"
#include "protocol/Messages.hpp"
#include "protocol/Protocol.hpp"
#include "scene/ScenePlugin.hpp"
#include "scene/scripting/SceneScriptingPlugin.hpp"
#include "scenes/ClientScenesPlugin.hpp"
#include "scenes/GameScenes.hpp"
#include "scripting/ScriptPlugin.hpp"
#include "settings/SettingsPlugin.hpp"
#include "ui/UiPlugin.hpp"
#include "ui/components/UiText.hpp"

// The client's menus, without a window: fake window, draw service and transport, the real scenes and session.
namespace {

using ecs::net::Bytes;
using ecs::net::Channel;
using ecs::net::EventType;
namespace protocol = rtype::protocol;

constexpr float DT = 1.0F / 64.0F;
constexpr std::uint32_t ROOM = 7;

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
    bool connect(const std::string& host, std::uint16_t port) override {
        target = host + ":" + std::to_string(port);
        return true;
    }
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
    [[nodiscard]] std::vector<Message> sentOf() const {
        std::vector<Message> found;
        for (const auto& message : sent) {
            if (const auto* typed = std::get_if<Message>(&message)) {
                found.push_back(*typed);
            }
        }
        return found;
    }

    std::string target;
    std::vector<protocol::Message> sent;
    std::vector<ecs::net::NetworkEvent> next;
};

class ClientScenesTest : public ::testing::Test {
  protected:
    ClientScenesTest() {
        // The scripted level transition is loaded from scripts/ at the project root.
        std::filesystem::current_path(RLSS_PROJECT_DIR);
        engine.setFixedDeltaTime(DT);
        const auto connection = std::make_shared<ecs::net::ClientConnection>(nullptr);
        engine.addPlugin<FakeServicesPlugin>()
            .addPlugin<hierarchy::HierarchyPlugin>()
            .addPlugin<ui::UiPlugin>()
            .addPlugin<scene::ScenePlugin>()
            .addPlugin<scripting::ScriptPlugin>()
            .addPlugin<scene::SceneScriptingPlugin>()
            .addPlugin<prefabs::PrefabPlugin>()
            .addPlugin<settings::SettingsPlugin>(std::filesystem::path{})
            .addPlugin<ecs::net::NetworkPlugin>(connection, connection)
            .addPlugin<ecs::net::ClientReplicationPlugin>(std::make_shared<rtype::game::ReplicationCodec>())
            .addPlugin<client::RTypeNetworkPlugin>(connection,
                                                   [this] {
                                                       transport = std::make_shared<FakeClient>();
                                                       return transport;
                                                   })
            .addPlugin<client::PlayerControlPlugin>()
            .addPlugin<client::ClientScenesPlugin>("127.0.0.1", 4242, true);
        engine.init();
        frame();
    }

    void frame(double seconds = DT) {
        now += seconds;
        engine.advance();
    }

    [[nodiscard]] std::string top() {
        const auto& scenes = engine.getPlugin<scene::ScenePlugin>().manager();
        return scenes.top() == scene::INVALID_SCENE ? "" : std::string(scenes.scene(scenes.top()).name());
    }

    void click(std::string action) {
        engine.eventBus().publish(ui::UiButtonClickedEvent{.entity = 0, .action = std::move(action)});
        frame();
    }

    void receive(const protocol::Message& message) {
        transport->next.push_back(
            {.type = EventType::Data, .peer = 0, .data = protocol::encode(message), .channel = Channel::Reliable});
        frame();
    }

    void reachTheRoom() {
        click("title.play");
        click("connect.go");
        transport->next.push_back({.type = EventType::Connected, .peer = 0, .data = {}, .channel = Channel::Reliable});
        receive(protocol::Welcome{.udpPort = 4243});
        receive(protocol::LobbyList{
            .rooms = {{.roomId = ROOM, .playerCount = 1, .maxPlayers = 4, .state = protocol::RoomState::Waiting}}});
        click("lobby.join." + std::to_string(ROOM));
        receive(protocol::LobbyUpdate{.roomId = ROOM, .slots = {{.clientId = 1, .ready = false}}});
    }

    [[nodiscard]] bool showsText(std::string_view text) {
        bool found = false;
        engine.registry().runSystem<ui::UiText>(
            [&](ecs::Entity /*entity*/, const ui::UiText& ui) { found = found || ui.text == text; });
        return found;
    }

    double now = 0.0;
    std::shared_ptr<FakeClient> transport;
    engine::Engine engine{[this]() -> double { return now; }};
};

}  // namespace

TEST_F(ClientScenesTest, OpensOnTheTitleScreen) { EXPECT_EQ(top(), "Title"); }

TEST_F(ClientScenesTest, SettingsOpenOverTheTitleAndGoBack) {
    click("title.settings");
    EXPECT_EQ(top(), "Settings");
    engine.eventBus().publish(ui::UiBackEvent{});
    frame();
    EXPECT_EQ(top(), "Title");
}

TEST_F(ClientScenesTest, FromTheTitleToARoom) {
    click("title.play");
    EXPECT_EQ(top(), "Connect");
    click("connect.go");
    ASSERT_TRUE(transport);
    EXPECT_EQ(transport->target, "127.0.0.1:4242");

    transport->next.push_back({.type = EventType::Connected, .peer = 0, .data = {}, .channel = Channel::Reliable});
    receive(protocol::Welcome{.udpPort = 4243});
    frame();
    EXPECT_EQ(top(), "Lobby");

    receive(protocol::LobbyList{
        .rooms = {{.roomId = ROOM, .playerCount = 1, .maxPlayers = 4, .state = protocol::RoomState::Waiting}}});
    click("lobby.join." + std::to_string(ROOM));
    ASSERT_EQ(transport->sentOf<protocol::LobbyJoin>().size(), 1U);
    EXPECT_EQ(transport->sentOf<protocol::LobbyJoin>()[0].roomId, ROOM);

    receive(protocol::LobbyUpdate{.roomId = ROOM, .slots = {{.clientId = 1, .ready = false}}});
    frame();
    EXPECT_EQ(top(), "Room");
    click("room.ready");
    ASSERT_EQ(transport->sentOf<protocol::LobbyReady>().size(), 1U);
    EXPECT_TRUE(transport->sentOf<protocol::LobbyReady>()[0].ready);
}

TEST_F(ClientScenesTest, AGameShowsItsLevelsInTheScriptedOverlay) {
    reachTheRoom();
    receive(protocol::GameStart{.roomId = ROOM, .playerIndex = 0, .playerCount = 1});
    receive(protocol::LevelStart{.levelIndex = 0, .levelCount = 3, .name = "Outer Rim"});
    frame();
    frame();
    EXPECT_EQ(top(), std::string(client::LEVEL_TRANSITION_SCENE));
    EXPECT_TRUE(showsText("LEVEL 1")) << "the banner prefab spawned by the Wren scene";
    EXPECT_TRUE(showsText("Outer Rim"));

    frame(3.0);  // the scripted scene waits 2.5 s then pops itself
    frame();
    EXPECT_EQ(top(), "Game");
    EXPECT_FALSE(showsText("Outer Rim")) << "the banner left with its scene";
}

TEST_F(ClientScenesTest, PauseIsLocalAndLeavingGoesBackToTheLobby) {
    reachTheRoom();
    receive(protocol::GameStart{.roomId = ROOM, .playerIndex = 0, .playerCount = 1});
    frame();
    ASSERT_EQ(top(), "Game");

    engine.eventBus().publish(ui::UiBackEvent{});
    frame();
    EXPECT_EQ(top(), "Pause");
    EXPECT_TRUE(engine.getPlugin<client::PlayerControlPlugin>().paused());
    EXPECT_TRUE(engine.getPlugin<scene::ScenePlugin>().manager().isRunning(
        engine.getPlugin<scene::ScenePlugin>().manager().find("Game").value()))
        << "the game keeps running under the pause menu";

    click("pause.leave");
    frame();
    EXPECT_EQ(top(), "Lobby");
    EXPECT_EQ(transport->sentOf<protocol::Leave>().size(), 1U);
    EXPECT_FALSE(engine.getPlugin<client::PlayerControlPlugin>().paused());
}

TEST_F(ClientScenesTest, GameOverShowsTheResult) {
    reachTheRoom();
    receive(protocol::GameStart{.roomId = ROOM, .playerIndex = 0, .playerCount = 1});
    receive(protocol::GameOver{.reason = protocol::GameOverReason::Victory, .score = 42, .playerScores = {42}});
    frame();
    EXPECT_EQ(top(), "GameOver");
    EXPECT_TRUE(showsText("VICTORY"));
    EXPECT_TRUE(showsText("SCORE 42"));
    click("over.lobby");
    EXPECT_EQ(top(), "Lobby");
}

TEST_F(ClientScenesTest, ALostConnectionReturnsToTheConnectScreenWithTheReason) {
    reachTheRoom();
    transport->next.push_back({.type = EventType::Disconnected, .peer = 0, .data = {}, .channel = Channel::Reliable});
    frame();
    frame();
    EXPECT_EQ(top(), "Connect");
    EXPECT_TRUE(showsText("ERROR: connection to the server lost"));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
