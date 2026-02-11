# pkHouse

A Pokemon box manager for Nintendo Switch and PC. Lets you view, organize, and back up Pokemon from your save file into multiple bank files.

## Screens

### Splash Screen

Displayed on startup while data loads. Shows `romfs/splash.png` centered on a black background.

### Bank Selector

The first interactive screen. Lists all bank files found in the `banks/` directory. Each row shows the bank name and how many of the 960 slots are occupied (e.g. `42/960`).

On first launch, if a legacy `bank.bin` file exists, it is automatically migrated to `banks/Default.bin`.

| Action | Switch | PC Keyboard |
|---|---|---|
| Navigate list | D-Pad Up/Down | Arrow Up/Down |
| Open selected bank | A | A / Enter |
| Create new bank | X | X |
| Rename selected bank | Y | Y |
| Delete selected bank | - (Minus) | Delete |
| Quit app | + (Plus) | Escape |

- **Create / Rename**: Opens a text input dialog (Switch software keyboard or PC text popup). Bank names are limited to 32 characters. Invalid filesystem characters are stripped automatically.
- **Delete**: Shows a confirmation popup before removing the bank file permanently.

### Main View (Two-Panel Box Viewer)

Left panel shows your game save boxes. Right panel shows the currently loaded bank. The bank name is shown in the right panel header (e.g. `MyBank - Bank 3 (3/32)`).

| Action | Switch | PC Keyboard |
|---|---|---|
| Move cursor | D-Pad | Arrow keys |
| Switch box | L / R | Q / E |
| Pick up / Place Pokemon | A | A / Enter |
| Cancel (return held Pokemon) | B | B / Escape |
| View Pokemon details | Y | Y |
| Open menu | + (Plus) | + |
| Quit without saving | - (Minus) | - |

**While holding a Pokemon:**
- Placing on an empty slot drops the Pokemon there.
- Placing on an occupied slot swaps the held Pokemon with the one in the slot.
- Cancel returns the Pokemon to its original position.

### Pokemon Detail Popup

Shows detailed info for the selected Pokemon: species, level, gender, OT/TID, nature, ability, moves, IVs, and EVs. Shiny Pokemon names are displayed in gold. Perfect IVs (31) are highlighted.

| Action | Switch | PC Keyboard |
|---|---|---|
| Close | B or Y | B, Y, or Escape |

### Menu Popup

Opened with + from the main view.

| # | Option | Description |
|---|---|---|
| 1 | Save (Game & Bank) | Saves both the game save and the current bank file |
| 2 | Switch Bank (With saving) | Saves game and bank, then returns to the bank selector |
| 3 | Switch Bank (Without saving) | Returns to the bank selector without saving anything |
| 4 | Save & Quit | Saves both files and exits the app |
| 5 | Quit Without Saving | Exits immediately without saving |

| Action | Switch | PC Keyboard |
|---|---|---|
| Navigate | D-Pad Up/Down | Arrow Up/Down |
| Confirm | A | A / Enter |
| Cancel | B | B / Escape |

### Delete Confirmation Popup

Shown when deleting a bank from the bank selector. Displays the bank name and a warning that deletion cannot be undone.

| Action | Switch | PC Keyboard |
|---|---|---|
| Confirm delete | A | A / Enter |
| Cancel | B | B / Escape |

### Text Input Popup (PC only)

Used for creating and renaming banks. On Switch, the system software keyboard is used instead.

| Action | Key |
|---|---|
| Type characters | Keyboard |
| Move cursor | Left / Right |
| Delete character | Backspace / Delete |
| Confirm | Enter |
| Cancel | Escape |

## File Structure

```
<app directory>/
  main              # Game save file
  banks/            # Bank storage directory
    Default.bin     # Migrated from legacy bank.bin (if applicable)
    MyBank.bin      # User-created banks
    ...
  romfs/
    splash.png      # Splash screen image
    sprites/        # Pokemon sprite PNGs (001.png - 1025.png)
    icons/          # Status icons (shiny.png, alpha.png, shiny_alpha.png)
    fonts/          # Font files (default.ttf)
    data/           # Text data (species_en.txt, moves_en.txt, natures_en.txt, abilities_en.txt)
```

## Building

Requires [devkitPro](https://devkitpro.org/) with libnx and the following Switch portlibs: SDL2, SDL2_image, SDL2_ttf.

```bash
make        # Build .nro
make clean  # Clean build artifacts
```

The output `pkHouse.nro` goes in `sdmc:/switch/pkHouse/` on the Switch SD card alongside the `main` save file.
