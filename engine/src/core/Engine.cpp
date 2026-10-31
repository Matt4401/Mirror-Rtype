/**
 * @file Engine.cpp
 * @brief Implementation of Engine coordinator and game loop.
 */

#include "core/Engine.hpp"

#include <memory>
#include <ranges>
#include <typeindex>
#include <utility>
#include <vector>

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

void Engine::ensureDefaultPlugins() {
    if (!hasPlugin<CorePlugin>()) {
        _pluginGraph.addPlugin(std::make_unique<CorePlugin>(), std::type_index(typeid(CorePlugin)), 2);
    }
    if (!hasPlugin<TimePlugin>()) {
        _pluginGraph.addPlugin(std::make_unique<TimePlugin>(), std::type_index(typeid(TimePlugin)), 1);
    }
}

void Engine::init() {
    if (_initialized) {
        return;
    }

    ensureDefaultPlugins();
    _pluginGraph.resolve();

    std::vector<IPlugin*> initialized;
    initialized.reserve(_pluginGraph.executionOrder().size());
    try {
        for (IPlugin* plugin : _pluginGraph.executionOrder()) {
            plugin->init(*this);
            initialized.push_back(plugin);
        }
    } catch (...) {
        for (IPlugin* plugin : std::views::reverse(initialized)) {
            plugin->shutdown(*this);
        }
        throw;
    }

    _initialized = true;
}

void Engine::step(float dt) {
    if (!_initialized) {
        init();
    }

    for (IPlugin* plugin : _pluginGraph.executionOrder()) {
        plugin->preUpdate(*this);
    }

    _clock.advance(_fixedDt);
    const ecs::time::Time fixedTime = _clock.time();

    for (IPlugin* plugin : _pluginGraph.executionOrder()) {
        plugin->fixedUpdate(*this, fixedTime);
    }

    const ecs::time::Time frameTime = _clock.frameTime(dt);

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

        for (IPlugin* plugin : _pluginGraph.executionOrder()) {
            plugin->preUpdate(*this);
        }

        _clock.tick(_fixedDt, [&](const ecs::time::Time& fixedTime) {
            for (IPlugin* plugin : _pluginGraph.executionOrder()) {
                plugin->fixedUpdate(*this, fixedTime);
            }
        });

        const ecs::time::Time frameTime = _clock.frameTime();

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
