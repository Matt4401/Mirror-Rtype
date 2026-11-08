#include "scenes/GameScenes.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "components/Hidden.hpp"
#include "components/Renderable.hpp"
#include "core/Engine.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "gameplay/BossEffects.hpp"
#include "net/PlayerControlPlugin.hpp"
#include "net/RTypeNetworkPlugin.hpp"
#include "net/SessionEvents.hpp"
#include "protocol/Messages.hpp"
#include "scene/SceneContext.hpp"
#include "scene/SceneManager.hpp"
#include "scene/SceneTypes.hpp"
#include "scenes/LobbyScenes.hpp"
#include "scenes/MenuKit.hpp"
#include "scenes/MenuScenes.hpp"
#include "ui/components/UiAnchor.hpp"
#include "ui/components/UiProgressBar.hpp"
#include "ui/components/UiRect.hpp"
#include "ui/components/UiText.hpp"

namespace client {

namespace protocol = rtype::protocol;

namespace {

/** @brief Size of the dimming rectangle behind the pause menu; larger than any window. */
constexpr float DIM_SIZE = 8192.0F;

/**
 * @brief Gets the session of the client.
 * @param context Scene context.
 * @return The session plugin.
 */
RTypeNetworkPlugin& sessionOf(scene::SceneContext& context) { return context.engine().getPlugin<RTypeNetworkPlugin>(); }

/**
 * @brief Spawns a HUD text anchored on the screen.
 * @param context Scene owning it.
 * @param anchor Placement.
 * @param text Initial text.
 * @param size Font size.
 * @param color Colour.
 * @return The text element.
 */
ecs::Entity hudText(scene::SceneContext& context, const ui::UiAnchor& anchor, std::string text, float size,
                    ecs::Rgba color) {
    ecs::Registry& registry = context.registry();
    const ecs::Entity entity = context.spawn();
    registry.addComponent(entity, anchor);
    registry.addComponent(entity, ui::UiText{.text = std::move(text), .fontSize = size, .color = color});
    return entity;
}

/** @brief Height of the boss health bar, as a fraction of the screen: below the HUD line. */
constexpr float BOSS_BAR_Y = 0.11F;
/** @brief Width of the boss health bar, in pixels. */
constexpr float BOSS_BAR_WIDTH = 480.0F;
/** @brief Height of the boss health bar, in pixels. */
constexpr float BOSS_BAR_HEIGHT = 12.0F;
/** @brief Colour of the hit points left. */
constexpr ecs::Rgba BOSS_BAR_COLOR = {.r = 230, .g = 60, .b = 70, .a = 255};
/** @brief Colour of the hit points lost. */
constexpr ecs::Rgba BOSS_BAR_BACKGROUND = {.r = 50, .g = 20, .b = 30, .a = 220};

}  // namespace

// --- Game ------------------------------------------------------------------------------------------------------------

void GameScene::onEnter(scene::SceneContext& context) {
    menu::focus(context.registry(), ecs::NULL_ENTITY);
    const RTypeNetworkPlugin& session = sessionOf(context);
    const int player = session.playerIndex().value_or(0) + 1;
    (void)hudText(context, ui::UiAnchor{.anchorX = 0.02F, .anchorY = 0.03F}, "P" + std::to_string(player), 24.0F,
                  menu::ACCENT);
    _level = hudText(context, ui::UiAnchor{.anchorX = 0.5F, .anchorY = 0.03F, .pivotX = 0.5F}, "", 22.0F, menu::TEXT);
    _score = hudText(context, ui::UiAnchor{.anchorX = 0.98F, .anchorY = 0.03F, .pivotX = 1.0F}, "SCORE 0", 24.0F,
                     menu::GOLD);
    spawnBossBar(context);

    const auto onLevel = [this, &context](const protocol::LevelStart& level) {
        menu::setText(context.registry(), _level,
                      "LEVEL " + std::to_string(level.levelIndex + 1) + "/" + std::to_string(level.levelCount) + "  " +
                          level.name);
        showBanner(context, "LEVEL " + std::to_string(level.levelIndex + 1), level.name);
    };
    context.subscribe<LevelStarted>([onLevel](const LevelStarted& event) { onLevel(event.level); });
    context.subscribe<LevelCompleted>([this, &context](const LevelCompleted& event) {
        menu::setText(context.registry(), _score, "SCORE " + std::to_string(event.result.score));
        showBanner(context, "LEVEL " + std::to_string(event.result.levelIndex + 1) + " CLEARED",
                   "SCORE " + std::to_string(event.result.score));
    });
    context.subscribe<GameEnded>(
        [&context](const GameEnded& /*event*/) { context.manager().switchTo<GameOverScene>(); });
    context.subscribe<RoomLeft>([&context](const RoomLeft& /*event*/) { context.manager().switchTo<LobbyScene>(); });
    context.subscribe<SessionClosed>(
        [&context](const SessionClosed& /*event*/) { context.manager().switchTo<ConnectScene>(); });
    menu::onBack(context, [&context] { context.manager().push<PauseScene>(); });

    // The first LevelStart follows GameStart at once: it may have arrived before this scene entered.
    if (const auto& level = session.level()) {
        onLevel(*level);
    }
}

void GameScene::spawnBossBar(scene::SceneContext& context) {
    ecs::Registry& registry = context.registry();
    _bossCaption = hudText(context, ui::UiAnchor{.anchorX = 0.5F, .anchorY = BOSS_BAR_Y - 0.03F, .pivotX = 0.5F},
                           "BOSS", 16.0F, menu::ERROR);
    _bossBar = context.spawn();
    registry.addComponent(_bossBar, ui::UiAnchor{.anchorX = 0.5F, .anchorY = BOSS_BAR_Y, .pivotX = 0.5F});
    registry.addComponent(_bossBar, ui::UiProgressBar{.width = BOSS_BAR_WIDTH,
                                                      .height = BOSS_BAR_HEIGHT,
                                                      .foreground = BOSS_BAR_COLOR,
                                                      .background = BOSS_BAR_BACKGROUND});
    ecs::hide(registry, _bossBar);
    ecs::hide(registry, _bossCaption);
    context.addSystem(scene::SceneStage::Update, [this](engine::Engine& engine, const ecs::time::Time& /*time*/) {
        ecs::Registry& world = engine.registry();
        const std::optional<gameplay::BossHealth> health = gameplay::bossHealth(world);
        for (const ecs::Entity element : {_bossBar, _bossCaption}) {
            if (health) {
                ecs::show(world, element);
            } else {
                ecs::hide(world, element);
            }
        }
        if (health) {
            auto& bar = world.getComponents<ui::UiProgressBar>().get(_bossBar);
            bar.value = health->points;
            bar.max = std::max(health->max, 1.0F);
        }
    });
}

void GameScene::showBanner(scene::SceneContext& context, std::string title, std::string subtitle) {
    scene::SceneManager& scenes = context.manager();
    const std::optional<scene::SceneId> transition = scenes.find(LEVEL_TRANSITION_SCENE);
    if (!transition) {
        return;
    }
    ecs::Registry& registry = context.registry();
    if (!registry.hasResource<LevelBanner>()) {
        registry.emplaceResource<LevelBanner>();
    }
    registry.getResource<LevelBanner>() = LevelBanner{.title = std::move(title), .subtitle = std::move(subtitle)};
    if (scenes.top() == *transition) {
        scenes.replace(*transition);  // a new banner restarts the overlay
    } else if (scenes.top() == context.id()) {
        scenes.push(*transition);
    }
}

// --- Pause -----------------------------------------------------------------------------------------------------------

void PauseScene::onEnter(scene::SceneContext& context) {
    ecs::Registry& registry = context.registry();
    context.engine().getPlugin<PlayerControlPlugin>().setPaused(true);

    const ecs::Entity dim = context.spawn();
    registry.addComponent(dim, ui::UiAnchor{});
    registry.addComponent(
        dim, ui::UiRect{.width = DIM_SIZE, .height = DIM_SIZE, .fillColor = {.r = 0, .g = 0, .b = 0, .a = 150}});

    const ecs::Entity box = menu::panel(context, 420.0F, 330.0F);
    (void)menu::label(context, box, "PAUSED", 28.0F, 40.0F, menu::GOLD);
    (void)menu::label(context, box, "THE GAME GOES ON FOR THE OTHER PLAYERS", 82.0F, 14.0F, menu::MUTED);
    _resume = menu::button(context, box, "RESUME", "pause.resume", 130.0F, 0);
    (void)menu::button(context, box, "SETTINGS", "pause.settings", 185.0F, 1);
    (void)menu::button(context, box, "LEAVE THE GAME", "pause.leave", 240.0F, 2);
    menu::focus(registry, _resume);

    menu::onBack(context, [&context] { context.manager().pop(); });
    menu::onClick(context, [&context](std::string_view action) {
        if (action == "pause.resume") {
            context.manager().pop();
        } else if (action == "pause.settings") {
            context.manager().push<SettingsScene>();
        } else if (action == "pause.leave") {
            context.bus().publish(LeaveRoomRequest{});
        }
    });
}

void PauseScene::onExit(scene::SceneContext& context) {
    context.engine().getPlugin<PlayerControlPlugin>().setPaused(false);
    menu::focus(context.registry(), ecs::NULL_ENTITY);
}

void PauseScene::onResume(scene::SceneContext& context) { menu::focus(context.registry(), _resume); }

// --- Game over -------------------------------------------------------------------------------------------------------

void GameOverScene::onEnter(scene::SceneContext& context) {
    const auto& result = sessionOf(context).lastResult();
    const ecs::Entity box = menu::panel(context, 460.0F, 400.0F);
    std::string title = "GAME OVER";
    ecs::Rgba color = menu::ERROR;
    if (result && result->reason == protocol::GameOverReason::Victory) {
        title = "VICTORY";
        color = menu::GOOD;
    } else if (result && result->reason == protocol::GameOverReason::ServerShutdown) {
        title = "THE SERVER STOPPED";
        color = menu::MUTED;
    }
    (void)menu::label(context, box, title, 30.0F, 48.0F, color);
    const std::uint32_t score = result ? result->score : 0;
    (void)menu::label(context, box, "SCORE " + std::to_string(score), 100.0F, 28.0F, menu::GOLD);
    if (result) {
        for (std::size_t i = 0; i < result->playerScores.size(); ++i) {
            (void)menu::label(context, box,
                              "PLAYER " + std::to_string(i + 1) + "    " + std::to_string(result->playerScores[i]),
                              150.0F + (30.0F * static_cast<float>(i)), 20.0F);
        }
    }
    const ecs::Entity back = menu::button(context, box, "BACK TO THE LOBBY", "over.lobby", 330.0F, 0);
    menu::focus(context.registry(), back);

    const auto toLobby = [&context] { context.manager().switchTo<LobbyScene>(); };
    menu::onBack(context, toLobby);
    menu::onClick(context, [toLobby](std::string_view action) {
        if (action == "over.lobby") {
            toLobby();
        }
    });
    context.subscribe<SessionClosed>(
        [&context](const SessionClosed& /*event*/) { context.manager().switchTo<ConnectScene>(); });
}

}  // namespace client
