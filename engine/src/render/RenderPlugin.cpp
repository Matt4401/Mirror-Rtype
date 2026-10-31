/**
 * @file RenderPlugin.cpp
 * @brief Implementation of RenderPlugin and its abstract service adapters.
 */

#include "render/RenderPlugin.hpp"

#include <raylib.h>

#include <memory>
#include <utility>

#include "core/Engine.hpp"
#include "core/Mediator.hpp"
#include "core/MediatorError.hpp"
#include "core/PluginBuilder.hpp"
#include "core/plugins/CorePlugin.hpp"
#include "core/services/IRenderService.hpp"
#include "core/services/IWindowService.hpp"
#include "render/texture/TextureStore.hpp"
#include "render/window/Window.hpp"
#include "render/window/WindowConfig.hpp"
#include "systems/render/RenderSystem.hpp"

namespace render {

namespace {

/** @brief Raylib has no enumerator for an absent gamepad axis. */
constexpr int INVALID_GAMEPAD_AXIS = -1;

}  // namespace

namespace {

[[nodiscard]] int toRaylib(engine::Key key) noexcept {
    switch (key) {
        case engine::Key::Unknown:
            return KEY_NULL;
        case engine::Key::Apostrophe:
            return KEY_APOSTROPHE;
        case engine::Key::Comma:
            return KEY_COMMA;
        case engine::Key::Minus:
            return KEY_MINUS;
        case engine::Key::Period:
            return KEY_PERIOD;
        case engine::Key::Slash:
            return KEY_SLASH;
        case engine::Key::Digit0:
            return KEY_ZERO;
        case engine::Key::Digit1:
            return KEY_ONE;
        case engine::Key::Digit2:
            return KEY_TWO;
        case engine::Key::Digit3:
            return KEY_THREE;
        case engine::Key::Digit4:
            return KEY_FOUR;
        case engine::Key::Digit5:
            return KEY_FIVE;
        case engine::Key::Digit6:
            return KEY_SIX;
        case engine::Key::Digit7:
            return KEY_SEVEN;
        case engine::Key::Digit8:
            return KEY_EIGHT;
        case engine::Key::Digit9:
            return KEY_NINE;
        case engine::Key::Semicolon:
            return KEY_SEMICOLON;
        case engine::Key::Equal:
            return KEY_EQUAL;
        case engine::Key::A:
            return KEY_A;
        case engine::Key::B:
            return KEY_B;
        case engine::Key::C:
            return KEY_C;
        case engine::Key::D:
            return KEY_D;
        case engine::Key::E:
            return KEY_E;
        case engine::Key::F:
            return KEY_F;
        case engine::Key::G:
            return KEY_G;
        case engine::Key::H:
            return KEY_H;
        case engine::Key::I:
            return KEY_I;
        case engine::Key::J:
            return KEY_J;
        case engine::Key::K:
            return KEY_K;
        case engine::Key::L:
            return KEY_L;
        case engine::Key::M:
            return KEY_M;
        case engine::Key::N:
            return KEY_N;
        case engine::Key::O:
            return KEY_O;
        case engine::Key::P:
            return KEY_P;
        case engine::Key::Q:
            return KEY_Q;
        case engine::Key::R:
            return KEY_R;
        case engine::Key::S:
            return KEY_S;
        case engine::Key::T:
            return KEY_T;
        case engine::Key::U:
            return KEY_U;
        case engine::Key::V:
            return KEY_V;
        case engine::Key::W:
            return KEY_W;
        case engine::Key::X:
            return KEY_X;
        case engine::Key::Y:
            return KEY_Y;
        case engine::Key::Z:
            return KEY_Z;
        case engine::Key::LeftBracket:
            return KEY_LEFT_BRACKET;
        case engine::Key::Backslash:
            return KEY_BACKSLASH;
        case engine::Key::RightBracket:
            return KEY_RIGHT_BRACKET;
        case engine::Key::Grave:
            return KEY_GRAVE;
        case engine::Key::Space:
            return KEY_SPACE;
        case engine::Key::Escape:
            return KEY_ESCAPE;
        case engine::Key::Enter:
            return KEY_ENTER;
        case engine::Key::Tab:
            return KEY_TAB;
        case engine::Key::Backspace:
            return KEY_BACKSPACE;
        case engine::Key::Insert:
            return KEY_INSERT;
        case engine::Key::Delete:
            return KEY_DELETE;
        case engine::Key::Right:
            return KEY_RIGHT;
        case engine::Key::Left:
            return KEY_LEFT;
        case engine::Key::Down:
            return KEY_DOWN;
        case engine::Key::Up:
            return KEY_UP;
        case engine::Key::PageUp:
            return KEY_PAGE_UP;
        case engine::Key::PageDown:
            return KEY_PAGE_DOWN;
        case engine::Key::Home:
            return KEY_HOME;
        case engine::Key::End:
            return KEY_END;
        case engine::Key::CapsLock:
            return KEY_CAPS_LOCK;
        case engine::Key::ScrollLock:
            return KEY_SCROLL_LOCK;
        case engine::Key::NumLock:
            return KEY_NUM_LOCK;
        case engine::Key::PrintScreen:
            return KEY_PRINT_SCREEN;
        case engine::Key::Pause:
            return KEY_PAUSE;
        case engine::Key::F1:
            return KEY_F1;
        case engine::Key::F2:
            return KEY_F2;
        case engine::Key::F3:
            return KEY_F3;
        case engine::Key::F4:
            return KEY_F4;
        case engine::Key::F5:
            return KEY_F5;
        case engine::Key::F6:
            return KEY_F6;
        case engine::Key::F7:
            return KEY_F7;
        case engine::Key::F8:
            return KEY_F8;
        case engine::Key::F9:
            return KEY_F9;
        case engine::Key::F10:
            return KEY_F10;
        case engine::Key::F11:
            return KEY_F11;
        case engine::Key::F12:
            return KEY_F12;
        case engine::Key::LeftShift:
            return KEY_LEFT_SHIFT;
        case engine::Key::LeftControl:
            return KEY_LEFT_CONTROL;
        case engine::Key::LeftAlt:
            return KEY_LEFT_ALT;
        case engine::Key::LeftSuper:
            return KEY_LEFT_SUPER;
        case engine::Key::RightShift:
            return KEY_RIGHT_SHIFT;
        case engine::Key::RightControl:
            return KEY_RIGHT_CONTROL;
        case engine::Key::RightAlt:
            return KEY_RIGHT_ALT;
        case engine::Key::RightSuper:
            return KEY_RIGHT_SUPER;
        case engine::Key::KeyboardMenu:
            return KEY_KB_MENU;
        case engine::Key::Keypad0:
            return KEY_KP_0;
        case engine::Key::Keypad1:
            return KEY_KP_1;
        case engine::Key::Keypad2:
            return KEY_KP_2;
        case engine::Key::Keypad3:
            return KEY_KP_3;
        case engine::Key::Keypad4:
            return KEY_KP_4;
        case engine::Key::Keypad5:
            return KEY_KP_5;
        case engine::Key::Keypad6:
            return KEY_KP_6;
        case engine::Key::Keypad7:
            return KEY_KP_7;
        case engine::Key::Keypad8:
            return KEY_KP_8;
        case engine::Key::Keypad9:
            return KEY_KP_9;
        case engine::Key::KeypadDecimal:
            return KEY_KP_DECIMAL;
        case engine::Key::KeypadDivide:
            return KEY_KP_DIVIDE;
        case engine::Key::KeypadMultiply:
            return KEY_KP_MULTIPLY;
        case engine::Key::KeypadSubtract:
            return KEY_KP_SUBTRACT;
        case engine::Key::KeypadAdd:
            return KEY_KP_ADD;
        case engine::Key::KeypadEnter:
            return KEY_KP_ENTER;
        case engine::Key::KeypadEqual:
            return KEY_KP_EQUAL;
    }
    return KEY_NULL;
}

[[nodiscard]] int toRaylib(engine::GamepadButton button) noexcept {
    switch (button) {
        case engine::GamepadButton::Unknown:
            return GAMEPAD_BUTTON_UNKNOWN;
        case engine::GamepadButton::LeftFaceUp:
            return GAMEPAD_BUTTON_LEFT_FACE_UP;
        case engine::GamepadButton::LeftFaceRight:
            return GAMEPAD_BUTTON_LEFT_FACE_RIGHT;
        case engine::GamepadButton::LeftFaceDown:
            return GAMEPAD_BUTTON_LEFT_FACE_DOWN;
        case engine::GamepadButton::LeftFaceLeft:
            return GAMEPAD_BUTTON_LEFT_FACE_LEFT;
        case engine::GamepadButton::RightFaceUp:
            return GAMEPAD_BUTTON_RIGHT_FACE_UP;
        case engine::GamepadButton::RightFaceRight:
            return GAMEPAD_BUTTON_RIGHT_FACE_RIGHT;
        case engine::GamepadButton::RightFaceDown:
            return GAMEPAD_BUTTON_RIGHT_FACE_DOWN;
        case engine::GamepadButton::RightFaceLeft:
            return GAMEPAD_BUTTON_RIGHT_FACE_LEFT;
        case engine::GamepadButton::LeftTrigger1:
            return GAMEPAD_BUTTON_LEFT_TRIGGER_1;
        case engine::GamepadButton::LeftTrigger2:
            return GAMEPAD_BUTTON_LEFT_TRIGGER_2;
        case engine::GamepadButton::RightTrigger1:
            return GAMEPAD_BUTTON_RIGHT_TRIGGER_1;
        case engine::GamepadButton::RightTrigger2:
            return GAMEPAD_BUTTON_RIGHT_TRIGGER_2;
        case engine::GamepadButton::MiddleLeft:
            return GAMEPAD_BUTTON_MIDDLE_LEFT;
        case engine::GamepadButton::Middle:
            return GAMEPAD_BUTTON_MIDDLE;
        case engine::GamepadButton::MiddleRight:
            return GAMEPAD_BUTTON_MIDDLE_RIGHT;
        case engine::GamepadButton::LeftThumb:
            return GAMEPAD_BUTTON_LEFT_THUMB;
        case engine::GamepadButton::RightThumb:
            return GAMEPAD_BUTTON_RIGHT_THUMB;
    }
    return GAMEPAD_BUTTON_UNKNOWN;
}

[[nodiscard]] int toRaylib(engine::GamepadAxis axis) noexcept {
    switch (axis) {
        case engine::GamepadAxis::None:
            return INVALID_GAMEPAD_AXIS;
        case engine::GamepadAxis::LeftX:
            return GAMEPAD_AXIS_LEFT_X;
        case engine::GamepadAxis::LeftY:
            return GAMEPAD_AXIS_LEFT_Y;
        case engine::GamepadAxis::RightX:
            return GAMEPAD_AXIS_RIGHT_X;
        case engine::GamepadAxis::RightY:
            return GAMEPAD_AXIS_RIGHT_Y;
        case engine::GamepadAxis::LeftTrigger:
            return GAMEPAD_AXIS_LEFT_TRIGGER;
        case engine::GamepadAxis::RightTrigger:
            return GAMEPAD_AXIS_RIGHT_TRIGGER;
    }
    return INVALID_GAMEPAD_AXIS;
}

}  // namespace

/**
 * @brief Concrete adapter implementing IWindowService via Raylib Window.
 */
class RenderPlugin::WindowServiceImpl final : public engine::IWindowService {
  public:
    /**
     * @brief Constructs WindowServiceImpl with specified window configuration.
     * @param config Window dimension and display configuration.
     */
    explicit WindowServiceImpl(WindowConfig config) : _config(std::move(config)) {}

