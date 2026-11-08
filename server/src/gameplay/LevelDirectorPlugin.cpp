#include "gameplay/LevelDirectorPlugin.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "LogTag.hpp"
#include "Logger.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "game/ReplicationCodec.hpp"
#include "gameplay/GameplayEvents.hpp"
#include "level/Level.hpp"
#include "net/ReplicationSystems.hpp"
#include "prefabs/PrefabParams.hpp"
#include "prefabs/alien_scout/AlienScout.hpp"
#include "prefabs/boss_core/BossCore.hpp"
#include "protocol/Messages.hpp"
#include "timeline/TimelineEvent.hpp"
#include "timeline/TimelineEvents.hpp"
#include "timeline/TimelinePlugin.hpp"

namespace rtype::server::gameplay {

namespace {

/**
 * @brief Subscribes a handler and returns the token with the matching unsubscribe function.
 * @tparam Event Event type.
 * @param bus Room bus.
 * @param handler Callback.
 * @return What `shutdown()` needs to unsubscribe.
 */
template <typename Event, typename Handler>
std::pair<ecs::EventBus::EventToken, void (*)(ecs::EventBus&, ecs::EventBus::EventToken)> listen(ecs::EventBus& bus,
                                                                                                 Handler handler) {
    return {bus.subscribe<Event>(std::move(handler)),
            [](ecs::EventBus& owner, const ecs::EventBus::EventToken token) { owner.unsubscribe<Event>(token); }};
}

}  // namespace

LevelDirectorPlugin::LevelDirectorPlugin(std::shared_ptr<const level::Level> level) : _level(std::move(level)) {}

void LevelDirectorPlugin::build(engine::PluginBuilder& builder) { builder.require<timeline::TimelinePlugin>(); }

std::optional<std::vector<std::string>> LevelDirectorPlugin::levelPrefab(const std::string_view type,
                                                                         const std::string_view prefab) {
    if (type == level::EVENT_SPAWN && prefab == ::prefabs::ALIEN_SCOUT_PREFAB) {
        return std::vector<std::string>{"speed", "z", "rotation", "scaleX", "scaleY"};
    }
    if (type == level::EVENT_BOSS && prefab == ::prefabs::BOSS_CORE_PREFAB) {
        return std::vector<std::string>{"speed", "stop_x", "health", "z", "rotation", "scaleX", "scaleY"};
    }
    return std::nullopt;
}

void LevelDirectorPlugin::init(engine::Engine& engine) {
    _registry = &engine.registry();
    _bus = &engine.eventBus();
    _timeline = &engine.getPlugin<timeline::TimelinePlugin>();
    ecs::EventBus& bus = *_bus;
    _tokens.push_back(listen<timeline::TimelineEventFired>(
        bus, [this](const timeline::TimelineEventFired& fired) { play(fired.event); }));
    _tokens.push_back(listen<BossFightStarted>(bus, [this](const BossFightStarted& /*started*/) {
        _bossFight = true;
        holdScroll();
    }));
    _tokens.push_back(listen<BossFightEnded>(bus, [this](const BossFightEnded& /*ended*/) {
        _bossFight = false;
        releaseScroll();
    }));
    _tokens.push_back(listen<ShipRespawnStarted>(bus, [this](const ShipRespawnStarted& /*started*/) {
        _respawning = true;
        holdScroll();
    }));
    _tokens.push_back(listen<AllRespawnsOver>(bus, [this](const AllRespawnsOver& /*over*/) {
        _respawning = false;
        releaseScroll();
    }));
    startLevel();
}

void LevelDirectorPlugin::shutdown(engine::Engine& /*engine*/) {
    for (const auto& [token, unsubscribe] : _tokens) {
        unsubscribe(*_bus, token);
    }
    _tokens.clear();
    _timeline = nullptr;
}

void LevelDirectorPlugin::startGame() {
    _gameStarted = true;
    const std::string name = _level != nullptr ? _level->name : "";
    _bus->publish(protocol::LevelStart{
        .levelIndex = 0,
        .levelCount = 1,
        .name = name.substr(0, protocol::MAX_LEVEL_NAME_LENGTH),
    });
}

void LevelDirectorPlugin::startLevel() {
    if (_timeline == nullptr || _level == nullptr) {
        return;
    }
    _timeline->timeline() = _level->timeline;
    _timeline->setSpeed(_level->scrollSpeed);
    _scrollHeld = false;
    _heldEvents.clear();
    _bus->publish(LevelStarted{});
}

void LevelDirectorPlugin::play(const timeline::TimelineEvent& event) {
    if (_finished) {
        return;
    }
    if (_scrollHeld && event.type != level::EVENT_BOSS) {
        _heldEvents.push_back(event);
        return;
    }
    _bus->publish(LevelEventPlayed{.event = event});
    if (event.type == level::EVENT_SPAWN) {
        spawnEnemy(event.params);
    } else if (event.type == level::EVENT_SCROLL) {
        _timeline->setSpeed(event.params.get("speed", _timeline->speed()));
    } else if (event.type == level::EVENT_END && _gameStarted) {
        log::info(LOG_TAG, "level '", _level != nullptr ? _level->name : "", "' complete");
        _finished = true;
        _timeline->setSpeed(0.0F);
        _bus->publish(LevelFinished{.levelIndex = 0, .lastLevel = true});
    } else if (event.type == level::EVENT_END) {
        log::info(LOG_TAG, "level '", _level != nullptr ? _level->name : "", "' complete, restarting it");
        startLevel();
    }
}

void LevelDirectorPlugin::spawnEnemy(const ::prefabs::PrefabParams& params) {
    // The loader only keeps the prefabs accepted by levelPrefab(), so the name is known here.
    if (params.get<std::string>("prefab", "") != ::prefabs::ALIEN_SCOUT_PREFAB) {
        return;
    }
    ecs::Registry& registry = *_registry;
    const ecs::Entity enemy = registry.spawnEntity();
    ::prefabs::buildAlienScout(enemy, registry, params);
    registry.addComponent(enemy, game::EntityNetState{});
    ecs::net::replicate(registry, enemy, game::KIND_ALIEN_SCOUT);
}

void LevelDirectorPlugin::holdScroll() {
    if (_scrollHeld || _timeline == nullptr) {
        return;
    }
    _scrollHeld = true;
    _speedBeforeHold = _timeline->speed();
    _timeline->setSpeed(0.0F);
}

void LevelDirectorPlugin::releaseScroll() {
    if (!_scrollHeld || _bossFight || _respawning) {
        return;
    }
    _scrollHeld = false;
    _timeline->setSpeed(_speedBeforeHold);
    log::info(LOG_TAG, "the level goes on");
    // Played in order; a new boss among them holds the scroll again and keeps the next ones waiting.
    const std::vector<timeline::TimelineEvent> waiting = std::move(_heldEvents);
    _heldEvents.clear();
    for (const timeline::TimelineEvent& event : waiting) {
        play(event);
    }
}

}  // namespace rtype::server::gameplay
