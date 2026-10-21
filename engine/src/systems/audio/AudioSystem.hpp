#pragma once

#include <cstddef>
#include <optional>
#include <unordered_map>
#include <variant>
#include <vector>

#include "audio/IAudioBackend.hpp"
#include "audio/common/AudioConstants.hpp"
#include "audio/common/AudioTypes.hpp"
#include "audio/common/SoundParams.hpp"
#include "audio/common/SpatialConfig.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "events/AudioEvents.hpp"
#include "events/EventBus.hpp"
#include "utils/MoveOnlyFunction.hpp"

namespace systems {

/**
 * @brief Plays the sounds and musics requested through the `EventBus`.
 *
 * Gameplay code publishes audio events (`PlaySoundEvent`, `PlayMusicEvent`, ...) and never touches the audio
 * library. Listeners only queue the events, so publishing stays cheap and never blocks; `update()` then executes
 * them in arrival order, once per frame. Works the same with a muted backend.
 *
 * Gains are split in three buses: master, music and SFX. A sound plays at
 * `event volume x SFX volume x distance attenuation`, and the master volume is applied by the backend on top.
 * Sounds with a position are panned and attenuated around the first active `AudioListener` entity, and entities
 * with an `AudioEmitter` emit a sound that follows them while it plays.
 *
 * The backend and the event bus must outlive the system.
 */
class AudioSystem {
  public:
    /**
     * @brief Subscribes to the audio events.
     *
     * @param backend Audio output used to play the sounds.
     * @param eventBus Bus the audio events are published on.
     */
    AudioSystem(audio::IAudioBackend& backend, ecs::EventBus& eventBus);

    /** @brief Unsubscribes from the audio events and stops the sounds of the emitters. */
    ~AudioSystem();

    /** @brief Deleted copy constructor: the event listeners point to this instance. */
    AudioSystem(const AudioSystem&) = delete;
    /** @brief Deleted copy assignment operator. */
    AudioSystem& operator=(const AudioSystem&) = delete;
    /** @brief Deleted move constructor. */
    AudioSystem(AudioSystem&&) = delete;
    /** @brief Deleted move assignment operator. */
    AudioSystem& operator=(AudioSystem&&) = delete;

    /**
     * @brief Executes the queued audio events, updates the emitters, then refills the music stream.
     *
     * Call it once per frame, after the entities have moved.
     *
     * Sounds requested during the frame start after the other events, so a `StopAllSoundsEvent` drops the sounds
     * requested before it in the same frame.
     *
     * @param registry Registry holding the `AudioListener` and `AudioEmitter` entities.
     */
    void update(ecs::Registry& registry);

    /**
     * @brief Sets the gain of a bus immediately.
     *
     * @param bus Bus to change.
     * @param volume Linear gain, clamped to [`MIN_VOLUME`, `MAX_VOLUME`].
     */
    void setVolume(audio::AudioBus bus, float volume);

    /**
     * @brief Reads the gain of a bus.
     * @param bus Bus to read.
     * @return Linear gain in [`MIN_VOLUME`, `MAX_VOLUME`].
     */
    [[nodiscard]] float volume(audio::AudioBus bus) const noexcept;

    /**
     * @brief Changes how positioned sounds are panned and attenuated.
     * @param config Pan and attenuation settings, in world units.
     */
    void setSpatialConfig(const audio::SpatialConfig& config) noexcept { _spatial = config; }

    /** @brief Reads the pan and attenuation settings. */
    [[nodiscard]] const audio::SpatialConfig& spatialConfig() const noexcept { return _spatial; }

  private:
    /** @brief Any audio event waiting for the next update. */
    using AudioCommand = std::variant<events::PlaySoundEvent, events::PlayMusicEvent, events::StopMusicEvent,
                                      events::StopAllSoundsEvent, events::SetAudioVolumeEvent>;

    /** @brief A sound to start at the end of the current update, with its final settings. */
    struct PendingSound {
        /** @brief Sound to play. */
        audio::SoundId id = audio::NO_SOUND;
        /** @brief Final volume, pan and pitch. */
        audio::SoundParams params;
    };

    /** @brief Playback state of an `AudioEmitter` entity. */
    struct EmitterState {
        /** @brief Instance currently or last played, or `NO_VOICE`. */
        audio::VoiceId voice = audio::NO_VOICE;
        /** @brief Sound of that instance, to restart it if the component changes sound. */
        audio::SoundId sound = audio::NO_SOUND;
        /** @brief Whether the sound was started once, so a non-looping emitter plays only once. */
        bool started = false;
        /** @brief Whether the entity was still an emitter during the current update. */
        bool seen = false;
    };

    /** @brief Combines a sound volume with the SFX bus and, when positioned, the listener's pan and attenuation. */
    [[nodiscard]] audio::SoundParams resolveParams(float volume, float pitch,
                                                   const std::optional<audio::SoundPosition>& position) const;

    /** @brief Starts, follows and stops the sounds of the `AudioEmitter` entities. */
    void updateEmitters(ecs::Registry& registry);

    /** @brief Queues a sound request, unless it is invalid or the queue is full. */
    void enqueue(const events::PlaySoundEvent& event);

    /** @brief Queues any other audio event; these are never dropped. */
    void enqueue(const AudioCommand& command);

    /** @brief Resolves a sound's final settings and keeps it for the end of the update. */
    void execute(const events::PlaySoundEvent& event);
    /** @brief Starts a music stream. */
    void execute(const events::PlayMusicEvent& event);
    /** @brief Stops the music stream. */
    void execute(const events::StopMusicEvent& event);
    /** @brief Stops every sound effect, including the ones requested earlier in this update. */
    void execute(const events::StopAllSoundsEvent& event);
    /** @brief Changes a bus gain. */
    void execute(const events::SetAudioVolumeEvent& event);

    /** @brief Audio output; never null. */
    audio::IAudioBackend* _backend;
    /** @brief Undo the event subscriptions when the system is destroyed. */
    std::vector<ecs::MoveOnlyFunction<void()>> _unsubscribers;
    /** @brief Events received since the last update, in arrival order. */
    std::vector<AudioCommand> _queue;
    /** @brief Number of `PlaySoundEvent` in `_queue`, bounded by `MAX_QUEUED_SOUNDS`. */
    std::size_t _queuedSounds = 0;
    /** @brief Sounds to start at the end of the current update, at most one per sound. */
    std::vector<PendingSound> _pendingSounds;
    /** @brief Playback state of each emitter entity. */
    std::unordered_map<ecs::Entity, EmitterState> _emitters;
    /** @brief Where sounds are heard from during the current update, if a listener exists. */
    std::optional<audio::SoundPosition> _listener;
    /** @brief Pan and attenuation settings. */
    audio::SpatialConfig _spatial;
    /** @brief Global gain. */
    float _masterVolume = audio::DEFAULT_VOLUME;
    /** @brief Music gain. */
    float _musicVolume = audio::DEFAULT_VOLUME;
    /** @brief Sound effect gain. */
    float _sfxVolume = audio::DEFAULT_VOLUME;
};

}  // namespace systems
