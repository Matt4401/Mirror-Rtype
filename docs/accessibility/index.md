# Accessibility

This page lists what the R-Type client (and, where it applies, RLSS and the Squacaron engine) offers to players with
a disability. Every feature below can be shown in game; each one says where to find it. The options live in
**Settings** (title screen, or **Escape** during a game then **SETTINGS**) and are saved at once to
`settings.toml` in the user configuration directory, so they survive a restart.

## Physical and motor

| Feature | How to try it |
| --- | --- |
| **Key rebinding.** In R-Type every control (move up/down/left/right, shoot, charge, ready) can be given another key; in RLSS the same goes for steer left/right, nose up/down, jump and boost. Choosing a key that another control uses swaps the two, so no key drives two controls. **RESET** (RLSS: **PAR DEFAUT**) restores the defaults. | R-Type: Settings → **CONTROLS**. RLSS: **PARAMETRES** → **TOUCHES**. Select a control, press the new key (Escape cancels). |
| **Auto-fire.** One press of shoot starts firing and the next one stops it, so the player does not have to hold or mash a button. | Settings → **AUTO-FIRE**, then in game press shoot once. |
| **Gamepad.** The ship moves with the left stick, shoots with the bottom face button and charges with the right trigger. The menus are driven with the D-pad, the bottom face button (confirm) and the right face button (back). | Plug a gamepad in before or during the game. |
| **No mouse needed.** Every menu works with the keyboard alone: arrows or Tab move the focus, Enter or Space confirm, Escape goes back. The mouse also works for players who prefer it. | Any menu. |
| **Two key sets by default.** Moving works with W A S D *and* the arrows, shooting with Space *and* Enter, charging with either Shift, so a player can use one hand on either side of the keyboard. | In game. |
| **Physical key positions.** Bindings follow the key position, not the letter: the default movement keys are W A S D on QWERTY and Z Q S D on AZERTY, with no setup. The controls screens of both games show the letter printed on the player's keyboard. | Settings → **CONTROLS** (RLSS: **TOUCHES**) on an AZERTY keyboard. |
| **Local pause.** Escape pauses the inputs at any time, so a player can rest without leaving the room. | Escape in game. |

## Audio and visual

| Feature | How to try it |
| --- | --- |
| **Text size.** The menus (text, buttons, sliders) can be shown at 85 %, 100 %, 115 % or 130 %. | Settings → **TEXT SIZE**, each press moves to the next size and the screen updates at once. |
| **Separate volumes.** Master, music and effects each have their own slider, applied to the engine audio buses. | Settings → **MASTER / MUSIC / EFFECTS**. |
| **Spatial audio (RLSS).** Sounds are panned and attenuated by position, so the side of an event can be heard. | Play RLSS with headphones. |
| **Visible focus.** The focused widget has a yellow outline on a dark blue panel, readable without colour vision. | Any menu. |
| **State shown without sound.** Invulnerability after a hit is shown by the ship blinking, and the lobby shows each player's ready state as text, so no information is given by sound only. | Get hit in game; open a room. |
| **Metrics panel.** F3 (or the **METRICS** toggle) shows FPS, round-trip time and missed snapshots, which helps tell a slow network apart from a slow reaction. | F3 in game. |

## Mental and cognitive

| Feature | How to try it |
| --- | --- |
| **Short menu flow.** Title → server address → lobby → room → game, with the same Back key everywhere. | Start the client. |
| **Remembered choices.** The last server address and port, the volumes and every option above are saved, so a returning player does not have to set them again. | Restart the client. |
| **Plain error messages.** A failed connection or a lost server shows the reason in words on the connect screen instead of closing the game. | Connect to an address where no server runs. |
| **Level banner.** Each level starts with its name shown on screen before the enemies come. | Start a game. |

## Not done yet

- Colour-blind palette: the players' ships are told apart by colour only (one row of the spritesheet each).
- Option to reduce flashes and screen effects.
- Text size for the in-game HUD (the option only covers the menus).
- Rebinding the gamepad buttons (the gamepad layout is fixed).
- Sounds in R-Type: the engine has audio buses and spatial audio (used by RLSS), but R-Type plays no sound yet.

## Where it lives in the code

- Rebinding, engine side: `input::ActionMapping` (`engine/src/input/ActionMapping.hpp`) and the key names and AZERTY
  labels of `engine/src/input/KeyNames.hpp` (`keyId`, `keyFromId`, `keyLabel`), shared by both games.
- Rebinding in R-Type: `client/src/input/RTypeBindings.cpp` (`setupBindings`, `assignKey`), screen in `ControlsScene`
  (`client/src/scenes/MenuScenes.cpp`).
- Rebinding in RLSS: `rlss/client/Controls.cpp` (`setupBindings`, `assignKey`, the `controls.*` settings), screen in
  `ControlsScene` (`rlss/client/menu/MenuScenes.cpp`).
- Auto-fire: `client::AutoFireLatch` (`client/src/gameplay/AutoFire.hpp`), used by `PlayerControlPlugin`.
- Text size: `menu::setScale` / `menu::MenuScale` (`client/src/scenes/MenuKit.hpp`).
- Settings keys: `client/src/scenes/ClientSettings.hpp`, applied by `ClientScenesPlugin`.
- Menu navigation: `ui::UiNavigation` (`engine/src/ui/components/UiNavigation.hpp`).
