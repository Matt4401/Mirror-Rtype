#include "scene/SceneManager.hpp"

#include <cstddef>
#include <exception>
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

std::optional<SceneId> SceneManager::findType(std::type_index type) const {
    std::optional<SceneId> found;
    for (std::size_t i = 0; i < _slots.size(); ++i) {
        if (_slots[i]->type == type) {
            if (found) {
                throw std::logic_error("SceneManager: several scenes have this type, look them up by name");
            }
            found = static_cast<SceneId>(i);
        }
    }
    return found;
}

SceneId SceneManager::spawnTarget() const noexcept {
    for (std::size_t i = _stack.size(); i-- > 0;) {
        if (_slots[_stack[i]]->scene->mode() == SceneMode::Exclusive) {
            return _stack[i];
        }
    }
    return _stack.empty() ? INVALID_SCENE : _stack.front();
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
    if (_engine == nullptr || _requests.empty() || _applyingTransitions || _runningStage) {
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
    try {
        slot.scene->onEnter(slot.context);
    } catch (...) {
        // Roll back: what the scene created goes, and the scenes below run and show again. No `onExit`, no
        // `SceneExited`: the scene never entered.
        const std::exception_ptr error = std::current_exception();
        std::exception_ptr ignored;
        removeTop(ignored);
        try {
            refreshStates(false);
        } catch (...) {  // NOLINT(bugprone-empty-catch): the error of `onEnter` is the one to report.
        }
        std::rethrow_exception(error);
    }
    _engine->eventBus().publish(SceneEntered{.id = id});
}

void SceneManager::exitTop(bool refresh) {
    const SceneId id = _stack.back();
    Slot& slot = *_slots[id];
    std::exception_ptr error;
    try {
        slot.scene->onExit(slot.context);
    } catch (...) {
        error = std::current_exception();
    }
    removeTop(error);
    try {
        _engine->eventBus().publish(SceneExited{.id = id});
        if (refresh) {
            refreshStates(false);
        }
    } catch (...) {
        if (!error) {
            error = std::current_exception();
        }
    }
    if (error) {
        std::rethrow_exception(error);
    }
}

void SceneManager::removeTop(std::exception_ptr& error) {
    const SceneId id = _stack.back();
    Slot& slot = *_slots[id];
    // Destroy listeners may subscribe, add systems or spawn through the exiting scene: release and destroy again
    // until no member is left. Each pass only finds what the previous one created, so this ends unless a listener
    // spawns into the exiting scene for every entity it sees go, which is a bug like any endless loop.
    bool destroyed = true;
    while (destroyed) {
        slot.context.release();
        try {
            destroyed = destroyMembers(id);
        } catch (...) {
            // `killEntities` finishes its non-empty batch before rethrowing: go on with the teardown.
            destroyed = true;
            if (!error) {
                error = std::current_exception();
            }
        }
    }
    slot.context.release();
    slot.active = false;
    slot.running = false;
    slot.visible = false;
    _stack.pop_back();
}

bool SceneManager::destroyMembers(SceneId id) {
    ecs::Registry& registry = _engine->registry();
    const auto* members = static_cast<const ecs::Registry&>(registry).findComponents<SceneMember>();
    if (members == nullptr) {
        return false;
    }
    _scratch.clear();
    const auto& entities = members->getDenseEntities();
    const auto& owners = members->getDense();
    for (std::size_t i = 0; i < owners.size(); ++i) {
        if (owners[i].id == id) {
            _scratch.push_back(entities[i]);
        }
    }
    if (_scratch.empty()) {
        return false;
    }
    registry.killEntities(_scratch);
    _scratch.clear();
    return true;
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
    // Transitions are deferred (and `applyTransitions` / `clear` refuse to run from here), so the stack cannot
    // change while this loop runs.
    if (_runningStage) {
        return;
    }
    _runningStage = true;
    try {
        for (const SceneId id : _stack) {
            Slot& slot = *_slots[id];
            if (slot.running) {
                slot.context.runSystems(stage, engine, time);
            }
        }
    } catch (...) {
        _runningStage = false;
        throw;
    }
    _runningStage = false;
}

void SceneManager::clear() {
    if (_runningStage) {
        throw std::logic_error("SceneManager: clear() cannot run from a scene system; queue a transition instead");
    }
    _requests.clear();
    if (_engine == nullptr) {
        return;
    }
    std::exception_ptr error;
    while (!_stack.empty()) {
        try {
            exitTop(false);
        } catch (...) {
            if (!error) {
                error = std::current_exception();
            }
        }
    }
    // The exit hooks may have queued transitions: the stack is meant to stay empty.
    _requests.clear();
    if (error) {
        std::rethrow_exception(error);
    }
}

}  // namespace scene
