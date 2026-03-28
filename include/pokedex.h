#pragma once

// Pokedex registration — updates the save file's Pokedex when a Pokemon is
// placed into a box slot.  Ported from PKHeX.Core Zukan* classes.
//
// Supported games:
//   ZA       — Zukan9a   (SCBlock 0x2D87BE5C, 0x84-byte entries)
//   SV       — Zukan9    (Paldea 0x0DEAAEBD / Kitakami 0xF5D7C0E2)
//   SwSh     — Zukan8    (SCBlock 0x4716c404, 0x30-byte entries)
//   FRLG     — SAV3      (sector-based caught/seen bitfields)
//   BDSP     — Zukan8b   (flat binary, offset 0x7A328)
//   LGPE     — Zukan7b   (flat binary, block at 0x02A00)
//   LA       — skipped   (research-task based, not applicable)

struct Pokemon;
class SaveFile;

namespace Pokedex {

// Register a Pokemon in the save file's Pokedex.
// Skips eggs and empty slots.  Safe to call for any GameType.
void registerPokemon(SaveFile& save, const Pokemon& pkm);

} // namespace Pokedex
