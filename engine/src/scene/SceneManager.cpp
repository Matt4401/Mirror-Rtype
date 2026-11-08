#include "scene/SceneManager.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <typeindex>
#include <utility>

#include "core/Engine.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "scene/IScene.hpp"
#include "scene/SceneEvents.hpp"
#include "scene/SceneTypes.hpp"

namespace scene {

SceneId SceneManager::add(std::unique_ptr<IScene> scene) {
    if (!scene) {
        throw std::invalid_argument("SceneManager: cannot register a null scene");
    }
    const std::type_index type(typeid(*scene));
    if (findType(type)) {
        throw std::invalid_argument("SceneManager: a scene of this type is already registered");
    }
    if (find(scene->name())) {
        throw std::invalid_argument("SceneManager: a scene with this name is already registered");
    }
    if (_slots.size() >= static_cast<std::size_t>(INVALID_SCENE)) {
        throw std::length_error("SceneManager: too many scenes");
    }
    const auto id = static_cast<SceneId>(_slots.size());
    _slots.push_back(
        std::make_unique<Slot>(Slot{.scene = std::move(scene), .type = type, .context = SceneContext(*this, id)}));
    return id;
}

std::optional<SceneId> SceneManager::find(std::string_view name) const noexcept {
    for (std::size_t i = 0; i < _slots.size(); ++i) {
        if (_slots[i]->scene->name() == name) {
            return static_cast<SceneId>(i);
        }
    }
    return std::nullopt;
}

std::optional<SceneId> SceneManager::findType(std::type_index type) const noexcept {
    for (std::size_t i = 0; i < _slots.size(); ++i) {
        if (_slots[i]->type == type) {
            return static_cast<SceneId>(i);
        }
    }
    return std::nullopt;
}

SceneManager::Slot& SceneManager::slotOf(SceneId id) const {
    if (id >= _slots.size()) {
        throw std::out_of_range("SceneManager: unknown scene id");
    }
    return *_slots[id];
}

IScene& SceneManager::scene(SceneId id) const { return *slotOf(id).scene; }

SceneContext& SceneManager::context(SceneId id) const { return slotOf(id).context; }

void SceneManager::switchTo(SceneId id) {
    (void)slotOf(id);
    _requests.push_back(Request{.operation = Operation::Switch, .id = id});
}

void SceneManager::push(SceneId id) {
    (void)slotOf(id);
    _requests.push_back(Request{.operation = Operation::Push, .id = id});
}

void SceneManager::pop() { _requests.push_back(Request{.operation = Operation::Pop}); }

void SceneManager::replace(SceneId id) {
    (void)slotOf(id);
    _requests.push_back(Request{.operation = Operation::Replace, .id = id});
}

bool SceneManager::isActive(SceneId id) const noexcept { return id < _slots.size() && _slots[id]->active; }

bool SceneManager::isRunning(SceneId id) const noexcept { return id < _slots.size() && _slots[id]->running; }

bool SceneManager::isVisible(SceneId id) const noexcept { return id < _slots.size() && _slots[id]->visible; }

void SceneManager::applyTransitions() {
    if (_engine == nullptr || _requests.empty() || _applyingTransitions) {
        return;
    }
    _applyingTransitions = true;
    std::size_t applied = 0;
    try {
        // Hooks may queue more transitions: take the queue batch by batch until it stays empty.
        while (!_requests.empty()) {
            _applying.swap(_requests);
            for (const Request& request : _applying) {
                if (++applied > MAX_TRANSITIONS_PER_FRAME) {
                    throw std::logic_error("SceneManager: too many scene transitions in one frame");
                }
                apply(request);
            }
            _applying.clear();
        }
    } catch (...) {
        _applying.clear();
        _requests.clear();
        _applyingTransitions = false;
        throw;
    }
    _applyingTransitions = false;
}

void SceneManager::apply(const Request& request) {
    switch (request.operation) {
        case Operation::Switch:
            while (!_stack.empty()) {
                exitTop(false);
            }
            enter(request.id);
            break;
        case Operation::Push:
            enter(request.id);
            break;
        case Operation::Pop:
            if (!_stack.empty()) {
                exitTop(true);
            }
            break;
        case Operation::Replace:
            if (!_stack.empty()) {
                exitTop(false);
            }
            enter(request.id);
            break;
    }
}

void SceneManager::enter(SceneId id) {
    Slot& slot = slotOf(id);
    if (slot.active) {
        throw std::logic_error("SceneManager: the scene is already on the stack");
    }
    slot.active = true;
    _stack.push_back(id);
    // The scenes below learn they are paused or hidden before the new scene spawns anything.
    refreshStates(true);
    slot.scene->onEnter(slot.context);
    _engine->eventBus().publish(SceneEntered{.id = id});
}

void SceneManager::exitTop(bool refresh) {
    const SceneId id = _stack.back();
    Slot& slot = *_slots[id];
    slot.scene->onExit(slot.context);
    slot.context.release();
    destroyMembers(id);
    slot.active = false;
    slot.running = false;
    slot.visible = false;
    _stack.pop_back();
    _engine->eventBus().publish(SceneExited{.id = id});
    if (refresh) {
        refreshStates(false);
    }
}

void SceneManager::destroyMembers(SceneId id) {
    ecs::Registry& registry = _engine->registry();
    const auto* members = static_cast<const ecs::Registry&>(registry).findComponents<SceneMember>();
    if (members == nullptr) {
        return;
    }
    _scratch.clear();
    const auto& entities = members->getDenseEntities();
    const auto& owners = members->getDense();
    for (std::size_t i = 0; i < owners.size(); ++i) {
        if (owners[i].id == id) {
            _scratch.push_back(entities[i]);
        }
    }
    registry.killEntities(_scratch);
    _scratch.clear();
}

void SceneManager::refreshStates(bool silentTop) {
    bool running = true;
    bool visible = true;
    ecs::EventBus& bus = _engine->eventBus();
    for (std::size_t i = _stack.size(); i-- > 0;) {
        const SceneId id = _stack[i];
        Slot& slot = *_slots[id];
        const bool wasRunning = slot.running;
        const bool wasVisible = slot.visible;
        slot.running = running;
        slot.visible = visible;

        const bool silent = silentTop && i + 1 == _stack.size();
        if (!silent) {
            if (wasVisible != visible) {
                bus.publish(SceneVisibilityChanged{.id = id, .visible = visible});
            }
            if (wasRunning && !running) {
                slot.scene->onPause(slot.context);
                bus.publish(ScenePaused{.id = id});
            } else if (!wasRunning && running) {
                slot.scene->onResume(slot.context);
                bus.publish(SceneResumed{.id = id});
            }
        }

        if (slot.scene->mode() == SceneMode::Exclusive) {
            running = false;
            visible = false;
        } else if (slot.scene->pausesBelow()) {
            running = false;
        }
    }
}

void SceneManager::runStage(SceneStage stage, engine::Engine& engine, const ecs::time::Time& time) {
    // Transitions are deferred, so the stack cannot change while this loop runs.
    for (const SceneId id : _stack) {
        Slot& slot = *_slots[id];
        if (slot.running) {
            slot.context.runSystems(stage, engine, time);
        }
    }
}

void SceneManager::clear() {
    _requests.clear();
    if (_engine == nullptr) {
        return;
    }
    while (!_stack.empty()) {
        exitTop(false);
    }
}

}  // namespace scene
