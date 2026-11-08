#include "settings/SettingsPlugin.hpp"

#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "config/Settings.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "core/plugins/CorePlugin.hpp"
#include "ecs/time/Time.hpp"
#include "events/EventBus.hpp"
#include "settings/SettingsEvents.hpp"

namespace settings {

SettingsPlugin::SettingsPlugin(std::filesystem::path file, float saveDelay)
    : _settings(std::make_shared<config::Settings>(std::move(file))), _saveDelay(saveDelay) {}

void SettingsPlugin::build(engine::PluginBuilder& builder) {
    builder.require<engine::CorePlugin>();
    builder.provides<config::Settings>();
}

void SettingsPlugin::init(engine::Engine& engine) {
    if (auto loaded = _settings->load(); !loaded) {
        _lastError = loaded.error();
        std::cerr << "[Settings] " << _lastError << " (defaults are used)\n";
    }
    for (const std::string& warning : _settings->warnings()) {
        std::cerr << "[Settings] " << warning << "\n";
    }
    if (!engine.mediator().registerService<config::Settings>(_settings)) {
        throw std::logic_error("SettingsPlugin: a Settings service is already registered");
    }
}

void SettingsPlugin::update(engine::Engine& engine, const ecs::time::Time& time) {
    ecs::EventBus& bus = engine.eventBus();
    bool changed = false;
    _settings->drainChanged([&bus, &changed](std::string_view key) {
        changed = true;
        bus.publish(SettingChanged{.key = std::string(key)});
    });
    if (changed) {
        _sinceChange = 0.0F;
    }
    if (!_settings->dirty()) {
        return;
    }
    _sinceChange += time.deltaTime;
    if (_sinceChange >= _saveDelay) {
        save();
    }
}

void SettingsPlugin::shutdown(engine::Engine& engine) {
    if (_settings->dirty()) {
        save();
    }
    engine.mediator().removeService<config::Settings>();
}

void SettingsPlugin::save() {
    _sinceChange = 0.0F;
    if (auto saved = _settings->save(); !saved) {
        _lastError = saved.error();
        std::cerr << "[Settings] " << _lastError << "\n";
    }
}

}  // namespace settings
