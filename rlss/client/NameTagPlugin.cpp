#include "NameTagPlugin.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "Arena.hpp"
#include "CameraPlugin.hpp"
#include "ClientEvents.hpp"
#include "PixelFont.hpp"
#include "UiCanvas.hpp"
#include "components/CameraComponent.hpp"
#include "components/Renderable.hpp"
#include "components/Transform.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "game/Components.hpp"
#include "net/ClientReplicationPlugin.hpp"
#include "net/NetworkEntityMap.hpp"
#include "net/NetworkEvents.hpp"
#include "net/NetworkPlugin.hpp"
#include "protocol/Protocol.hpp"
#include "scripting/ScriptPlugin.hpp"

namespace rlss::client {

namespace {

/** @brief Above the world and the scoreboard, below the menus. */
constexpr int LAYER_BACKGROUND = 150;
constexpr int LAYER_TEXT = 151;

constexpr ecs::Rgba BACKGROUND_COLOR{.r = 8, .g = 12, .b = 22, .a = 170};
/** @brief Side of one square of a letter, in pixels: the label keeps its size whatever the zoom. */
constexpr float SQUARE = 2.0F;
/** @brief Space around the letters, in pixels. */
constexpr float PADDING = 4.0F;
/** @brief The bottom of the label is this far above the car center, in world units: above the boost gauge. */
constexpr float HEIGHT_ABOVE_CAR = 82.0F;
/** @brief The player color is mixed with this much white, to read well on the dark label. */
constexpr float WHITE_MIX = 0.45F;

ecs::Rgba lighter(const Rgb color) {
    const auto mixed = [](const std::uint8_t channel) {
        const auto value = static_cast<float>(channel);
        return static_cast<std::uint8_t>(value + ((255.0F - value) * WHITE_MIX));
    };
    return ecs::Rgba{.r = mixed(color.r), .g = mixed(color.g), .b = mixed(color.b), .a = 255};
}

}  // namespace

void NameTagPlugin::build(engine::PluginBuilder& builder) {
    builder.require<ecs::net::NetworkPlugin>();
    builder.require<ecs::net::ClientReplicationPlugin>();
    // The camera script runs in the ScriptPlugin update: the labels are placed after it, with the final camera.
    builder.require<scripting::ScriptPlugin>();
    builder.require<CameraPlugin>();
}

void NameTagPlugin::init(engine::Engine& engine) {
    auto& events = engine.eventBus();
    _packets = events.subscribe<ecs::net::PacketReceived>([this](const ecs::net::PacketReceived& packet) {
        const auto message = protocol::decode(packet.data);
        const auto* names = message ? std::get_if<protocol::PlayerNames>(&*message) : nullptr;
        if (names == nullptr || !_inMatch) {
            return;
        }
        _names.clear();
        for (const protocol::PlayerName& player : names->players) {
            _names.insert_or_assign(player.carId, player.name);
        }
    });
    _joined = events.subscribe<MatchJoined>([this](const MatchJoined& /*event*/) { _inMatch = true; });
    _left = events.subscribe<MatchLeft>([this, &engine](const MatchLeft& /*event*/) {
        _inMatch = false;
        _names.clear();
        clearTags(engine);
    });
}

void NameTagPlugin::update(engine::Engine& engine, const ecs::time::Time& /*time*/) {
    ecs::Registry& registry = engine.registry();
    const auto& map = registry.getResource<ecs::net::NetworkEntityMap>();
    auto& transforms = registry.getComponents<ecs::Transform>();

    // Drop the labels whose name or car is gone, or whose name changed.
    std::vector<std::uint32_t> stale;
    for (const auto& [carId, tag] : _tags) {
        const auto name = _names.find(carId);
        const auto car = map.entityOf(carId);
        if (name == _names.end() || name->second != tag.name || !car || *car != tag.car || !transforms.contains(*car)) {
            stale.push_back(carId);
        }
    }
    for (const std::uint32_t carId : stale) {
        _tags.at(carId).canvas.clear(registry);
        _tags.erase(carId);
    }

    // Build the missing ones, once their car is there.
    const auto& looks = registry.getComponents<game::CarLook>();
    for (const auto& [carId, name] : _names) {
        const auto car = map.entityOf(carId);
        if (_tags.contains(carId) || !car || !transforms.contains(*car)) {
            continue;
        }
        Tag& tag = _tags[carId];
        tag.car = *car;
        tag.name = name;
        // Around the anchor point (0, 0): the middle of the bottom edge of the label.
        const float textWidth = UiCanvas::textWidth(name, SQUARE);
        const float textHeight = static_cast<float>(font::GLYPH_ROWS) * SQUARE;
        tag.canvas.rect(registry,
                        ScreenRect{
                            .x = -(textWidth / 2.0F) - PADDING,
                            .y = -textHeight - (2.0F * PADDING),
                            .width = textWidth + (2.0F * PADDING),
                            .height = textHeight + (2.0F * PADDING),
                        },
                        BACKGROUND_COLOR, LAYER_BACKGROUND);
        const Rgb color = playerColor(looks.contains(*car) ? looks.get(*car).color : 0);
        tag.canvas.text(registry, name, 0.0F, -textHeight - PADDING, SQUARE, lighter(color), LAYER_TEXT);
    }

    // Follow the cars: the labels are laid out in pixels, shifted to the point above each car on screen.
    const ecs::Entity cameraEntity = engine.getPlugin<CameraPlugin>().camera();
    const auto& cameras = registry.getComponents<ecs::CameraComponent>();
    if (!cameras.contains(cameraEntity)) {
        return;
    }
    const ecs::CameraComponent camera = cameras.get(cameraEntity);
    const float zoom = camera.zoom > 0.0F ? camera.zoom : 1.0F;
    for (const auto& [carId, tag] : _tags) {
        const ecs::Transform& car = transforms.get(tag.car);
        const float screenX = camera.offsetX + ((car.x - camera.targetX) * zoom);
        const float screenY = camera.offsetY + ((car.y - HEIGHT_ABOVE_CAR - camera.targetY) * zoom);
        tag.canvas.place(registry, camera, screenX, screenY);
    }
}

void NameTagPlugin::clearTags(engine::Engine& engine) {
    for (auto& [carId, tag] : _tags) {
        tag.canvas.clear(engine.registry());
    }
    _tags.clear();
}

void NameTagPlugin::shutdown(engine::Engine& engine) {
    auto& events = engine.eventBus();
    if (_packets) {
        events.unsubscribe<ecs::net::PacketReceived>(*_packets);
        _packets.reset();
    }
    if (_joined) {
        events.unsubscribe<MatchJoined>(*_joined);
        _joined.reset();
    }
    if (_left) {
        events.unsubscribe<MatchLeft>(*_left);
        _left.reset();
    }
    clearTags(engine);
    _names.clear();
}

}  // namespace rlss::client
