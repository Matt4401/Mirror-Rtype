/**
 * @file TimelinePlugin.hpp
 * @brief Engine plugin moving a Timeline cursor at a set speed and publishing the events it reaches.
 */

#pragma once

#include <string_view>

#include "core/IPlugin.hpp"
#include "ecs/time/Time.hpp"
#include "timeline/Timeline.hpp"

namespace timeline {

/**
 * @brief Plays a Timeline: moves its cursor by `speed * dt` every fixed step and publishes what it reaches.
 *
 * For each event reached, publishes `TimelineEventFired` on the engine EventBus; when the last pending event fires,
 * publishes `TimelineFinished`. That is once per play-through: after a `rewind()` or a new timeline it can fire again,
 * but events added once the timeline has finished fire without a new `TimelineFinished`. The plugin gives no meaning to
 * events: the game subscribes and handles each type (spawning entities, changing the speed, ending the level...). It
 * runs in `fixedUpdate`, so playback depends only on simulated time, never on the frame rate. Headless.
 *
 * @code
 * auto& player = engine.getPlugin<timeline::TimelinePlugin>();
 * player.timeline().add({.position = 400.0F, .type = "spawn", .params = {{"prefab", std::string("enemy")}}});
 * player.setSpeed(60.0F);  // cursor units per second, e.g. scroll pixels
 * @endcode
 */
class TimelinePlugin final : public engine::IPlugin {
  public:
    /** @brief Constructs the plugin with an empty timeline, stopped. */
    TimelinePlugin() = default;

    /** @brief Destructor. */
    ~TimelinePlugin() override = default;

    /** @brief Deleted copy constructor. */
    TimelinePlugin(const TimelinePlugin&) = delete;
    /** @brief Deleted copy assignment operator. */
    TimelinePlugin& operator=(const TimelinePlugin&) = delete;
    /** @brief Deleted move constructor. */
    TimelinePlugin(TimelinePlugin&&) = delete;
    /** @brief Deleted move assignment operator. */
    TimelinePlugin& operator=(TimelinePlugin&&) = delete;

    /**
     * @brief Gets plugin name.
     * @return String view "TimelinePlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "TimelinePlugin"; }

    /**
     * @brief Moves the cursor by `speed * dt` and publishes the events reached.
     * @param engine Reference to host Engine.
     * @param time Fixed step timing.
     */
    void fixedUpdate(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Moves the cursor by a given distance right now and publishes the events reached.
     *
     * Useful to start a level at a checkpoint, or for tests.
     *
     * @param engine Reference to host Engine.
     * @param delta Distance to move by.
     */
    void step(engine::Engine& engine, float delta);

    /**
     * @brief Sets how fast the cursor moves.
     * @param unitsPerSecond Cursor units per second; 0 pauses playback, negative or non-finite values are ignored.
     */
    void setSpeed(float unitsPerSecond) noexcept;

    /**
     * @brief Gets how fast the cursor moves.
     * @return Cursor units per second.
     */
    [[nodiscard]] float speed() const noexcept { return _speed; }

    /**
     * @brief Gets the timeline being played, to fill or inspect it.
     * @return The timeline.
     */
    [[nodiscard]] Timeline& timeline() noexcept { return _timeline; }

    /**
     * @brief Gets the timeline being played.
     * @return The timeline.
     */
    [[nodiscard]] const Timeline& timeline() const noexcept { return _timeline; }

  private:
    /** @brief Events being played. */
    Timeline _timeline;
    /** @brief Cursor units per second. */
    float _speed = 0.0F;
};

}  // namespace timeline
