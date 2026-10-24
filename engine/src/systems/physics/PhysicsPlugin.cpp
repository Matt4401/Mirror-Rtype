/**
 * @file PhysicsPlugin.cpp
 * @brief Implementation of PhysicsPlugin and its abstract service adapter.
 */

#include "systems/physics/PhysicsPlugin.hpp"

#include <cstddef>

#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "core/plugins/CorePlugin.hpp"
#include "core/services/IPhysicsService.hpp"
#include "ecs/time/Time.hpp"
#include "systems/physics/PhysicsSystem.hpp"

namespace systems {

class PhysicsPlugin::PhysicsServiceImpl final : public engine::IPhysicsService {
  public:
    explicit PhysicsServiceImpl(PhysicsSystem& system) : _system(&system) {}

    void step(float fixedDt) override {
        if (_system != nullptr) {
            _system->update(fixedDt);
        }
    }

    [[nodiscard]] std::size_t bodyCount() const noexcept override {
        return (_system != nullptr) ? _system->bodyCount() : 0;
    }

    void setSystem(PhysicsSystem* system) noexcept { _system = system; }

  private:
    PhysicsSystem* _system = nullptr;
};

PhysicsPlugin::~PhysicsPlugin() {
    if (_physicsService != nullptr) {
        _physicsService->setSystem(nullptr);
    }
}

PhysicsPlugin::PhysicsPlugin(PhysicsPlugin&&) noexcept = default;
PhysicsPlugin& PhysicsPlugin::operator=(PhysicsPlugin&&) noexcept = default;

void PhysicsPlugin::build(engine::PluginBuilder& builder) { builder.require<engine::CorePlugin>(); }

void PhysicsPlugin::init(engine::Engine& engine) {
    _physicsSystem = std::make_unique<PhysicsSystem>(engine.registry());
    _physicsService = std::make_shared<PhysicsServiceImpl>(*_physicsSystem);
    engine.mediator().registerService<engine::IPhysicsService>(_physicsService);
}

void PhysicsPlugin::fixedUpdate(engine::Engine& /*engine*/, const ecs::time::Time& time) {
    if (_physicsSystem != nullptr) {
        _physicsSystem->update(time.deltaTime);
    }
}

void PhysicsPlugin::shutdown(engine::Engine& engine) {
    if (engine.mediator().hasService<engine::IPhysicsService>()) {
        engine.mediator().removeService<engine::IPhysicsService>();
    }
    if (_physicsService != nullptr) {
        _physicsService->setSystem(nullptr);
    }
    _physicsService.reset();
    _physicsSystem.reset();
}

}  // namespace systems
