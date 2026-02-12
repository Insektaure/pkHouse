# pkHouse

A local bank system for Pokemon games on Nintendo Switch. Move Pokemon between your save files and local bank storage.

## Disclaimer

This software is provided "as-is" without any warranty.\
While the app has been tested, it may contain bugs that could corrupt or damage your save files.

**Use at your own risk.**

The author is not responsible for any data loss or damage to your save data.

This is why the automatic backup system exists — always verify your backups before making changes.

If you need to restore a backup, use a save manager such as [Checkpoint](https://github.com/FlagBrew/Checkpoint) or [JKSV](https://github.com/J-D-K/JKSV) to import the backup files back onto your Switch.


## Supported Games

| Game | Tested Version | Save Format | Boxes |
|------|----------------|-------------|-------|
| Pokemon Sword | 1.3.2 | SCBlock (`main`) | 32 |
| Pokemon Shield | 1.3.2 | SCBlock (`main`) | 32 |
| Pokemon Brilliant Diamond | 1.3.0 | Flat binary (`SaveData.bin`) | 40 |
| Pokemon Shining Pearl | 1.3.0 | Flat binary (`SaveData.bin`) | 40 |
| Pokemon Legends: Arceus | 1.1.1 | SCBlock (`main`) | 32 |
| Pokemon Scarlet | 4.0.0 | SCBlock (`main`) | 32 |
| Pokemon Violet | 4.0.0 | SCBlock (`main`) | 32 |
| Pokemon Legends: Z-A | 2.0.1 | SCBlock (`main`) | 32 |

Each box holds 30 Pokemon slots.

> **Note:** Moving Pokemon between different games is not supported. Banks are separated by game family because transferred Pokemon would lack the HOME Tracker ID required for cross-game compatibility.

## Features

### Profile Selection

On Switch, pkHouse loads all user profiles from the system.\
Select your profile, then choose a game.\
The app detects which games have save data for the selected profile.\
Your profile name is shown alongside the game name in all views.

### Two-Panel Box Viewer

The main view displays your **game save** on the left and your **bank** on the right, side by side.\
Navigate freely between both panels to move Pokemon back and forth.

### Pick & Place

Press **A** on a Pokemon to pick it up, navigate to any slot (in either panel), and press **A** again to place it.\
Placing on an occupied slot swaps the two Pokemon.\
Press **B** to cancel and return the Pokemon to its original slot.

### Multi-Select

Toggle selection on individual Pokemon in the current box with **Y**.\
Selected slots display a numbered badge showing the pick-up order.\
Press **A** to pick up all selected Pokemon, then navigate to another box and press **A** to place them into the first available empty slots (in selection order).\
The destination must have enough empty slots for the entire group. Press **B** to cancel and return all Pokemon to their original positions.

Selection is cleared when switching boxes or panels.

### Pokemon Details

Press **X** on any Pokemon to view detailed information:

- Species, level, gender
- National Pokedex number
- Original Trainer (OT) and Trainer ID
- Nature and Ability
- All 4 moves
- IVs (perfect 31s highlighted in gold)
- EVs

Shiny Pokemon names are displayed in gold. Alpha Pokemon (Legends: Arceus / Z-A) show a dedicated icon.

### Bank System

Banks are local `.bin` files stored per game family. Paired games share the same bank folder, so you can move Pokemon between versions (e.g. Sword and Shield).

| Game Family | Bank Folder |
|-------------|-------------|
| Sword / Shield | `banks/SwordShield/` |
| Brilliant Diamond / Shining Pearl | `banks/BDSP/` |
| Legends: Arceus | `banks/LegendsArceus/` |
| Scarlet / Violet | `banks/ScarletViolet/` |
| Legends: Z-A | `banks/LegendsZA/` |

From the bank selector you can:

- **Create** a new bank (up to 32-character name)
- **Rename** an existing bank
- **Delete** a bank (with confirmation)

Each bank has the same box capacity as its game family (32 or 40 boxes).\
The bank list shows the number of occupied slots for each bank.

You can switch between banks from the main view via the menu, with or without saving first.

### Backup System

When loading a game save on Switch, an automatic backup is created before any modifications:

```
backups/<profile>/<game>/<profile>_YYYY-MM-DD_HH-MM-SS/
```

This is a full copy of the mounted save directory.\
The backup is only created once when initially selecting a game — switching banks does not trigger additional backups.

### Save Integrity

- **SCBlock saves** (ZA, SV, SwSh, LA): Decrypted, modified, and re-encrypted. A round-trip verification runs on load to confirm the cycle is lossless.
- **BDSP saves**: Flat binary with MD5 checksum, recalculated on every save.
- All saves are written in-place to preserve the Switch filesystem journal.

## Controls

### Profile Selector

| Button | Action |
|--------|--------|
| D-Pad Left/Right | Navigate profiles |
| A | Select profile |
| - | About |
| + | Quit |

### Game Selector

| Button | Action |
|--------|--------|
| D-Pad | Navigate game grid |
| A | Select game |
| B | Back to profile selector |
| - | About |
| + | Quit |

### Bank Selector

| Button | Action |
|--------|--------|
| D-Pad Up/Down | Navigate bank list |
| A | Open bank |
| Y | Create new bank |
| X | Rename bank |
| + | Delete bank |
| B | Back to game selector |
| - | About |

### Main View

| Button | Action |
|--------|--------|
| D-Pad | Move cursor |
| L / R | Switch box |
| A | Pick up / Place Pokemon |
| B | Cancel / Return held Pokemon |
| Y | Toggle multi-select |
| X | View Pokemon details |
| + | Open menu |
| - | About |

### Menu Options

| Option | Description |
|--------|-------------|
| Switch Bank (With saving) | Save game and bank, return to bank selector |
| Switch Bank (Without saving) | Return to bank selector without saving |
| Save & Quit | Save everything and exit |
| Quit Without Saving | Exit without saving changes |

## Building

### Prerequisites

- [devkitPro](https://devkitpro.org/) with the devkitA64 toolchain
- Switch portlibs: SDL2, SDL2_image, SDL2_ttf

```bash
dkp-pacman -S switch-sdl2 switch-sdl2_image switch-sdl2_ttf switch-freetype switch-harfbuzz
```

### Build

```bash
export DEVKITPRO=/opt/devkitpro
make all
```

Produces `pkHouse.nro`.

```bash
make clean
```

### Running

Place `pkHouse.nro` on your Switch SD card (`sdmc:/switch/pkHouse/`) and launch via a homebrew launcher in **title takeover mode**.

Applet mode **is not supported** due to memory constraints.

## Screenshots

<div align="center">
    <img src="screenshots/001.jpg">
    <img src="screenshots/002.jpg">
    <img src="screenshots/003.jpg">
    <img src="screenshots/004.jpg">
    <img src="screenshots/005.jpg">
    <img src="screenshots/006.jpg">
    <img src="screenshots/007.jpg">
    <img src="screenshots/008.jpg">
    <img src="screenshots/009.jpg">
    <img src="screenshots/010.jpg">
    <img src="screenshots/011.jpg">
    <img src="screenshots/012.jpg">
    <img src="screenshots/013.jpg">
    <img src="screenshots/014.jpg">
</div>

## Credits

- [PKHeX](https://github.com/kwsch/PKHeX) by kwsch — PokeCrypto research and save structure reference
- [JKSV](https://github.com/J-D-K/JKSV) by J-D-K — Save backup and write logic reference
- Built with [libnx](https://github.com/switchbrew/libnx) and [SDL2](https://www.libsdl.org/)
