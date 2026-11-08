/**
 * @file SoundPlugin.hpp
 * @brief Client plugin playing the gameplay sounds: engines, boost, jumps, impacts, ball hits and bounces, match
 * events.
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "audio/types/AudioTypes.hpp"
#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "events/EventBus.hpp"
#include "game/Components.hpp"

namespace ecs {
class Registry;
}

namespace ecs::time {
struct Time;
}

namespace rlss::client {

/** @brief Directory of the sound files, relative to the working directory (the repository root, like the scripts). */
inline constexpr std::string_view SOUND_DIRECTORY = "rlss/assets/sounds/";

/** @brief Every sound of the game; `fileName()` gives its file in `SOUND_DIRECTORY`. */
enum class Sound : std::uint8_t {
    /** @brief Engine hum of a car (loop), pitched with its speed. */
    Engine,
    /** @brief Boost roar (loop), while a car boosts. */
    Boost,
    /** @brief Crowd murmur (loop), all through a match. */
    Crowd,
    /** @brief First jump. */
    Jump,
    /** @brief Double jump, or jump off the ball. */
    DoubleJump,
    /** @brief Flip. */
    Dodge,
    /** @brief A car landing hard, or its body hitting a wall. */
    Impact,
    /** @brief A car hitting the ball. */
    BallHit,
    /** @brief Colored shot (red, purple, gold), on top of the hit. */
    PowerShot,
    /** @brief The ball bouncing off the arena. */
    Bounce,
    /** @brief One beep of the kickoff countdown (3, 2, 1). */
    Countdown,
    /** @brief End of the countdown: the ball is in play. */
    Kickoff,
    /** @brief Goal horn. */
    GoalHorn,
    /** @brief Crowd cheering, after a goal and at the end of the match. */
    Cheer,
    /** @brief Time is up on a tied score: overtime. */
    Overtime,
    /** @brief The match is over. */
    MatchEnd,
    /** @brief One tick of the last seconds of the clock. */
    ClockTick,
    /** @brief The ball exploding in the goal. */
    GoalExplosion,
};

/** @brief Number of `Sound` values. */
inline constexpr std::size_t SOUND_COUNT = static_cast<std::size_t>(Sound::GoalExplosion) + 1;

/**
 * @brief File name of a sound, in `SOUND_DIRECTORY`.
 * @param sound Sound.
 * @return File name, such as "ball_hit.wav".
 */
[[nodiscard]] std::string_view fileName(Sound sound) noexcept;

/** @brief Client: what the sounds of a car last heard, kept on the car entity by `SoundPlugin`. */
struct CarAudio {
    /** @brief Entity playing the boost roar (an emitter follows its own entity, the car already plays its engine). */
    ecs::Entity boost = 0;
    /** @brief `CarCues::jumps` already played. */
    int jumps = 0;
    /** @brief `CarCues::impacts` already played. */
    int impacts = 0;
    /** @brief Position on the previous frame, to measure the speed. */
    float lastX = 0.0F;
    /** @brief Position on the previous frame, to measure the speed. */
    float lastY = 0.0F;
    /** @brief Smoothed speed, in uu/s: pitches the engine. */
    float speed = 0.0F;
};

/** @brief Client: what the sounds of the ball last heard, kept on the ball entity by `SoundPlugin`. */
struct BallAudio {
    /** @brief `BallCues::hits` already played. */
    int hits = 0;
    /** @brief `BallCues::bounces` already played. */
    int bounces = 0;
};

/**
 * @brief Plays the sounds of the match through the engine audio module (`audio::AudioPlugin`).
 *
 * Everything heard comes from the server's state, the client never guesses an event:
 * - each car plays its engine (pitched with its speed) and, while it boosts, the boost roar: looping
 *   `ecs::AudioEmitter`s that follow the cars;
 * - the `CarCues` and `BallCues` counters, replicated with the snapshots, play the jumps, flips, impacts, hits and
 *   bounces where they happen, louder when stronger; a colored shot adds its own blast;
 * - the `MatchState` written by `HudPlugin` plays the countdown, the kickoff, the goals, the overtime, the
 *   end of the match and the last seconds of the clock; a crowd murmurs during the whole match.
 *
 * Positioned sounds are heard from the camera (an `ecs::AudioListener` follows its target): the cars and the ball
 * pan left and right with their place on screen and fade out when they are far away. A missing sound file only
 * mutes that sound.
 */
class SoundPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Gets plugin name.
     * @return String view "SoundPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "SoundPlugin"; }

    /**
     * @brief Declares the dependencies on the audio, client replication, camera and scoreboard plugins.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Loads the sounds, sets the spatialization, spawns the listener and subscribes to the events.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Moves the listener with the camera, updates the loops and plays the new events.
     * @param engine Host engine.
     * @param time Frame time.
     */
    void update(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Unsubscribes from the events and releases the sounds.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

  private:
    /**
     * @brief Requests a sound.
     * @param engine Host engine.
     * @param sound Sound to play.
     * @param volume Gain of this instance.
     * @param pitch Playback speed factor.
     * @param position Where it happens, or nothing for a sound heard everywhere.
     */
    void play(engine::Engine& engine, Sound sound, float volume, float pitch,
              std::optional<audio::SoundPosition> position = std::nullopt) const;

    /**
     * @brief Engine and boost loops of every car, and the sounds of their new jumps and impacts.
     * @param engine Host engine.
     * @param dt Frame time, in seconds.
     */
    void updateCars(engine::Engine& engine, float dt) const;

    /**
     * @brief Sounds of the new hits, colored shots and bounces of the ball.
     * @param engine Host engine.
     */
    void updateBall(engine::Engine& engine) const;

    /**
     * @brief Sounds of the changes of the match state: countdown, kickoff, goal, overtime, end, clock.
     * @param engine Host engine.
     */
    void updateMatch(engine::Engine& engine);

    /** @brief Loaded sounds, `audio::NO_SOUND` for those that could not be loaded. */
    std::array<audio::SoundId, SOUND_COUNT> _sounds{};
    /** @brief Entity hearing the sounds, following the camera; it also plays the crowd. */
    ecs::Entity _listener = 0;
    /** @brief True between `MatchJoined` and `MatchLeft`. */
    bool _inMatch = false;
    /** @brief Match state of the previous frame, or nothing right after joining. */
    std::optional<game::MatchState> _lastMatch;
    /** @brief Subscription to `ReplicatedEntitySpawned`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _spawned;
    /** @brief Subscription to `ReplicatedEntityDespawned`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _despawned;
    /** @brief Subscription to `MatchJoined`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _joined;
    /** @brief Subscription to `MatchLeft`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _left;
};

}  // namespace rlss::client