    [[nodiscard]] bool shouldClose() const noexcept override { return Window::shouldClose(); }

    void beginFrame() override { Window::beginFrame(); }

    void endFrame() override { Window::endFrame(); }

    [[nodiscard]] int width() const noexcept override { return _config.width; }

    [[nodiscard]] int height() const noexcept override { return _config.height; }

    [[nodiscard]] bool isKeyDown(engine::Key key) const noexcept override {
        const int code = toRaylib(key);
        return code != KEY_NULL && IsKeyDown(code);
    }

    [[nodiscard]] bool isGamepadAvailable(int gamepad) const noexcept override { return IsGamepadAvailable(gamepad); }

    [[nodiscard]] bool isGamepadButtonDown(int gamepad, engine::GamepadButton button) const noexcept override {
        const int code = toRaylib(button);
        return code != GAMEPAD_BUTTON_UNKNOWN && IsGamepadButtonDown(gamepad, code);
    }

    [[nodiscard]] float gamepadAxis(int gamepad, engine::GamepadAxis axis) const noexcept override {
        const int code = toRaylib(axis);
        return code != INVALID_GAMEPAD_AXIS ? GetGamepadAxisMovement(gamepad, code) : 0.0F;
    }

  private:
    /** @brief Stored window configuration. */
    WindowConfig _config;
};

/**
 * @brief Concrete adapter implementing IRenderService via RenderSystem.
 */
class RenderPlugin::RenderServiceImpl final : public engine::IRenderService {
  public:
    /**
     * @brief Constructs RenderServiceImpl wrapping a RenderSystem reference.
     * @param system Reference to the active RenderSystem.
     */
    explicit RenderServiceImpl(RenderSystem& system) : _system(&system) {}

