# pkHouse

A local bank system for Pokemon games on Nintendo Switch. Move Pokemon between your save files and local bank storage.

[![Discord](https://img.shields.io/badge/Discord-join-5865F2?logo=discord&logoColor=white)](https://discord.gg/tRbuDZYWV)

Questions, bug reports and feature ideas are welcome on the [Discord](https://discord.gg/tRbuDZYWV).

## Disclaimer

This software is provided "as-is" without any warranty.\
While the app has been tested, it may contain bugs that could corrupt or damage your save files.

**Use at your own risk.**

The author is not responsible for any data loss or damage to your save data.

This is why the automatic backup system exists - always verify your backups before making changes.

If you need to restore a backup, use a save manager such as [Checkpoint](https://github.com/FlagBrew/Checkpoint) or [JKSV](https://github.com/J-D-K/JKSV) to import the backup files back onto your Switch.


## Supported Games

| Game                      | Tested Version | Save Format                     | Boxes | Slots/Box |
|---------------------------|----------------|---------------------------------|-------|-----------|
| Pokemon Let's Go Pikachu  | 1.0.2          | BEEF blocks (`savedata.bin`)    | 40    | 25        |
| Pokemon Let's Go Eevee    | 1.0.2          | BEEF blocks (`savedata.bin`)    | 40    | 25        |
| Pokemon Sword             | 1.3.2          | SCBlock (`main`)                | 32    | 30        |
| Pokemon Shield            | 1.3.2          | SCBlock (`main`)                | 32    | 30        |
| Pokemon Brilliant Diamond | 1.3.0          | Flat binary (`SaveData.bin`)    | 40    | 30        |
| Pokemon Shining Pearl     | 1.3.0          | Flat binary (`SaveData.bin`)    | 40    | 30        |
| Pokemon Legends: Arceus   | 1.1.1          | SCBlock (`main`)                | 32    | 30        |
| Pokemon Scarlet           | 4.0.0          | SCBlock (`main`)                | 32    | 30        |
| Pokemon Violet            | 4.0.0          | SCBlock (`main`)                | 32    | 30        |
| Pokemon Legends: Z-A      | 2.0.2          | SCBlock (`main`)                | 32    | 30        |
| Pokemon FireRed           | 1.0.0          | GBA sectors (`FireRed_*.sav`)   | 14    | 30        |
| Pokemon LeafGreen         | 1.0.0          | GBA sectors (`LeafGreen_*.sav`) | 14    | 30        |

All regional versions of FireRed / LeafGreen are supported: English, Spanish, French, German, Italian, and Japanese. The save file format is identical across all languages - only the title ID and save filename differ.

> **Note:** Moving Pokemon between different games is not supported. Banks are separated by game family because transferred Pokemon would lack the HOME Tracker ID required for cross-game compatibility.

## Features

pkHouse 2.0 redraws every screen: rounded panels, the same top bar and button hints on every screen, and a
details bar under the boxes. Every feature and every piece of information from 1.x is still there, on the
same buttons.

### Profile Selection

On Switch, pkHouse loads all user profiles from the system, each with how many supported saves it has.\
Select your profile, then choose a game.\
Your profile is shown in the top right of every screen that follows.

### Game Selection

The games are a grid of cards, each with its generation and how many banks it has. **L / R** switch
between filters: **All**, **Recent** (backed up in the last 30 days) and one per generation.

The panel on the right describes the highlighted game: whether a save was found, when it was last backed
up, how many backups are on the SD card, its box format, and its banks. Pressing **A** backs the save up
and then opens the bank picker; the top bar follows the steps (Game → Backup → Bank).

Two tiles sit above the grid: **Online GTS** and **All Banks**.

### Two-Panel Box Viewer

In **title override mode**, the main view displays your **game save** on the left and your **bank** on the right, side by side.\
In **applet mode** (bank-only), both panels show **banks**, allowing bank-to-bank transfers.\
Navigate freely between both panels to move Pokemon back and forth.

The **details bar** under the boxes describes the Pokemon under the cursor (or the one you are holding):
sprite, shiny and alpha marks, name, gender and level; nature, ability and ball; all four moves with
their types; how many perfect IVs it has and its EV total; its trainer, origin game and met location.

### View All Banks

Browse banks across all your games from a single screen: the **All Banks** tile above the game grid.\
Banks are grouped by game family (e.g. "Sword / Shield", "Scarlet / Violet"), in the same order as the
game cards. **ZL / ZR** jump from one game to the next, and the highlighted bank's boxes are previewed on
the right before you open it (**L / R** page through them).\
Selecting a bank opens it in dual-bank mode - no save file needed. After picking your first bank, you're prompted to pick a second bank from the same game for side-by-side management.

### Pick & Place

Press **A** on a Pokemon to pick it up, navigate to any slot (in either panel), and press **A** again to place it.\
Placing on an occupied slot swaps the two Pokemon.\
Press **B** to cancel and return the Pokemon to its original slot.\
Press **X** while holding Pokemon to delete them (with confirmation). Useful for clearing boxes of rejected eggs.

### Multi-Select

There are three ways to select Pokemon:

- **Tap Y** on individual Pokemon to toggle selection one by one.
- **Hold Y + D-Pad** to draw a rectangular selection across multiple slots at once.
- **Double-tap Y** to select all Pokemon in the current box (shown in green).

Only occupied slots are selected. Selected slots display a numbered badge showing the pick-up order.

Press **A** to pick up all selected Pokemon, then navigate to another box and press **A** to place them.\
Regular selections (cyan) place Pokemon into the first available empty slots.\
"Select all" selections (green) preserve original positions - each Pokemon is placed at the same slot index in the target box. All matching slots must be empty.\
Press **B** to cancel and return all Pokemon to their original positions.

Selection is cleared when switching boxes or panels.

### Box View

Press **ZL** to open a box overview of all save boxes, or **ZR** for all bank boxes; inside it, ZL / ZR
switch between the two.\
Navigate the grid with the D-Pad. A preview of the highlighted box's contents (Pokemon sprites) appears as you move.\
Press **A** to jump directly to that box, **Y** to rename the box (bank boxes only, max 16 characters), or **B** to cancel.

### Pokemon Details

Press **X** on any Pokemon to open its summary, laid out like a card:

- Sprite, catch ball, species name, nickname, level, gender and National Pokedex number
- Status badges: shiny, alpha, Gigantamax, egg; types, and Tera type on Scarlet / Violet
- Original Trainer (OT) and Trainer ID, origin game, met date and location
- Nature and Ability
- Held item (or "---" if none)
- All 4 moves with their types
- IVs as a hexagonal radar chart (perfect 31s highlighted in gold, with a count badge)
- EVs with their total out of 510
- Ribbons and marks, split into a ribbon count and a mark count (**Up / Down** scroll them)
- Technical data: PID, EC, raw TID/SID, and TSV

From the summary, **X** exports the Pokemon as a `.pk` file, **Y** saves it as a card, and **A** releases
it - held for two seconds, since a released Pokemon is gone.

Sprites are form-aware - alternate forms like Alolan, Galarian, Hisuian, Paldean, Origin, Therian, and many others display their correct sprite.\
Shiny Pokemon use their shiny color sprites. Both apply everywhere: box grid, detail popup, held overlay, box view preview, and wondercard list.\
If a form or shiny sprite is not available, the base sprite is used as fallback.

Shiny names are displayed in gold.\
Alpha Pokemon (Legends: Arceus / Z-A) show a dedicated icon.

Use **L/R** in the detail view to navigate to the next or previous non-empty slot without closing the popup.

### Bank System

Banks are local `.bin` files stored per game family. Paired games share the same bank folder, so you can move Pokemon between versions (e.g. Sword and Shield).

| Game Family | Bank Folder |
|-------------|-------------|
| Let's Go Pikachu / Eevee | `banks/LetsGo/` |
| Sword / Shield | `banks/SwordShield/` |
| Brilliant Diamond / Shining Pearl | `banks/BDSP/` |
| Legends: Arceus | `banks/LegendsArceus/` |
| Scarlet / Violet | `banks/ScarletViolet/` |
| Legends: Z-A | `banks/LegendsZA/` |
| FireRed / LeafGreen | `banks/FireRedLeafGreen/` |

From the bank selector you can:

- **Create** a new bank (up to 32-character name). On Switch, available SD card space is checked before creating - if there isn't enough room, an error is shown. A name that already exists in the game is rejected with a message instead of failing silently (the SD card filesystem is case-insensitive, so `MyBank` and `mybank` count as the same name).
- **Rename** an existing bank - renaming onto a name that already exists is rejected with the same message.
- **Delete** a bank (with confirmation - held for two seconds when the bank still holds Pokemon). This is a **soft delete**: the file is moved to `banks/trash/<GameFamily>/` instead of being erased, so it can be recovered with a file manager. If a bank of the same name is already in the trash, a numeric suffix is added (e.g. `MyBank (2).bin`) so nothing is overwritten.

Each bank has the same box capacity as its game family (32 or 40 boxes).\
The bank list shows how full each bank is and when it was last edited, and the highlighted bank's boxes
are previewed beside the list before you open it.

Bank files are validated when the folder is listed. A `.bin` that isn't a valid pkHouse bank (missing or corrupt header - for example a stray `.bin` copied into the folder by mistake) is shown as **`[INVALID BANK FILE]`** in place of its slot count and cannot be opened.

You can switch between banks from the main view via the menu. Both the save and bank are saved together before switching to prevent data inconsistency.\
If no other bank is available when switching, the app offers to create a new one directly.\
You cannot delete a bank that is currently loaded.

### Backup System

When loading a game save on Switch, an automatic backup is created before any modifications:

```
backups/<profile>/<game>/<profile>_YYYY-MM-DD_HH-MM-SS/
```

This is a full copy of the mounted save directory, made as its own step with a progress bar, and the bank
picker then says where it was written.\
The backup is only created once when initially selecting a game - switching banks does not trigger additional backups.

Before backing up, the app checks available SD card space. If there isn't enough free space (2x the save size), a warning is shown with the option to continue without a backup or cancel. If the backup itself fails, you'll see a similar prompt before proceeding.

### Theme

pkHouse 2.0 has a single design. The 1.x colour themes were retired with the redesign; the **Theme** row in
the menu stays, so new themes can be added later without changing the screens.

### Language

pkHouse supports 9 languages:

| Language | Code |
|----------|------|
| English | en |
| Deutsch | de |
| Español | es |
| Français | fr |
| Italiano | it |
| Nederlands | nl |
| Português | pt |
| Русский | ru |
| 日本語 | ja |

The app automatically detects the Switch system language on startup. You can also override it from the **menu** (+ button): **Left / Right** on the Language row switch languages straight away, and the menu redraws in the new one.\
Your language choice is saved to `language.txt` and persists across sessions. Delete this file to revert to automatic system detection.

Missing keys automatically fall back to English.

> **Note:** Korean (한국어), Simplified Chinese (简体中文), and Traditional Chinese (繁體中文) translation files are included but currently disabled. The system font (`PlSharedFontType_Standard`) does not include CJK/Korean glyphs - enabling these languages requires loading additional system fonts.

### Search / Filter

Search for Pokemon across both panels (save and bank) using the **menu** (+ button → Search).

Available filters:

| Filter | Description |
|--------|-------------|
| Species | One-screen picker: the letters on the left, that letter's species (with sprites) on the right. Only species available in the current game are listed; **Y** picks "any species". |
| OT Name | Text search (substring, case-insensitive) |
| Gender | Any / Male / Female / Genderless |
| Level | Min and max level: **A** types a value, **L / R** pick the end, **ZL / ZR** step it by one |
| Shiny | On / off |
| Egg | On / off |
| Alpha | On / off (Legends: Arceus / Z-A only) |
| Perfect IVs | Off, or at least 1 to 6 perfect IVs |
| Ribbons/Marks | Off / Has Ribbon / Has Mark / Has Either |

The filters sit in two columns: the D-Pad moves between them and **A** changes the one you are on. The
active filters are shown as chips; **X** resets them and **Y** runs the search.

Two result modes are available (the "Show results" row, defaults to Highlight):

- **Highlight** (default): Returns to the box view with matching Pokemon outlined in color and non-matching Pokemon dimmed. The highlight follows Pokemon as you move them between slots. In box view (ZL/ZR), boxes containing matches are outlined and matching slots are highlighted in the preview. Press **B** to clear highlights, or open the menu to start a new search.
- **List**: Results are shown as a scrollable list with the sprite, shiny and alpha marks, species name, gender, level, and where it is (save or bank, box and slot). Press **A** to jump directly to a result in the box view, **L/R** to skip 10 results (hold to auto-repeat), **X** to go back and adjust filters, or **B** to close.

### Wondercard Injection

Inject event wondercard files directly into your save as fully formed Pokemon. Access from the **menu** (+ button → Wondercard).

Place wondercard files in the corresponding folder on your SD card:

| Game Family | Folder | File Extension |
|-------------|--------|----------------|
| Let's Go Pikachu / Eevee | `wondercards/LetsGo/` | `.wb7` / `.wb7full` |
| Sword / Shield | `wondercards/SwordShield/` | `.wc8` |
| Brilliant Diamond / Shining Pearl | `wondercards/BDSP/` | `.wb8` |
| Legends: Arceus | `wondercards/LegendsArceus/` | `.wa8` |
| Scarlet / Violet | `wondercards/ScarletViolet/` | `.wc9` |
| Legends: Z-A | `wondercards/LegendsZA/` | `.wa9` |

The wondercard list shows each card's number, species, level and shiny status, the card's title and region
(read from the file name), and a **YOUR OT** tag on the cards that take your trainer as their OT. The pane
beside it says which slot the Pokemon will go into - and, for a card that takes your OT, that it can only
go into a save, before you press anything. Select a card and press **A** to inject it into the currently
selected box slot; **L / R** skip 10 cards.

The injected Pokemon is fully generated from the wondercard data - PID, IVs, nature, ability, moves, OT, and all metadata are set according to the event's rules, matching official distribution behavior.

Wondercard files can be downloaded from the [Project Pokemon EventsGallery](https://github.com/projectpokemon/EventsGallery) repository.

### Export Pokemon

Export Pokemon as decrypted `.pk` files compatible with PKHeX. Access from the **detail view** (single export) or the **menu** (batch export of selected Pokemon).

Exported files are saved to:

```
export/<GameFamily>/<filename>
```

Filenames follow the PKHeX naming convention: game tag, national dex number, form name, status flags (`[S]`hiny/`[A]`lpha/`[E]`gg), species name, and checksum.

| Game Family | File Extension |
|-------------|----------------|
| Let's Go Pikachu / Eevee | `.pb7` |
| Sword / Shield | `.pk8` |
| Brilliant Diamond / Shining Pearl | `.pb8` |
| Legends: Arceus | `.pa8` |
| Scarlet / Violet | `.pk9` |
| Legends: Z-A | `.pa9` |
| FireRed / LeafGreen | `.pk3` |

### Pokemon Cards

Save any Pokemon as a shareable 1280x720 PNG. Open the **detail view** (X) and press **Y**, or select
several Pokemon and use **menu (+) → Export Cards** to write them all in one pass.

Cards are written to:

```
cards/<GameFamily>/<Dex>-<Form> - <Species> - <GameTag> - [flags] - <EC>.png
```

for example `0111-00 - Rhyhorn - LGPE - [S] - 4FCD97B2.png`. The four-digit Pokedex number and two-digit
form come first so that the import list can show the right sprite before a card is read, and lists cards
in Pokedex order; eggs are `0000-00`. Cards made before 2.0 have no number and still import as before.

The card shows, at a glance:

- Species sprite (form and shiny aware), name in gold when shiny, nickname and form
- Level, gender, catch ball, national dex number
- Status badges: shiny, alpha, Gigantamax, egg
- Primary and secondary types
- Nature (with the stats it raises and lowers), ability, held item, and Tera type on Scarlet / Violet
- All 4 moves with their types
- IVs as a hexagonal radar chart, with perfect 31s picked out in gold and a count badge
- EVs as bars with the running total out of 510
- Original Trainer, origin game, met date, met location, and language
- Ribbons and marks
- A QR code carrying the entire Pokemon

The card is painted in a fixed palette of its own rather than following the app's theme: a card gets
shared, and it should look the same wherever it lands. The QR tile is the one exception - it keeps a white ground and black modules
whatever the rest of the card is painted in, because scanners need real black on real white.

> **Note:** Only the public Trainer ID is printed on the card. The secret ID is deliberately left off,
> since it would let anyone who sees the card work out the trainer's shiny frames. It is still present
> in the QR code, which carries the raw data, so treat a card you post publicly as you would the `.pk`
> file itself.

#### The QR code

The QR code beside the sprite contains the complete decrypted Pokemon, so a card can be decoded straight
back into a bank slot. Image metadata was the obvious place to put it, but chat apps and image hosts
strip or re-encode it; a QR code survives re-encoding, resizing, and a photo of a screen.

| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | Magic `PKHC` |
| 4 | 1 | Payload format version (currently 1) |
| 5 | 1 | `GameType` enum value - tells the reader which PKM format the body is in |
| 6 | 2 | Body length in bytes, little endian |
| 8 | 2 | CRC-16/CCITT-FALSE over the body, little endian |
| 10 | N | Decrypted party-size Pokemon data, byte for byte what the `.pk` export writes |

Payloads run from 110 bytes (FireRed / LeafGreen) to 386 (Legends: Arceus), which encodes to a version 7
to version 15 QR at ECC level M. The code is drawn at a whole number of pixels per module - never scaled
up afterwards, which is what actually breaks scanners - giving at least 4 pixels per module and a 4 module
quiet zone for every supported game.

Encoding uses [qrcodegen](https://www.nayuki.io/page/qr-code-generator-library) by Project Nayuki (MIT),
vendored as `source/qrcodegen.c` and `include/qrcodegen.h`. The payload format lives in
`include/card_payload.h`.

Cards are filed per game family, using the same folder names as `banks/`, `export/` and `wondercards/`,
so a card can only ever belong to the family it was exported from.

If encoding ever fails, the card is still written: the sprite simply takes the whole panel instead of
sharing it with the code.

### Importing Cards

Cards can be read back in. Open the **menu** (+ button) and choose **Import Card** to browse
`cards/<GameFamily>/`.

The browser shows the file list on the left and, on the right, **what the highlighted card actually
contains** - sprite, species, level, gender, nature, ability, held item, all four moves with their types,
IVs, OT and origin game. All of it is read out of the QR code, never from the filename, so the pane shows
the Pokemon you would really be importing.

The list itself is labelled from the filename, so it stays quick however many cards there are: the name,
the game tag and the shiny / alpha / egg marks, with the sprite taken from the Pokedex number a 2.0 card
starts with. Once a card has been read, its row shows what it holds - species, gender, level, trainer and
game.

A card that has been resized or re-cropped outside those proportions cannot be read that cheaply; the pane
says **Press A to read this card** and the full-image search runs when you commit.

Press **A** and pkHouse decodes the whole image, validates the payload, and shows the Pokemon in the
**full detail view** - IV and EV radar charts, ribbons, technical data - with **A: Import  B: Cancel**.
Nothing is written until you confirm.

An imported Pokemon goes through exactly the same path as a bank-to-save move, so the handling trainer is
updated as an in-game trade would and the Pokedex is registered automatically.

If the target slot is already occupied, the import is refused rather than overwriting it.

#### Card Validation

A card is checked in this order before anything is written:

| Check | Rejects |
|-------|---------|
| Magic `PKHC` | An image with no pkHouse card data |
| Payload format version | A card written by a newer pkHouse |
| Body length vs the length field | Truncated or padded data |
| CRC-16 over the body | Corruption, partial downloads |
| `GameType` byte in range | A damaged game field |
| Body length vs the format's party size | Data that is not the size that game uses |
| **Game family match** | **A card belonging to another game** |
| The checksum the games themselves store | Data damaged inside the Pokemon |
| Species and form present in the target game | Blobs that decode but cannot exist there |
| Range checks on nature, ball, level, EV total, language, met date | Data read through the wrong offsets |

The **game family match** is the one that matters. The folder a card sits in is a convenience for
listing, never a guarantee: the payload carries the family it was exported from, that byte is covered by
the CRC, and it is what gets compared. This has to be a hard gate, because placing a Pokemon writes it
through the open game's field offsets - a Sword Pokemon written into a Scarlet save would be read with
Gen 9 offsets and corrupted.

Family, not exact version: a Sword card imports into a Shield bank, the same rule banks already follow.

> **Tip:** If you drop a card into the wrong game folder, pkHouse tells you which family it belongs to and
> offers to move the file there for you. Nothing is overwritten - a name that is already taken gets a
> numeric suffix.

Decoding uses [quirc](https://github.com/dlbeer/quirc) by Daniel Beer (ISC). A card that a chat app has
downscaled below roughly two pixels per QR module will not decode; sharing the original file always works.

### Pokedex Auto-Registration

When transferring Pokemon from a bank to a save file, the Pokedex is automatically updated to register the Pokemon as caught. This solves the issue where version-exclusive Pokemon transferred between paired games would not appear in the Pokedex.

| Game Family                       | What gets registered                                                                                             |
|-----------------------------------|------------------------------------------------------------------------------------------------------------------|
| Scarlet / Violet (+ DLCs)         | Caught, seen, heard, gender, language (Pokemon + save), shiny, display, neighbor discovery                       |
| Sword / Shield (+ DLCs)           | Caught, seen (all gender/shiny regions), language, display, battled count, Gigantamax, Alcremie forms, Eternatus |
| Brilliant Diamond / Shining Pearl | Caught, gender, shiny, language, alternate form tracking (Unown, Rotom, Arceus, etc.)                            |
| Let's Go Pikachu / Eevee          | Caught, seen, display, language, size tracking (height/weight), capture count                                    |
| FireRed / LeafGreen               | Caught, seen (with all 3 copy sync), Unown/Spinda PID                                                            |

> **_Legends: Arceus_** and **_Legends: Z-A_** are excluded from auto-registration as all Pokemon are obtainable in a single playthrough without trading.

Pokedex completion rewards (Shiny Charm, diplomas) are triggered by the in-game NPC when the dex is complete.

### Handling Trainer Updates

When a Pokemon is placed into a save file, its handling trainer (HT) data is updated exactly as an in-game trade would.

The games compare the full trainer identity (TID/SID, name, gender, game version), not just the trainer name, so a Pokemon moved to a save with the same OT name but different IDs is correctly treated as traded.

- If the Pokemon **belongs to the save's trainer**, it is marked as being back with its OT.
- If it belongs to a **different trainer**, the save's trainer is registered as its handling trainer: HT name, gender, and language are set, HT friendship is reset to the species' base value, and stale HT memories are cleared. The original OT data is never modified.
- **No friendship loss on round-trips**: re-placing a Pokemon into a save whose trainer is already its registered handler keeps the HT friendship it has earned.
- **Eggs** traded to a different trainer are marked as link-traded with the current date, as the games do.
- **Let's Go**: friendship is carried over instead of reset, so CP values are unaffected.

This keeps friendship routing (e.g. friendship evolutions), memories, and legality checks correct after transferring Pokemon between save files with the bank.

> **_FireRed / LeafGreen_** are unaffected - Gen 3 has no handling trainer concept, so no changes are needed there.

### Online GTS

A public board for cards: leave a Pokemon on it for anyone to take, and take what other people have left.
Reached from the **Online GTS** tile above the game grid, before a game is picked - the board carries every
game at once, and what you take off it is saved as a card rather than dropped into a save, so it can be
imported into whichever game you open next.

**Browse** shows 60 Pokemon a page, laid out like a box (12 × 5), with **L / R** paging through the board.
The details bar under the grid describes the listing under the cursor from what the board already sent,
without downloading it: sprite, shiny and alpha marks, name, gender and level; nature, ability and ball;
its legality and the game it belongs to; its IV and EV totals; its trainer, and the depositor's note or
how many people have kept it. **A** opens the full summary - the same one the boxes use, with the game
shown in the corner - and **Y** there saves it into `cards/<GameFamily>/`.

**Search** filters the board:

| Filter | Choices |
|--------|---------|
| Species | Any, or one species (the box search's picker) |
| Game | Any, or one game family |
| Ball | Any, or one ball (**A** steps forward, **L / R** step back and forth) |
| Min IV total | Any / 90 / 120 / 150 / 180 / 186 |
| Shiny | Any / Yes / No |
| Egg | Any / Yes / No |
| Alpha | Any / Yes / No (only with Any game, Legends: Arceus or Z-A) |
| Legality | **Checked** (default) / Legal only / Everything |
| Sort by | Newest / Most downloaded |

As in the box search, the D-Pad moves between the filters, **A** changes the one you are on, **X** resets
them and **Y** searches.

**Deposit** lists the cards you have already exported, from every game folder at once, grouped by game in
the game list's order (**ZL / ZR** jump between games). The highlighted card is read and shown in full, and
it is read again in full before anything is uploaded. Depositing the same Pokemon twice does nothing - the
board deduplicates on a hash of the payload, so a blob that was taken down cannot be re-listed as though it
were new.

#### Legality

A deposit is listed the moment it arrives, marked as not yet checked.

Judging a Pokemon properly means encounter and RNG analysis: a legality script goes over the board every 15 minutes and writes each verdict back.

- By default the board shows what has been checked: legal Pokemon, and illegal or unverifiable ones too,
  so a depositor can see why theirs was refused. **Legal only** hides those; **Everything** also shows
  what is still waiting to be checked
- A listing that is not legal is framed in the colour of its verdict
- **X** - or **A** on an illegal listing - shows the checker's full report
- **Only a Pokemon that passed can be downloaded.** An illegal, unchecked or unverifiable one stays
  listed to look at, but the board will not hand it over, even if a verdict lands while you are looking
  at it

#### Your identity on the board

The board attributes a deposit to an id kept in `sdmc:/config/pkHouse/gts_id.txt`, made on first use.

> **Warning:** a deposit is public. Anyone can browse it, download it and keep it, and taking it off the
> board later does not reach copies people already have.

### Updates

pkHouse checks for a new release when it starts, in the background, so a slow or missing network
never holds the screen up. When there is one, it is offered on the profile or game selector - the only
places nothing is open that could have unsaved changes.

The offer is made once per launch. Turned down, it does not come back until the next launch - unless you
check again yourself from the About screen (see below), which offers it again.

> **Updating from 1.12.0 or earlier:** those versions cannot update themselves, so 2.0.0 has to be
> installed by hand, once: extract the release zip to the root of the SD card. From 2.0.0 on, pkHouse
> updates itself.

Installing is built around never destroying the only working copy:

1. The release zip is downloaded to `sdmc:/config/pkHouse/`, with a progress bar
2. The NRO inside it is unpacked first and its version checked against the release; a wrong or broken
   download changes nothing
3. The other files the release carries under `switch/pkHouse/` (the bundled wondercards) are written into
   the app's folder, each through a temporary file. Nothing the release does not carry is touched -
   banks, backups and cards included
4. The running NRO is backed up, replaced, and read back; if it does not verify, the backup is restored
5. pkHouse restarts straight into the new version

If any step fails, an **Update failed** message says why, and the version you are running stays in place:
nothing is replaced until the new build has been checked, and a replacement that does not verify is
rolled back from the backup.

The **About** screen (**−**) shows the check's result beside the version (up to date, a newer version
available, or that it could not check), **X** checks right away, and **Y** turns the check at launch on or
off.

### Quitting

Leaving the app always asks first - **+** opens the menu in the boxes but quits on the selectors and in
the GTS, so one press in the wrong place must not end the session. With unsaved changes, the "discard your
changes?" question is the confirmation.

### LED Activity Indicator

The controller notification LED blinks during save and backup operations (save writes, bank saves, backup creation) to provide visual feedback that data is being written.

- **Standard / OLED Switch**: The Joy-Con or Pro Controller LED pulses briefly during writes.
- **Switch Lite**: The built-in motherboard notification LED lights up during writes and turns off when the operation completes.

This works automatically with no configuration required.

> To disable the LED Indicator, place a `noled.cfg` file in the pkHouse folder (next to the NRO). Remove it to re-enable. The file can be empty - only its presence is checked.

### Save Integrity

- **SCBlock saves** (ZA, SV, SwSh, PLA): Decrypted, modified, and re-encrypted. A round-trip verification runs on load to confirm the cycle is lossless.
- **BDSP saves**: Flat binary with MD5 checksum, recalculated on every save.
- **LGPE saves**: BEEF block format with CRC16 checksums, recalculated on every save. Storage is compacted before writing.
- **FRLG saves**: GBA sector-based format (128KB, two save slots). CheckSum32 recalculated per sector on save. Both slots are updated.
- All saves are written in-place to preserve the Switch filesystem journal.

## Controls

Every screen shows its buttons in the footer; these tables are the same, in one place.

### Profile Selector

| Button | Action |
|--------|--------|
| D-Pad Left/Right | Navigate profiles |
| A | Select profile |
| - | About |
| + | Quit (asks first) |

### Game Selector

| Button | Action |
|--------|--------|
| D-Pad | Navigate the game grid and the Online GTS / All Banks tiles |
| A | Back up the save and choose a bank / Open the online GTS / Open All Banks |
| L / R | Previous / next filter (All, Recent, one per generation) |
| B | Back to profile selector (in applet mode: quit, asks first) |
| - | About |
| + | Quit (asks first) |

### Bank Selector

| Button | Action |
|--------|--------|
| D-Pad Up/Down | Navigate bank list |
| A | Open bank |
| X | Create new bank |
| Y | Rename bank |
| + | Delete bank (hold A to confirm when it still holds Pokemon) |
| L / R | Page through the previewed bank's boxes |
| ZL / ZR | Jump to the previous / next game (All Banks) |
| B | Back (main view if bank loaded, otherwise game selector) |
| - | About |

When switching banks, the selector appears on the side being switched while the other panel remains visible.

### Main View

| Button | Action |
|--------|--------|
| D-Pad | Move cursor |
| L / R | Switch box (hold to repeat) |
| ZL / ZR | Box overview (save / bank) |
| A | Pick up / Place Pokemon |
| B | Cancel / Return held Pokemon |
| Y | Toggle multi-select (hold + D-Pad to draw, double-tap for the whole box) |
| X | Pokemon summary / Delete held Pokemon |
| + | Open menu |
| - | About |

### Pokemon Summary

| Button | Action |
|--------|--------|
| L / R | Previous / next Pokemon in the box |
| Up / Down | Scroll ribbons and marks |
| X | Export as `.pk` |
| Y | Save as a PNG card |
| A (hold) | Release |
| B | Close |

### Online GTS

| Button | Action |
|--------|--------|
| D-Pad | Move between Browse, Search and Deposit / move the board's cursor |
| A | Select / open a listing (on an illegal listing: its report) |
| X | The legality report of the listing under the cursor |
| L / R | Previous / next page of the board |
| Y | Save the open listing as a card |
| ZL / ZR | Jump to the previous / next game (Deposit) |
| B | Back |
| + | Quit (asks first, hub and board) |

### About

| Button | Action |
|--------|--------|
| X | Check for updates now |
| Y | Turn the update check at launch on / off |
| B / - | Close |

### Menu (Title Override Mode)

The menu has two columns: tools and bank switching on the left, settings and every way out on the right,
from the safest to the most final.

| Option | Description |
|--------|-------------|
| Search | Search for Pokemon across both panels |
| Wondercard | Inject event wondercards as Pokemon (supported games only) |
| Export Selected | Export selected Pokemon as `.pk` files (shown when Pokemon are selected) |
| Export Cards | Export selected Pokemon as PNG cards (shown when Pokemon are selected) |
| Import Card | Browse `cards/<GameFamily>/` and import a Pokemon from a card PNG |
| Switch Bank | Save game and bank, return to bank selector |
| Theme | The theme (one for now); **Left / Right** switch it |
| Language | **Left / Right** switch the language straight away |
| Save & Quit | Save everything and exit (asks first) |
| Change Game | Save everything, return to game selector |
| Change game without saving | Return to game selector, dropping unsaved changes (asks first when there are any) |
| Quit Without Saving | Exit without saving changes (asks first) |

### Menu (Applet / Bank-Only Mode)

| Option | Description |
|--------|-------------|
| Search | Search for Pokemon across both panels |
| Wondercard | Inject event wondercards as Pokemon (supported games only) |
| Export Selected | Export selected Pokemon as `.pk` files (shown when Pokemon are selected) |
| Export Cards | Export selected Pokemon as PNG cards (shown when Pokemon are selected) |
| Import Card | Browse `cards/<GameFamily>/` and import a Pokemon from a card PNG |
| Switch Bank (Left) | Save both banks, switch the left bank |
| Switch Bank (Right) | Save both banks, switch the right bank |
| Theme | The theme (one for now); **Left / Right** switch it |
| Language | **Left / Right** switch the language straight away |
| Save Banks | Save both banks |
| Change Game | Save both banks, return to game selector |
| Change game without saving | Return to game selector, dropping unsaved changes (asks first when there are any) |
| Quit Without Saving | Exit without saving the banks (asks first) |

## Building

### Prerequisites

- [devkitPro](https://devkitpro.org/) with the devkitA64 toolchain
- Switch portlibs: SDL2, SDL2_image, SDL2_ttf, curl with mbedTLS for the online GTS and updates, and
  zlib with minizip for opening release zips

```bash
dkp-pacman -S switch-sdl2 switch-sdl2_image switch-sdl2_ttf switch-freetype switch-harfbuzz \
             switch-curl switch-mbedtls switch-zlib
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

Extract the release zip to the root of your SD card (it holds `switch/pkHouse/pkHouse.nro` and the bundled
wondercards), and launch via a homebrew launcher. From then on pkHouse updates itself (see **Updates**).

- **Title override mode**: Full access - game save on the left, bank on the right. Requires launching through a game title.
- **Applet mode** (album/homebrew menu): Bank-only access - two banks side by side for bank-to-bank transfers. Save data is not accessible in this mode. Use title override mode to transfer Pokemon between your save and a bank.

## Screenshots

### Getting in

|  |  |
|:--:|:--:|
| <img src="screenshots/2.0/01-profile-selector.jpg" width="420"> | <img src="screenshots/2.0/02-loading-progress.jpg" width="420"> |
| Profile selector | Long operations show their progress |
| <img src="screenshots/2.0/03-game-selector-gts.jpg" width="420"> | <img src="screenshots/2.0/04-game-selector-detail.jpg" width="420"> |
| Game selector, with the Online GTS and All banks | A game's save, last backup and banks |
| <img src="screenshots/2.0/05-backing-up.jpg" width="420"> | <img src="screenshots/2.0/06-choose-bank.jpg" width="420"> |
| The save is backed up before any bank opens | Choosing a bank, the save beside it |

### Banks

|  |  |
|:--:|:--:|
| <img src="screenshots/2.0/07-all-banks.jpg" width="420"> | <img src="screenshots/2.0/08-all-banks-preview.jpg" width="420"> |
| All banks, grouped by game | A bank's boxes, previewed before opening it |

|  |
|:--:|
| <img src="screenshots/2.0/09-dual-bank-picker.jpg" width="866"> |
| Dual bank: picking the left bank |

### Moving Pokemon

|  |  |
|:--:|:--:|
| <img src="screenshots/2.0/10-box-view-save-bank.jpg" width="420"> | <img src="screenshots/2.0/11-box-view-dual-bank.jpg" width="420"> |
| Save on the left, bank on the right, the details bar under them | Two banks side by side |

|  |  |
|:--:|:--:|
| <img src="screenshots/2.0/12-box-overview.jpg" width="420"> | <img src="screenshots/2.0/13-pokemon-summary.jpg" width="420"> |
| Box overview (ZL / ZR), with a preview of the highlighted box | The Pokemon summary (X) |

### Search, wondercards and cards

|  |  |
|:--:|:--:|
| <img src="screenshots/2.0/14-search.jpg" width="420"> | <img src="screenshots/2.0/15-wondercards.jpg" width="420"> |
| Search | Wondercards |

|  |  |
|:--:|:--:|
| <img src="screenshots/2.0/16-import-card.jpg" width="420"> | <img src="screenshots/1.0/015.png" width="420"> |
| Importing a card, read from its QR code | A card exported from the detail view |

### Menu and About

|  |  |
|:--:|:--:|
| <img src="screenshots/2.0/17-menu.jpg" width="420"> | <img src="screenshots/2.0/18-menu-dual-bank.jpg" width="420"> |
| Menu, with a save and a bank | Menu, in dual bank mode |

|  |
|:--:|
| <img src="screenshots/2.0/19-about.jpg" width="866"> |
| About, with the update check |

### The online GTS

|  |  |
|:--:|:--:|
| <img src="screenshots/2.0/20-gts-hub.jpg" width="420"> | <img src="screenshots/2.0/21-gts-board.jpg" width="420"> |
| Browse, search or deposit | The board: 60 listings a page, the details bar under them |
| <img src="screenshots/2.0/22-gts-search.jpg" width="420"> | <img src="screenshots/2.0/23-gts-deposit.jpg" width="420"> |
| Searching the board | Picking a card to deposit, read before it is sent |

## Credits

- [PKHeX](https://github.com/kwsch/PKHeX) by kwsch - PokeCrypto research and save structure reference
- [JKSV](https://github.com/J-D-K/JKSV) by J-D-K - Save backup and write logic reference
- [QR Code generator library](https://www.nayuki.io/page/qr-code-generator-library) by Project Nayuki (MIT) - QR encoding for Pokemon cards
- [quirc](https://github.com/dlbeer/quirc) by Daniel Beer (ISC) - QR decoding for card import
- Built with [libnx](https://github.com/switchbrew/libnx) and [SDL2](https://www.libsdl.org/)
