#include "ui/system/UiInteractionSystem.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "components/Hidden.hpp"
#include "core/services/IWindowService.hpp"
#include "core/services/InputCodes.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "events/UiEvents.hpp"
#include "hierarchy/Hierarchy.hpp"
#include "ui/components/UiButton.hpp"
#include "ui/components/UiFocusable.hpp"
#include "ui/components/UiNavigation.hpp"
#include "ui/components/UiScrollList.hpp"
#include "ui/components/UiSlider.hpp"
#include "ui/components/UiTextInput.hpp"
#include "ui/components/UiToggle.hpp"
#include "ui/system/UiLayout.hpp"

namespace ui {

namespace {

/** @brief First code point that is not ASCII. */
constexpr char32_t FIRST_NON_ASCII = 0x80;
/** @brief First code point that is printable. */
constexpr char32_t FIRST_PRINTABLE = 0x20;
/** @brief Delete control character, not printable. */
constexpr char32_t DELETE_CHARACTER = 0x7F;
/** @brief Highest Unicode code point. */
constexpr char32_t MAX_CODE_POINT = 0x10FFFF;

/**
 * @brief Tells whether any binding of a command is held.
 * @param window Window service.
 * @param binding Keys and buttons of the command.
 * @param gamepad Gamepad index.
 * @param ignored Key not taken into account, if any.
 * @return `true` if one of them is held.
 */
bool held(const engine::IWindowService& window, const UiNavBinding& binding, int gamepad,
          std::optional<engine::Key> ignored = std::nullopt) {
    if (std::ranges::any_of(binding.keys,
                            [&window, ignored](engine::Key key) { return key != ignored && window.isKeyDown(key); })) {
        return true;
    }
    if (binding.buttons.empty() || !window.isGamepadAvailable(gamepad)) {
        return false;
    }
    return std::ranges::any_of(binding.buttons, [&window, gamepad](engine::GamepadButton button) {
        return window.isGamepadButtonDown(gamepad, button);
    });
}

/**
 * @brief Tells whether a point lies inside a rectangle.
 * @param rect Rectangle.
 * @param position Point.
 * @return `true` if inside, edges included.
 */
bool contains(const UiScreenRect& rect, engine::MousePosition position) {
    return position.x >= rect.x && position.x <= rect.x + rect.width && position.y >= rect.y &&
           position.y <= rect.y + rect.height;
}

/**
 * @brief Counts the code points of a UTF-8 string.
 * @param text UTF-8 text.
 * @return Number of code points.
 */
std::size_t codePointCount(const std::string& text) {
    return static_cast<std::size_t>(
        std::ranges::count_if(text, [](char c) { return (static_cast<unsigned char>(c) & 0xC0U) != 0x80U; }));
}

/**
 * @brief Removes the last code point of a UTF-8 string.
 * @param text UTF-8 text.
 */
void popCodePoint(std::string& text) {
    while (!text.empty()) {
        const auto last = static_cast<unsigned char>(text.back());
        text.pop_back();
        if ((last & 0xC0U) != 0x80U) {
            return;
        }
    }
}

/**
 * @brief Appends a code point to a UTF-8 string.
 * @param text UTF-8 text.
 * @param cp Code point, at most `MAX_CODE_POINT`.
 */
void appendCodePoint(std::string& text, char32_t cp) {
    const auto byte = [&text](std::uint32_t value) { text.push_back(static_cast<char>(value)); };
    const auto value = static_cast<std::uint32_t>(cp);
    if (value < 0x80U) {
        byte(value);
    } else if (value < 0x800U) {
        byte(0xC0U | (value >> 6U));
        byte(0x80U | (value & 0x3FU));
    } else if (value < 0x10000U) {
        byte(0xE0U | (value >> 12U));
        byte(0x80U | ((value >> 6U) & 0x3FU));
        byte(0x80U | (value & 0x3FU));
    } else {
        byte(0xF0U | (value >> 18U));
        byte(0x80U | ((value >> 12U) & 0x3FU));
        byte(0x80U | ((value >> 6U) & 0x3FU));
        byte(0x80U | (value & 0x3FU));
    }
}

/**
 * @brief Tells whether a text input accepts a typed character.
 * @param input Text input.
 * @param cp Typed code point; made a capital letter if the input asks for it.
 * @return `true` if it is printable, allowed, and the text is not full.
 */
bool accepts(const UiTextInput& input, char32_t& cp) {
    if (input.uppercase && cp >= U'a' && cp <= U'z') {
        cp = cp - U'a' + U'A';
    }
    if (cp < FIRST_PRINTABLE || cp == DELETE_CHARACTER || cp > MAX_CODE_POINT) {
        return false;
    }
    if (!input.allowed.empty() &&
        (cp >= FIRST_NON_ASCII || input.allowed.find(static_cast<char>(cp)) == std::string::npos)) {
        return false;
    }
    return codePointCount(input.text) < input.maxLength;
}

/**
 * @brief Sets a slider value, clamped, and publishes the change.
 * @param bus Event bus.
 * @param entity Slider entity.
 * @param slider Slider.
 * @param value New value.
 */
void setSliderValue(ecs::EventBus& bus, ecs::Entity entity, UiSlider& slider, float value) {
    const float clamped = std::clamp(value, std::min(slider.min, slider.max), std::max(slider.min, slider.max));
    if (clamped == slider.value) {
        return;
    }
    slider.value = clamped;
    bus.publish(UiValueChangedEvent{.entity = entity, .action = slider.action, .value = clamped});
}

/**
 * @brief Flips a toggle and publishes the change.
 * @param bus Event bus.
 * @param entity Toggle entity.
 * @param toggle Toggle.
 */
void flip(ecs::EventBus& bus, ecs::Entity entity, UiToggle& toggle) {
    toggle.on = !toggle.on;
    bus.publish(UiToggleChangedEvent{.entity = entity, .action = toggle.action, .on = toggle.on});
}

/**
 * @brief Gets a component of an entity, if it has one.
 * @tparam C Component type.
 * @param registry Registry.
 * @param entity Entity.
 * @return The component, or null.
 */
template <typename C>
C* find(ecs::Registry& registry, ecs::Entity entity) {
    if (registry.findComponents<C>() == nullptr) {
        return nullptr;
    }
    auto& components = registry.getComponents<C>();
    return components.contains(entity) ? &components.get(entity) : nullptr;
}

}  // namespace

UiInteractionSystem::Commands UiInteractionSystem::readCommands(const engine::IWindowService& window,
                                                                const UiNavigation& navigation) {
    const int pad = navigation.gamepad;
    Commands commands{
        .up = held(window, navigation.up, pad),
        .down = held(window, navigation.down, pad),
        .left = held(window, navigation.left, pad),
        .right = held(window, navigation.right, pad),
        .confirm = held(window, navigation.confirm, pad),
        .back = held(window, navigation.back, pad),
    };
    // Space confirms a button but types in a text input: only the other bindings submit a text input.
    commands.confirmNotTyping = commands.confirm && held(window, navigation.confirm, pad, engine::Key::Space);
    return commands;
}

void UiInteractionSystem::update(ecs::Registry& registry, std::span<const UiLayoutNode> layout,
                                 engine::IWindowService& window, ecs::EventBus& bus, float dt) {
    if (!registry.hasResource<UiFocus>()) {
        registry.emplaceResource<UiFocus>();
    }
    if (!registry.hasResource<UiNavigation>()) {
        registry.emplaceResource<UiNavigation>();
    }
    auto& focus = registry.getResource<UiFocus>();
    const UiNavigation& navigation = registry.getResource<UiNavigation>();

    if (focus.focused != ecs::NULL_ENTITY &&
        (!registry.isAlive(focus.focused) || ecs::isHidden(registry, focus.focused))) {
        focus.focused = ecs::NULL_ENTITY;
    }

    handleMouse(registry, layout, window, bus, focus);

    const Commands now = readCommands(window, navigation);
    const auto pressed = [this, &now](bool Commands::* command) { return now.*command && !(_previous.*command); };

    const bool editing = find<UiTextInput>(registry, focus.focused) != nullptr;
    auto* slider = find<UiSlider>(registry, focus.focused);

    if (pressed(&Commands::up) || (pressed(&Commands::left) && slider == nullptr && !editing)) {
        moveFocus(registry, layout, focus, false);
    } else if (pressed(&Commands::down) || (pressed(&Commands::right) && slider == nullptr && !editing)) {
        moveFocus(registry, layout, focus, true);
    } else if (slider != nullptr && (pressed(&Commands::left) || pressed(&Commands::right))) {
        const float delta = pressed(&Commands::right) ? slider->step : -slider->step;
        setSliderValue(bus, focus.focused, *slider, slider->value + delta);
    } else if (editing ? pressed(&Commands::confirmNotTyping) : pressed(&Commands::confirm)) {
        activate(registry, bus, focus.focused);
    }
    if (pressed(&Commands::back)) {
        bus.publish(UiBackEvent{});
    }
    _previous = now;

    if (find<UiTextInput>(registry, focus.focused) != nullptr) {
        editText(registry, window, focus.focused, focus, dt);
    } else {
        _backspaceHeld = false;
    }
    _lastFocused = focus.focused;
}

void UiInteractionSystem::handleMouse(ecs::Registry& registry, std::span<const UiLayoutNode> layout,
                                      const engine::IWindowService& window, ecs::EventBus& bus, UiFocus& focus) {
    const engine::MousePosition mouse = window.mousePosition();
    if (window.isMouseButtonPressed(engine::MouseButton::Left)) {
        handleClick(registry, layout, mouse, bus, focus);
    }
    if (!window.isMouseButtonDown(engine::MouseButton::Left)) {
        _dragged = ecs::NULL_ENTITY;
    }
    if (_dragged != ecs::NULL_ENTITY) {
        drag(registry, layout, mouse, bus);
    }
    if (const float wheel = window.mouseWheelDelta(); wheel != 0.0F) {
        scroll(registry, layout, mouse, wheel);
    }
}

void UiInteractionSystem::handleClick(ecs::Registry& registry, std::span<const UiLayoutNode> layout,
                                      engine::MousePosition mouse, ecs::EventBus& bus, UiFocus& focus) {
    // The topmost widget under the cursor is the last one in draw order.
    const auto hit = std::ranges::find_if(layout.rbegin(), layout.rend(), [&](const UiLayoutNode& node) {
        return contains(node.rect, mouse) && registry.hasComponent<UiFocusable>(node.entity);
    });
    if (hit == layout.rend()) {
        focus.focused = ecs::NULL_ENTITY;
        return;
    }
    focus.focused = hit->entity;
    if (auto* toggle = find<UiToggle>(registry, hit->entity); toggle != nullptr) {
        flip(bus, hit->entity, *toggle);
    }
    if (find<UiSlider>(registry, hit->entity) != nullptr) {
        _dragged = hit->entity;
    }
}

void UiInteractionSystem::drag(ecs::Registry& registry, std::span<const UiLayoutNode> layout,
                               engine::MousePosition mouse, ecs::EventBus& bus) {
    auto* slider = find<UiSlider>(registry, _dragged);
    const auto node = std::ranges::find(layout, _dragged, &UiLayoutNode::entity);
    if (slider == nullptr || node == layout.end()) {
        _dragged = ecs::NULL_ENTITY;
        return;
    }
    if (node->rect.width > 0.0F) {
        const float ratio = std::clamp((mouse.x - node->rect.x) / node->rect.width, 0.0F, 1.0F);
        setSliderValue(bus, _dragged, *slider, slider->min + (ratio * (slider->max - slider->min)));
    }
}

void UiInteractionSystem::scroll(ecs::Registry& registry, std::span<const UiLayoutNode> layout,
                                 engine::MousePosition mouse, float wheel) {
    const auto list = std::ranges::find_if(layout.rbegin(), layout.rend(), [&](const UiLayoutNode& node) {
        return contains(node.rect, mouse) && registry.hasComponent<UiScrollList>(node.entity);
    });
    if (list == layout.rend()) {
        return;
    }
    UiScrollList& rows = registry.getComponents<UiScrollList>().get(list->entity);
    const std::size_t count = hierarchy::childrenOf(registry, list->entity).size();
    const std::size_t maxFirst = count > rows.visibleCount ? count - rows.visibleCount : 0;
    if (wheel > 0.0F && rows.first > 0) {
        --rows.first;
    } else if (wheel < 0.0F && rows.first < maxFirst) {
        ++rows.first;
    }
}

bool UiInteractionSystem::moveInList(ecs::Registry& registry, UiFocus& focus, bool forward) {
    const std::optional<ecs::Entity> parent = hierarchy::parentOf(registry, focus.focused);
    if (!parent) {
        return false;
    }
    auto* list = find<UiScrollList>(registry, *parent);
    if (list == nullptr) {
        return false;
    }
    const std::span<const ecs::Entity> rows = hierarchy::childrenOf(registry, *parent);
    const auto it = std::ranges::find(rows, focus.focused);
    if (it == rows.end()) {
        return false;
    }
    const auto index = static_cast<std::size_t>(it - rows.begin());
    if (forward ? index + 1 >= rows.size() : index == 0) {
        return false;  // leaving the list: regular navigation takes over
    }
    const std::size_t next = forward ? index + 1 : index - 1;
    if (!registry.hasComponent<UiFocusable>(rows[next])) {
        return false;
    }
    if (next < list->first) {
        list->first = next;
    } else if (next >= list->first + list->visibleCount) {
        list->first = next + 1 - list->visibleCount;
    }
    focus.focused = rows[next];
    return true;
}

void UiInteractionSystem::moveFocus(ecs::Registry& registry, std::span<const UiLayoutNode> layout, UiFocus& focus,
                                    bool forward) {
    if (focus.focused != ecs::NULL_ENTITY && moveInList(registry, focus, forward)) {
        return;
    }
    _focusables.clear();
    for (const UiLayoutNode& node : layout) {
        if (registry.hasComponent<UiFocusable>(node.entity)) {
            _focusables.push_back(node.entity);
        }
    }
    if (_focusables.empty()) {
        return;
    }
    const auto& ranks = registry.getComponents<UiFocusable>();
    std::ranges::stable_sort(_focusables, std::less{}, [&ranks](ecs::Entity e) { return ranks.get(e).order; });

    const auto current = std::ranges::find(_focusables, focus.focused);
    if (current == _focusables.end()) {
        focus.focused = forward ? _focusables.front() : _focusables.back();
        return;
    }
    const auto index = static_cast<std::size_t>(current - _focusables.begin());
    const std::size_t count = _focusables.size();
    focus.focused = _focusables[forward ? (index + 1) % count : (index + count - 1) % count];
}

void UiInteractionSystem::activate(ecs::Registry& registry, ecs::EventBus& bus, ecs::Entity focused) {
    if (const auto* button = find<UiButton>(registry, focused); button != nullptr) {
        bus.publish(UiButtonClickedEvent{.entity = focused, .action = button->action});
    } else if (auto* toggle = find<UiToggle>(registry, focused); toggle != nullptr) {
        flip(bus, focused, *toggle);
    } else if (const auto* input = find<UiTextInput>(registry, focused); input != nullptr) {
        bus.publish(UiTextSubmittedEvent{.entity = focused, .action = input->action, .text = input->text});
    }
}

void UiInteractionSystem::editText(ecs::Registry& registry, engine::IWindowService& window, ecs::Entity focused,
                                   UiFocus& focus, float dt) {
    UiTextInput& input = registry.getComponents<UiTextInput>().get(focused);
    bool edited = false;

    for (char32_t cp = window.nextTypedCharacter(); cp != U'\0'; cp = window.nextTypedCharacter()) {
        if (accepts(input, cp)) {
            appendCodePoint(input.text, cp);
            edited = true;
        }
    }

    const bool backspace = window.isKeyDown(engine::Key::Backspace);
    if (backspace && !_backspaceHeld) {
        popCodePoint(input.text);
        _backspaceTimer = BACKSPACE_DELAY;
        edited = true;
    } else if (backspace) {
        _backspaceTimer -= dt;
        if (_backspaceTimer <= 0.0F) {
            popCodePoint(input.text);
            _backspaceTimer += BACKSPACE_REPEAT;
            edited = true;
        }
    }
    _backspaceHeld = backspace;

    if (edited || focused != _lastFocused) {
        focus.caretVisible = true;
        _caretTimer = 0.0F;
        return;
    }
    _caretTimer += dt;
    if (_caretTimer >= CARET_BLINK) {
        _caretTimer -= CARET_BLINK;
        focus.caretVisible = !focus.caretVisible;
    }
}

}  // namespace ui
