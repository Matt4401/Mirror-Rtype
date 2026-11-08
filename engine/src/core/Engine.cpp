/**
 * @file Engine.cpp
 * @brief Implementation of Engine coordinator and game loop.
 */

#include "core/Engine.hpp"

#include <cstddef>
#include <exception>
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
        try {
            shutdown();
        } catch (...) {  // NOLINT(bugprone-empty-catch): a destructor cannot report it; call shutdown() to see it.
        }
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
    if (_initialized || _shutdown) {
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

void Engine::run() {
    if (_shutdown) {
        return;
    }
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

        advance();

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

    // Every plugin shuts down even if an earlier one throws; the first error is rethrown at the end.
    std::exception_ptr error;
    for (IPlugin* plugin : _pluginGraph.shutdownOrder()) {
        try {
            plugin->shutdown(*this);
        } catch (...) {
            if (!error) {
                error = std::current_exception();
            }
        }
    }

    _initialized = false;
    _shutdown = true;
    if (error) {
        std::rethrow_exception(error);
    }
}

std::size_t Engine::advance() {
    if (_shutdown) {
        return 0;
    }
    if (!_initialized) {
        init();
    }
    for (IPlugin* plugin : _pluginGraph.executionOrder()) {
        plugin->preUpdate(*this);
    }
    const std::size_t steps = _clock.tick(_fixedDt, [&](const ecs::time::Time& fixedTime) {
        for (IPlugin* plugin : _pluginGraph.executionOrder()) {
            plugin->fixedUpdate(*this, fixedTime);
        }
    });
    const ecs::time::Time frameTime = _clock.frameTime();
    for (IPlugin* plugin : _pluginGraph.executionOrder()) {
        plugin->update(*this, frameTime);
    }
    for (IPlugin* plugin : _pluginGraph.executionOrder()) {
        plugin->lateUpdate(*this, frameTime);
    }
    return steps;
}

}  // namespace engine
