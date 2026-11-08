/**
 * @file DevConsolePlugin.hpp
 * @brief Plugin showing the console in a window toggled with the key left of `1` (`~` / `²`).
 */

#pragma once

#include <array>
#include <memory>
#include <string_view>
#include <vector>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "core/services/InputCodes.hpp"
#include "tools/devconsole/DevConsole.hpp"
#include "tools/devconsole/DevConsoleTypes.hpp"
#include "tools/devconsole/DevConsoleView.hpp"
#include "tools/devconsole/KeyRepeater.hpp"

namespace engine {
class IInputCaptureService;
}

namespace tools {

/**
 * @brief In-game developer console window, on top of the `ConsolePlugin`.
 *
 * - The toggle key (`DevConsoleConfig::toggleKey`) opens and closes it; Escape closes it too.
 * - While it is open it captures the input (`engine::IInputCaptureService`): gameplay, menus and every other reader
 *   see an idle keyboard, mouse and gamepad.
 * - Typing edits the input line; Enter runs it, Tab completes it, Up/Down browse the history, Left/Right/Home/End move
 *   the cursor, Backspace/Delete erase, PageUp/PageDown and the mouse wheel scroll the output. Held keys repeat.
 * - Input is read in `preUpdate`, before the systems of the frame; the window is drawn in `lateUpdate`, after the UI
 *   and, thanks to its low `priority`, after every other plugin drawing on screen.
 *
 * Holds no graphics-library code: it talks to the window, capture and screen draw services only.
 */
class DevConsolePlugin final : public engine::IPlugin {
  public:
    /** @brief Keys of the editing actions. */
    static constexpr auto KEY_BINDINGS = std::to_array<KeyBinding>({
        {.key = engine::Key::Enter, .action = EditAction::Submit, .repeats = false},
        {.key = engine::Key::KeypadEnter, .action = EditAction::Submit, .repeats = false},
        {.key = engine::Key::Tab, .action = EditAction::Complete, .repeats = false},
        {.key = engine::Key::Backspace, .action = EditAction::Backspace, .repeats = true},
        {.key = engine::Key::Delete, .action = EditAction::Delete, .repeats = true},
        {.key = engine::Key::Left, .action = EditAction::Left, .repeats = true},
        {.key = engine::Key::Right, .action = EditAction::Right, .repeats = true},
        {.key = engine::Key::Home, .action = EditAction::Home, .repeats = false},
        {.key = engine::Key::End, .action = EditAction::End, .repeats = false},
        {.key = engine::Key::Up, .action = EditAction::HistoryPrevious, .repeats = true},
        {.key = engine::Key::Down, .action = EditAction::HistoryNext, .repeats = true},
        {.key = engine::Key::PageUp, .action = EditAction::PageUp, .repeats = true},
        {.key = engine::Key::PageDown, .action = EditAction::PageDown, .repeats = true},
        {.key = engine::Key::Escape, .action = EditAction::Close, .repeats = false},
    });

    /** @brief Runs after every plugin that does not depend on it, so the console is drawn over every overlay. */
    static constexpr int PRIORITY = -100;

    /** @brief Output lines scrolled per notch of the mouse wheel. */
    static constexpr float WHEEL_LINES = 3.0F;

    /**
     * @brief Creates the plugin.
     * @param config Toggle key, layout, font and colours.
     */
    explicit DevConsolePlugin(DevConsoleConfig config = {});

    /** @brief Default destructor. */
    ~DevConsolePlugin() override;
    /** @brief Not copyable. */
    DevConsolePlugin(const DevConsolePlugin&) = delete;
    /** @brief Not copyable. */
    DevConsolePlugin& operator=(const DevConsolePlugin&) = delete;
    /** @brief Not movable. */
    DevConsolePlugin(DevConsolePlugin&&) = delete;
    /** @brief Not movable. */
    DevConsolePlugin& operator=(DevConsolePlugin&&) = delete;

    /**
     * @brief Gets plugin name.
     * @return String view "DevConsolePlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "DevConsolePlugin"; }

    /**
     * @brief Gets the priority: low, so the console runs and draws after the HUDs and overlays of the game.
     * @return `PRIORITY`.
     */
    [[nodiscard]] int priority() const noexcept override { return PRIORITY; }

    /**
     * @brief Declares the console plugin, the window, capture and screen draw services, and the UI (drawn first).
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Creates the console state on the `Console` service and loads the font.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Reads the toggle key, the typed characters, the editing keys and the wheel; captures the input while open.
     * @param engine Host engine.
     */
    void preUpdate(engine::Engine& engine) override;

    /**
     * @brief Draws the console while it is open.
     * @param engine Host engine.
     * @param time Frame time.
     */
    void lateUpdate(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Releases the input capture.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

    /** @brief Gets the console state. @return The state, or `nullptr` before `init`. */
    [[nodiscard]] DevConsole* devConsole() noexcept { return _devConsole.get(); }

  private:
    /**
     * @brief Feeds the typed characters and the editing keys of the frame to the open console.
     * @param capture Real input.
     * @param dt Frame duration in seconds.
     */
    void readEditing(engine::IInputCaptureService& capture, float dt);

    /**
     * @brief Forgets the pending typed characters and makes held keys wait for their release.
     * @param capture Real input.
     */
    void discardPendingInput(engine::IInputCaptureService& capture);

    /** @brief Toggle key, layout, font and colours. */
    DevConsoleConfig _config;
    /** @brief Drawing of the console. */
    DevConsoleView _view;
    /** @brief Console state, created at `init`. */
    std::unique_ptr<DevConsole> _devConsole;
    /** @brief Toggle key presses. */
    KeyRepeater _toggle;
    /** @brief Presses of each key of `KEY_BINDINGS`. */
    std::vector<KeyRepeater> _repeaters;
    /** @brief Time since the cursor last changed visibility. */
    float _caretTimer = 0.0F;
    /** @brief Whether the cursor is shown. */
    bool _caretVisible = true;
};

}  // namespace tools
