#pragma once

#include <cstddef>
#include <variant>
#include <vector>

#include "audio/IAudioBackend.hpp"
#include "audio/common/AudioConstants.hpp"
#include "audio/common/AudioTypes.hpp"
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
 * Gains are split in three buses: master, music and SFX. A sound plays at `event volume x SFX volume`, and the
 * master volume is applied by the backend on top of everything.
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

    /** @brief Unsubscribes from the audio events. */
    ~AudioSystem();

    /** @brief Deleted copy constructor: the event listeners point to this instance. */
    AudioSystem(const AudioSystem&) = delete;
    /** @brief Deleted copy assignment operator. */
    AudioSystem& operator=(const AudioSystem&) = delete;
    /** @brief Deleted move constructor. */
    AudioSystem(AudioSystem&&) = delete;
    /** @brief Deleted move assignment operator. */
    AudioSystem& operator=(AudioSystem&&) = delete;

    /** @brief Executes the queued audio events, then refills the music stream; call it once per frame. */
    void update();

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

  private:
    /** @brief Any audio event waiting for the next update. */
    using AudioCommand = std::variant<events::PlaySoundEvent, events::PlayMusicEvent, events::StopMusicEvent,
                                      events::StopAllSoundsEvent, events::SetAudioVolumeEvent>;

    /** @brief Queues a sound request, unless it is invalid or the queue is full. */
    void enqueue(const events::PlaySoundEvent& event);

    /** @brief Queues any other audio event; these are never dropped. */
    void enqueue(const AudioCommand& command);

    /** @brief Plays a sound, unless it was already played during this update. */
    void execute(const events::PlaySoundEvent& event);
    /** @brief Starts a music stream. */
    void execute(const events::PlayMusicEvent& event);
    /** @brief Stops the music stream. */
    void execute(const events::StopMusicEvent& event);
    /** @brief Stops every sound effect. */
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
    /** @brief Sounds already played during the current update, to merge duplicates. */
    std::vector<audio::SoundId> _playedThisUpdate;
    /** @brief Global gain. */
    float _masterVolume = audio::DEFAULT_VOLUME;
    /** @brief Music gain. */
    float _musicVolume = audio::DEFAULT_VOLUME;
    /** @brief Sound effect gain. */
    float _sfxVolume = audio::DEFAULT_VOLUME;
};

}  // namespace systems