    void collect(ecs::Registry& registry) override {
        if (_system != nullptr) {
            _system->collect(registry);
        }
    }

    void draw() const override {
        if (_system != nullptr) {
            _system->draw();
        }
    }

    void update(ecs::Registry& registry) override {
        if (_system != nullptr) {
            _system->update(registry);
        }
    }

    /**
     * @brief Sets or updates the wrapped RenderSystem pointer.
     * @param system Pointer to RenderSystem instance.
     */
    void setSystem(RenderSystem* system) noexcept { _system = system; }

  private:
    /** @brief Non-owning pointer to the underlying RenderSystem. */
    RenderSystem* _system = nullptr;
};

RenderPlugin::RenderPlugin(WindowConfig config) : _config(std::move(config)) {}

RenderPlugin::~RenderPlugin() {
    _renderService.reset();
    _windowService.reset();
    _renderSystem.reset();
    _textures.reset();
    _window.reset();
}

void RenderPlugin::build(engine::PluginBuilder& builder) {
    builder.require<engine::CorePlugin>();
    builder.provides<engine::IWindowService>();
    builder.provides<engine::IRenderService>();
}

void RenderPlugin::init(engine::Engine& engine) {
    _window = std::make_unique<Window>(_config);
    _textures = std::make_unique<TextureStore>();
    _renderSystem = std::make_unique<RenderSystem>(*_textures);

    _windowService = std::make_shared<WindowServiceImpl>(_config);
    _renderService = std::make_shared<RenderServiceImpl>(*_renderSystem);

    if (const auto res = engine.mediator().registerService<engine::IWindowService>(_windowService); !res) {
        throw engine::MediatorError(res.error());
    }
    if (const auto res = engine.mediator().registerService<engine::IRenderService>(_renderService); !res) {
        engine.mediator().removeService<engine::IWindowService>();
        throw engine::MediatorError(res.error());
    }
}

void RenderPlugin::render(engine::Engine& engine) {
    Window::beginFrame();
    if (_renderSystem != nullptr) {
        _renderSystem->update(engine.registry());
    }
    Window::endFrame();
}

void RenderPlugin::shutdown(engine::Engine& engine) {
    if (engine.mediator().hasService<engine::IRenderService>()) {
        engine.mediator().removeService<engine::IRenderService>();
    }
    if (engine.mediator().hasService<engine::IWindowService>()) {
        engine.mediator().removeService<engine::IWindowService>();
    }

    if (_renderService != nullptr) {
        _renderService->setSystem(nullptr);
    }
    _renderService.reset();
    _windowService.reset();
    _renderSystem.reset();
    _textures.reset();
    _window.reset();
}

}  // namespace render
