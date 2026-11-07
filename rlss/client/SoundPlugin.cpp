#include "SoundPlugin.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "CameraPlugin.hpp"
#include "ClientEvents.hpp"
#include "HudPlugin.hpp"
#include "Logger.hpp"
#include "audio/AudioPlugin.hpp"
#include "audio/spatial/SpatialConfig.hpp"
#include "audio/types/AudioConstants.hpp"
#include "audio/types/AudioTypes.hpp"
#include "components/AudioEmitter.hpp"
#include "components/AudioListener.hpp"
#include "components/CameraComponent.hpp"
#include "components/Transform.hpp"
#include "core/Engine.hpp"
#include "core/Mediator.hpp"
#include "core/PluginBuilder.hpp"
#include "core/services/IAudioService.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "events/AudioEvents.hpp"
#include "game/Components.hpp"
#include "net/ClientReplicationPlugin.hpp"
#include "net/ReplicationEvents.hpp"
#include "protocol/Protocol.hpp"

namespace rlss::client {

namespace {

constexpr const char* LOG_TAG = "sound";

/** @brief The wire keeps the low 4 bits of the cue counters: compare them on those bits only. */
constexpr int CUE_MASK = protocol::SOUND_CUE_MAX;

// --- Spatialization (uu): the camera shows 1500 to 2900 uu across, everything on screen is heard in full ---
constexpr audio::SpatialConfig SPATIAL{
    .enabled = true,
    .panRange = 1500.0F,
    .maxPan = 0.6F,
    .minDistance = 1600.0F,
    .maxDistance = 4000.0F,
    .mergeDistance = 32.0F,
};

// --- Engine and boost loops ---
constexpr float ENGINE_IDLE_VOLUME = 0.18F;
constexpr float ENGINE_FULL_VOLUME = 0.42F;
constexpr float ENGINE_IDLE_PITCH = 0.7F;
constexpr float ENGINE_FULL_PITCH = 1.9F;
/** @brief Speed at which the engine sounds its highest (the car top speed). */
constexpr float ENGINE_FULL_SPEED = 2300.0F;
/** @brief How fast the measured speed follows the motion, per second (smooths the snapshots). */
constexpr float SPEED_SMOOTHING = 10.0F;
/** @brief A car moving farther than this in one frame was teleported (kickoff): it does not rev the engine. */
constexpr float TELEPORT_DISTANCE = 200.0F;
constexpr float BOOST_VOLUME = 0.55F;
constexpr float CROWD_VOLUME = 0.22F;

// --- One-shot sounds ---
constexpr float JUMP_VOLUME = 0.55F;
constexpr float DODGE_VOLUME = 0.7F;
constexpr float IMPACT_MIN_VOLUME = 0.2F;
constexpr float HIT_MIN_VOLUME = 0.25F;
constexpr float HIT_LOW_PITCH = 0.85F;
constexpr float HIT_PITCH_RANGE = 0.35F;
constexpr float POWER_SHOT_VOLUME = 0.9F;
constexpr float BOUNCE_MIN_VOLUME = 0.1F;
constexpr float COUNTDOWN_VOLUME = 0.6F;
constexpr float KICKOFF_VOLUME = 0.7F;
constexpr float EXPLOSION_VOLUME = 1.0F;
constexpr float GOAL_VOLUME = 0.85F;
constexpr float CHEER_VOLUME = 0.7F;
constexpr float OVERTIME_VOLUME = 0.8F;
constexpr float MATCH_END_VOLUME = 0.85F;
constexpr float TICK_VOLUME = 0.45F;
/** @brief The clock ticks during its last seconds. */
constexpr int TICK_SECONDS = 10;
/** @brief The last 3 ticks are higher. */
constexpr int HIGH_TICK_SECONDS = 3;
constexpr float HIGH_TICK_PITCH = 1.25F;

constexpr int PHASE_PLAYING = static_cast<int>(protocol::MatchPhase::Playing);
constexpr int PHASE_GOAL = static_cast<int>(protocol::MatchPhase::Goal);
constexpr int PHASE_KICKOFF = static_cast<int>(protocol::MatchPhase::Kickoff);
constexpr int PHASE_ENDED = static_cast<int>(protocol::MatchPhase::Ended);

/** @brief Strength of a cue speed, from 0 to 1. */
float strength(const float speed) { return std::clamp(speed / game::SOUND_CUE_FULL_SPEED, 0.0F, 1.0F); }

/** @brief True if a replicated counter moved since `played`. */
bool changed(const int counter, const int played) { return (counter & CUE_MASK) != (played & CUE_MASK); }

audio::SoundPosition positionOf(const ecs::Transform& transform) {
    return audio::SoundPosition{.x = transform.x, .y = transform.y};
}

/** @brief Pitch of the blast of a colored shot: each color sounds a bit different. */
float shotPitch(const int shot) {
    switch (static_cast<game::Shot>(shot)) {
        case game::Shot::Purple:
            return 0.85F;
        case game::Shot::Gold:
            return 1.2F;
        default:
            return 1.0F;
    }
}

}  // namespace

std::string_view fileName(const Sound sound) noexcept {
    switch (sound) {
        case Sound::Engine:
            return "engine_loop.wav";
        case Sound::Boost:
            return "boost_loop.wav";
        case Sound::Crowd:
            return "crowd_loop.wav";
        case Sound::Jump:
            return "jump.wav";
        case Sound::DoubleJump:
            return "double_jump.wav";
        case Sound::Dodge:
            return "dodge.wav";
        case Sound::Impact:
            return "impact.wav";
        case Sound::BallHit:
            return "ball_hit.wav";
        case Sound::PowerShot:
            return "power_shot.wav";
        case Sound::Bounce:
            return "ball_bounce.wav";
        case Sound::Countdown:
            return "countdown.wav";
        case Sound::Kickoff:
            return "kickoff.wav";
        case Sound::GoalHorn:
            return "goal_horn.wav";
        case Sound::Cheer:
            return "crowd_cheer.wav";
        case Sound::Overtime:
            return "overtime.wav";
        case Sound::MatchEnd:
            return "match_end.wav";
        case Sound::ClockTick:
            return "clock_tick.wav";
        case Sound::GoalExplosion:
            return "goal_explosion.wav";
    }
    return "";
}

void SoundPlugin::build(engine::PluginBuilder& builder) {
    builder.require<audio::AudioPlugin>();
    builder.require<ecs::net::ClientReplicationPlugin>();
    builder.require<CameraPlugin>();
    builder.require<HudPlugin>();
}

void SoundPlugin::init(engine::Engine& engine) {
    auto& audioService = engine.service<engine::IAudioService>();
    audioService.setSpatialConfig(SPATIAL);
    for (std::size_t i = 0; i < SOUND_COUNT; ++i) {
        const std::string path = std::string(SOUND_DIRECTORY) + std::string(fileName(static_cast<Sound>(i)));
        _sounds.at(i) = audioService.loadSound(path);
        if (_sounds.at(i) == audio::NO_SOUND && audioService.isAvailable()) {
            rtype::log::warning(LOG_TAG, "cannot load ", path, ": this sound stays silent");
        }
    }

    ecs::Registry& registry = engine.registry();
    _listener = registry.spawnEntity();
    registry.addComponent(_listener, ecs::Transform{});
    registry.addComponent(_listener, ecs::AudioListener{});
    // The crowd plays where the listener is: centered, full volume. Only during a match.
    registry.addComponent(_listener, ecs::AudioEmitter{
                                         .sound = _sounds.at(static_cast<std::size_t>(Sound::Crowd)),
                                         .volume = CROWD_VOLUME,
                                         .loop = true,
                                         .playing = false,
                                     });

    auto& events = engine.eventBus();
    _spawned = events.subscribe<ecs::net::ReplicatedEntitySpawned>(
        [this, &engine](const ecs::net::ReplicatedEntitySpawned& event) {
            ecs::Registry& world = engine.registry();
            if (!world.hasComponent<ecs::Transform>(event.entity)) {
                return;
            }
            const ecs::Transform transform = world.getComponents<ecs::Transform>().get(event.entity);
            if (event.kind == game::KIND_CAR) {
                // Start from the counters received: what happened before this car appeared is not played.
                const auto& cues = world.getComponents<game::CarCues>();
                const game::CarCues known = cues.contains(event.entity) ? cues.get(event.entity) : game::CarCues{};
                const ecs::Entity boost = world.spawnEntity();
                world.addComponent(boost, transform);
                world.addComponent(boost, ecs::AudioEmitter{
                                              .sound = _sounds.at(static_cast<std::size_t>(Sound::Boost)),
                                              .volume = BOOST_VOLUME,
                                              .loop = true,
                                              .playing = false,
                                          });
                world.addComponent(event.entity, CarAudio{
                                                     .boost = boost,
                                                     .jumps = known.jumps,
                                                     .impacts = known.impacts,
                                                     .lastX = transform.x,
                                                     .lastY = transform.y,
                                                 });
                world.addComponent(event.entity, ecs::AudioEmitter{
                                                     .sound = _sounds.at(static_cast<std::size_t>(Sound::Engine)),
                                                     .volume = ENGINE_IDLE_VOLUME,
                                                     .pitch = ENGINE_IDLE_PITCH,
                                                     .loop = true,
                                                 });
            } else if (event.kind == game::KIND_BALL) {
                const auto& cues = world.getComponents<game::BallCues>();
                const game::BallCues known = cues.contains(event.entity) ? cues.get(event.entity) : game::BallCues{};
                world.addComponent(event.entity, BallAudio{.hits = known.hits, .bounces = known.bounces});
            }
        });
    _despawned = events.subscribe<ecs::net::ReplicatedEntityDespawned>(
        [&engine](const ecs::net::ReplicatedEntityDespawned& event) {
            ecs::Registry& world = engine.registry();
            auto& cars = world.getComponents<CarAudio>();
            if (!cars.contains(event.entity)) {
                return;
            }
            const ecs::Entity boost = cars.get(event.entity).boost;
            if (boost != 0 && world.isAlive(boost)) {
                world.killEntity(boost);
            }
        });
    _joined = events.subscribe<MatchJoined>([this](const MatchJoined& /*event*/) {
        _inMatch = true;
        _lastMatch.reset();
    });
    _left = events.subscribe<MatchLeft>([this](const MatchLeft& /*event*/) {
        _inMatch = false;
        _lastMatch.reset();
    });
}

void SoundPlugin::update(engine::Engine& engine, const ecs::time::Time& time) {
    ecs::Registry& registry = engine.registry();
    // Heard from the center of the screen.
    std::optional<audio::SoundPosition> camera;
    registry.runSystem<ecs::CameraComponent>([&camera](ecs::Entity /*entity*/, const ecs::CameraComponent& view) {
        if (!camera) {
            camera = audio::SoundPosition{.x = view.targetX, .y = view.targetY};
        }
    });
    if (camera && registry.isAlive(_listener)) {
        ecs::Transform& listener = registry.getComponents<ecs::Transform>().get(_listener);
        listener.x = camera->x;
        listener.y = camera->y;
        registry.getComponents<ecs::AudioEmitter>().get(_listener).playing = _inMatch;
    }

    updateCars(engine, time.deltaTime);
    updateBall(engine);
    updateMatch(engine);
}

void SoundPlugin::play(engine::Engine& engine, const Sound sound, const float volume, const float pitch,
                       const std::optional<audio::SoundPosition> position) const {
    engine.eventBus().publish(events::PlaySoundEvent{
        .sound = _sounds.at(static_cast<std::size_t>(sound)),
        .volume = std::clamp(volume, audio::MIN_VOLUME, audio::MAX_VOLUME),
        .pitch = pitch,
        .position = position,
    });
}

void SoundPlugin::updateCars(engine::Engine& engine, const float dt) const {
    ecs::Registry& registry = engine.registry();
    auto& transforms = registry.getComponents<ecs::Transform>();
    auto& emitters = registry.getComponents<ecs::AudioEmitter>();
    registry.runSystem<CarAudio, game::CarCues, game::CarStatus, ecs::Transform, ecs::AudioEmitter>(
        [&](ecs::Entity /*entity*/, CarAudio& track, const game::CarCues& cues, const game::CarStatus& status,
            const ecs::Transform& transform, ecs::AudioEmitter& engineSound) {
            // Engine: pitch and volume follow the speed, measured on screen and smoothed.
            const float moved = std::hypot(transform.x - track.lastX, transform.y - track.lastY);
            track.lastX = transform.x;
            track.lastY = transform.y;
            if (dt > 0.0F && moved < TELEPORT_DISTANCE) {
                const float follow = std::min(1.0F, SPEED_SMOOTHING * dt);
                track.speed += ((moved / dt) - track.speed) * follow;
            } else if (moved >= TELEPORT_DISTANCE) {
                track.speed = 0.0F;
            }
            const float revs = std::clamp(track.speed / ENGINE_FULL_SPEED, 0.0F, 1.0F);
            engineSound.volume = ENGINE_IDLE_VOLUME + ((ENGINE_FULL_VOLUME - ENGINE_IDLE_VOLUME) * revs);
            engineSound.pitch = ENGINE_IDLE_PITCH + ((ENGINE_FULL_PITCH - ENGINE_IDLE_PITCH) * revs);

            // Boost: its own entity, which follows the car.
            if (track.boost != 0 && transforms.contains(track.boost) && emitters.contains(track.boost)) {
                ecs::Transform& boost = transforms.get(track.boost);
                boost.x = transform.x;
                boost.y = transform.y;
                emitters.get(track.boost).playing = status.boosting;
            }

            const audio::SoundPosition where = positionOf(transform);
            if (changed(cues.jumps, track.jumps)) {
                track.jumps = cues.jumps;
                switch (static_cast<game::JumpKind>(cues.jumpKind)) {
                    case game::JumpKind::Dodge:
                        play(engine, Sound::Dodge, DODGE_VOLUME, 1.0F, where);
                        break;
                    case game::JumpKind::DoubleJump:
                        play(engine, Sound::DoubleJump, JUMP_VOLUME, 1.0F, where);
                        break;
                    default:
                        play(engine, Sound::Jump, JUMP_VOLUME, 1.0F, where);
                        break;
                }
            }
            if (changed(cues.impacts, track.impacts)) {
                track.impacts = cues.impacts;
                const float hard = strength(cues.impactSpeed);
                play(engine, Sound::Impact, IMPACT_MIN_VOLUME + ((1.0F - IMPACT_MIN_VOLUME) * hard),
                     1.1F - (0.2F * hard), where);
            }
        });
}

void SoundPlugin::updateBall(engine::Engine& engine) const {
    engine.registry().runSystem<BallAudio, game::BallCues, game::ShotGlow, ecs::Transform>(
        [&](ecs::Entity /*entity*/, BallAudio& track, const game::BallCues& cues, const game::ShotGlow& glow,
            const ecs::Transform& transform) {
            const audio::SoundPosition where = positionOf(transform);
            if (changed(cues.hits, track.hits)) {
                track.hits = cues.hits;
                const float hard = strength(cues.hitSpeed);
                play(engine, Sound::BallHit, HIT_MIN_VOLUME + ((1.0F - HIT_MIN_VOLUME) * hard),
                     HIT_LOW_PITCH + (HIT_PITCH_RANGE * hard), where);
                // Every hit sets the glow (a plain one turns it off): a glowing ball after a new hit is a colored shot.
                if (glow.shot != static_cast<int>(game::Shot::None)) {
                    play(engine, Sound::PowerShot, POWER_SHOT_VOLUME, shotPitch(glow.shot), where);
                }
            }
            if (changed(cues.bounces, track.bounces)) {
                track.bounces = cues.bounces;
                const float hard = strength(cues.bounceSpeed);
                play(engine, Sound::Bounce, BOUNCE_MIN_VOLUME + ((1.0F - BOUNCE_MIN_VOLUME) * hard),
                     0.9F + (0.2F * hard), where);
            }
        });
}

void SoundPlugin::updateMatch(engine::Engine& engine) {
    std::optional<game::MatchState> current;
    engine.registry().runSystem<game::MatchState>(
        [&current](ecs::Entity /*entity*/, const game::MatchState& state) { current = state; });
    if (!_inMatch || !current || current->phase == game::NO_MATCH) {
        return;
    }
    const game::MatchState previous = _lastMatch.value_or(*current);
    _lastMatch = current;
    const game::MatchState& now = *current;

    if (now.phase == PHASE_KICKOFF && now.countdown > 0 &&
        (previous.phase != PHASE_KICKOFF || previous.countdown != now.countdown)) {
        play(engine, Sound::Countdown, COUNTDOWN_VOLUME, 1.0F);
    }
    if (previous.phase == PHASE_KICKOFF && now.phase == PHASE_PLAYING) {
        play(engine, Sound::Kickoff, KICKOFF_VOLUME, 1.0F);
    }
    if (previous.phase != PHASE_GOAL && now.phase == PHASE_GOAL) {
        play(engine, Sound::GoalExplosion, EXPLOSION_VOLUME, 1.0F);
        play(engine, Sound::GoalHorn, GOAL_VOLUME, 1.0F);
        play(engine, Sound::Cheer, CHEER_VOLUME, 1.0F);
    }
    if (!previous.overtime && now.overtime) {
        play(engine, Sound::Overtime, OVERTIME_VOLUME, 1.0F);
    }
    if (previous.phase != PHASE_ENDED && now.phase == PHASE_ENDED) {
        play(engine, Sound::MatchEnd, MATCH_END_VOLUME, 1.0F);
        play(engine, Sound::Cheer, CHEER_VOLUME, 1.0F);
    }
    // The last seconds of regular time tick (not in overtime, where the clock counts up).
    if (now.phase == PHASE_PLAYING && !now.overtime && now.clock < previous.clock && now.clock > 0 &&
        now.clock <= TICK_SECONDS) {
        play(engine, Sound::ClockTick, TICK_VOLUME, now.clock <= HIGH_TICK_SECONDS ? HIGH_TICK_PITCH : 1.0F);
    }
}

void SoundPlugin::shutdown(engine::Engine& engine) {
    auto& events = engine.eventBus();
    if (_spawned) {
        events.unsubscribe<ecs::net::ReplicatedEntitySpawned>(*_spawned);
        _spawned.reset();
    }
    if (_despawned) {
        events.unsubscribe<ecs::net::ReplicatedEntityDespawned>(*_despawned);
        _despawned.reset();
    }
    if (_joined) {
        events.unsubscribe<MatchJoined>(*_joined);
        _joined.reset();
    }
    if (_left) {
        events.unsubscribe<MatchLeft>(*_left);
        _left.reset();
    }
    if (engine.mediator().hasService<engine::IAudioService>()) {
        auto& audioService = engine.service<engine::IAudioService>();
        for (const audio::SoundId sound : _sounds) {
            audioService.unloadSound(sound);
        }
    }
    _sounds.fill(audio::NO_SOUND);
}

}  // namespace rlss::client
