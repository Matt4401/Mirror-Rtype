/**
 * @file Engine.cpp
 * @brief Implementation of Engine coordinator and game loop.
 */

#include "core/Engine.hpp"

#include <utility>

#include "core/IPlugin.hpp"
#include "core/plugins/CorePlugin.hpp"
#include "core/plugins/TimePlugin.hpp"
#include "core/services/IWindowService.hpp"
#include "ecs/time/Clock.hpp"
#include "ecs/time/Time.hpp"

namespace engine {

Engine::Engine(ecs::time::Clock::TimeSource timeSource) : _clock(std::move(timeSource)) {}

Engine::~Engine() {
    if (_initialized) {
        shutdown();
    }
}

void Engine::init() {
    if (_initialized) {
        return;
    }

    if (!hasPlugin<CorePlugin>()) {
        addPlugin<CorePlugin>();
    }
    if (!hasPlugin<TimePlugin>()) {
        addPlugin<TimePlugin>();
    }

    _pluginGraph.resolve();

    for (IPlugin* plugin : _pluginGraph.executionOrder()) {
        plugin->init(*this);
    }

    _initialized = true;
}

void Engine::step(float dt) {
    if (!_initialized) {
        init();
    }

    const ecs::time::Time fixedTime{
        .deltaTime = _fixedDt,
        .totalElapsedTime = _clock.time().totalElapsedTime + _fixedDt,
        .tickCount = _clock.time().tickCount + 1,
    };

    for (IPlugin* plugin : _pluginGraph.executionOrder()) {
        plugin->fixedUpdate(*this, fixedTime);
    }

    const ecs::time::Time frameTime{
        .deltaTime = dt,
        .totalElapsedTime = fixedTime.totalElapsedTime,
        .tickCount = fixedTime.tickCount,
    };

    for (IPlugin* plugin : _pluginGraph.executionOrder()) {
        plugin->update(*this, frameTime);
    }

    const auto windowService = _mediator.getService<IWindowService>();
    if (!windowService || !windowService->get().shouldClose()) {
        for (IPlugin* plugin : _pluginGraph.executionOrder()) {
            plugin->render(*this);
        }
    }
}

void Engine::run() {
    if (!_initialized) {
        init();
    }

    _running = true;

    while (_running) {
        const auto windowService = _mediator.getService<IWindowService>();
        if (windowService && windowService->get().shouldClose()) {
            _running = false;
            break;
        }

        _clock.tick(_fixedDt, [&](const ecs::time::Time& fixedTime) {
            for (IPlugin* plugin : _pluginGraph.executionOrder()) {
                plugin->fixedUpdate(*this, fixedTime);
            }
        });

        const ecs::time::Time frameTime{
            .deltaTime = _clock.lastFrameDelta(),
            .totalElapsedTime = _clock.time().totalElapsedTime,
            .tickCount = _clock.time().tickCount,
        };

        for (IPlugin* plugin : _pluginGraph.executionOrder()) {
            plugin->update(*this, frameTime);
        }

        for (IPlugin* plugin : _pluginGraph.executionOrder()) {
            plugin->render(*this);
        }
    }

    shutdown();
}

void Engine::shutdown() {
    if (!_initialized) {
        return;
    }

    for (IPlugin* plugin : _pluginGraph.shutdownOrder()) {
        plugin->shutdown(*this);
    }

    _initialized = false;
}

}  // namespace engine
