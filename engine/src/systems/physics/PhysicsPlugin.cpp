/**
 * @file PhysicsPlugin.cpp
 * @brief Implementation of PhysicsPlugin and its abstract service adapter.
 */

#include "systems/physics/PhysicsPlugin.hpp"

#include <cstddef>
#include <memory>

#include "core/Engine.hpp"
#include "core/Mediator.hpp"
#include "core/MediatorError.hpp"
#include "core/PluginBuilder.hpp"
#include "core/plugins/CorePlugin.hpp"
#include "core/services/IPhysicsService.hpp"
#include "ecs/time/Time.hpp"
#include "systems/physics/PhysicsSystem.hpp"

namespace systems {

/**
 * @brief Concrete adapter implementing IPhysicsService via PhysicsSystem.
 */
class PhysicsPlugin::PhysicsServiceImpl final : public engine::IPhysicsService {
  public:
    /**
     * @brief Constructs PhysicsServiceImpl wrapping a PhysicsSystem reference.
     * @param system Reference to the active PhysicsSystem.
     */
    explicit PhysicsServiceImpl(PhysicsSystem& system) : _system(&system) {}

    void step(float fixedDt) override {
        if (_system != nullptr) {
            _system->update(fixedDt);
        }
    }

    [[nodiscard]] std::size_t bodyCount() const noexcept override {
        return (_system != nullptr) ? _system->bodyCount() : 0;
    }

    /**
     * @brief Sets or updates the wrapped PhysicsSystem pointer.
     * @param system Pointer to PhysicsSystem instance.
     */
    void setSystem(PhysicsSystem* system) noexcept { _system = system; }

  private:
    /** @brief Non-owning pointer to the underlying PhysicsSystem. */
    PhysicsSystem* _system = nullptr;
};

PhysicsPlugin::~PhysicsPlugin() {
    if (_physicsService != nullptr) {
        _physicsService->setSystem(nullptr);
    }
}

void PhysicsPlugin::build(engine::PluginBuilder& builder) {
    builder.require<engine::CorePlugin>();
    builder.provides<engine::IPhysicsService>();
}

void PhysicsPlugin::init(engine::Engine& engine) {
    _physicsSystem = std::make_unique<PhysicsSystem>(engine.registry());
    _physicsService = std::make_shared<PhysicsServiceImpl>(*_physicsSystem);
    if (const auto res = engine.mediator().registerService<engine::IPhysicsService>(_physicsService); !res) {
        throw engine::MediatorError(res.error());
    }
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
