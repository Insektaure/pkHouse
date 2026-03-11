#include "entity_converter.h"
#include "species_converter.h"
#include "personal_za.h"
#include "personal_sv.h"
#include "personal_swsh.h"
#include "personal_la.h"
#include "personal_bdsp.h"
#include "personal_gg.h"
#include <cstring>
#include <algorithm>

// --- Legality constants (from PKHeX Legal.cs) ---

static constexpr uint16_t MaxSpeciesID_FRLG = 386;
static constexpr uint16_t MaxSpeciesID_LGPE = 809; // national dex up to Melmetal
static constexpr uint16_t MaxSpeciesID_SWSH = 898; // Calyrex
static constexpr uint16_t MaxSpeciesID_BDSP = 493; // Arceus
static constexpr uint16_t MaxSpeciesID_LA   = 905; // Enamorus
static constexpr uint16_t MaxSpeciesID_SV   = 1025; // Pecharunt
static constexpr uint16_t MaxSpeciesID_ZA   = 1025; // Gholdengo (same range)

// Species constants
static constexpr uint16_t Species_Pikachu    = 25;
static constexpr uint16_t Species_Eevee      = 133;
static constexpr uint16_t Species_Nincada    = 290;
static constexpr uint16_t Species_Spinda     = 327;
static constexpr uint16_t Species_Koraidon   = 1007;
static constexpr uint16_t Species_Miraidon   = 1008;

// --- Species presence check ---

bool EntityConverter::isPresentInGame(GameType game, uint16_t species, uint8_t form) {
    if (species == 0) return false;

    if (isFRLG(game))
        return species <= MaxSpeciesID_FRLG;

    if (isLGPE(game)) {
        // LGPE: first 151 + Meltan (808) + Melmetal (809) + Alolan forms
        return PersonalGG::isPresentInGame(species, form);
    }

    if (isSwSh(game))
        return PersonalSWSH::isPresentInGame(species, form);

    if (isBDSP(game))
        return PersonalBDSP::isPresentInGame(species, form);

    if (game == GameType::LA)
        return PersonalLA::isPresentInGame(species, form);

    if (isSV(game))
        return PersonalSV::isPresentInGame(species, form);

    // ZA
    return PersonalZA::isPresentInGame(species, form);
}

// --- Outbound restrictions (from EntityConverter.cs) ---

static ConvertResult checkOutbound(const Pokemon& src, GameType srcGame) {
    uint16_t species = src.species();
    uint8_t form = src.form();

    // PB7: Pikachu/Eevee with non-zero form can't leave
    if (isLGPE(srcGame)) {
        if (species == Species_Pikachu && form != 0)
            return ConvertResult::IncompatibleForm;
        if (species == Species_Eevee && form != 0)
            return ConvertResult::IncompatibleForm;
    }

    // PB8: Spinda and Nincada can't leave
    if (isBDSP(srcGame)) {
        if (species == Species_Spinda)
            return ConvertResult::IncompatibleSpecies;
        if (species == Species_Nincada)
            return ConvertResult::IncompatibleSpecies;
    }

    // PK9: Koraidon/Miraidon ride forms can't leave
    if (isSV(srcGame)) {
        if ((species == Species_Koraidon || species == Species_Miraidon)) {
            // FormArgument at 0xD0 in PK9/PA9 — non-zero means ride form
            uint32_t formArg;
            std::memcpy(&formArg, src.data.data() + 0xD0, 4);
            if (formArg != 0)
                return ConvertResult::IncompatibleForm;
        }
    }

    return ConvertResult::Success;
}

// --- Inbound restrictions ---

static ConvertResult checkInbound(const Pokemon& src, GameType destGame) {
    uint16_t species = src.species();
    uint8_t form = src.form();

    // PB8 can't receive Spinda or Nincada
    if (isBDSP(destGame)) {
        if (species == Species_Spinda)
            return ConvertResult::IncompatibleSpecies;
        if (species == Species_Nincada)
            return ConvertResult::IncompatibleSpecies;
    }

    // FRLG: only species 1-386
    if (isFRLG(destGame)) {
        if (species > MaxSpeciesID_FRLG)
            return ConvertResult::IncompatibleSpecies;
    }

    // Check species+form presence in destination game
    if (!EntityConverter::isPresentInGame(destGame, species, form))
        return ConvertResult::IncompatibleSpecies;

    return ConvertResult::Success;
}

// --- Public API ---

ConvertResult EntityConverter::canTransfer(const Pokemon& src, GameType srcGame, GameType destGame) {
    if (src.isEmpty())
        return ConvertResult::IncompatibleSpecies;

    // Same game family — no conversion needed
    if (srcGame == destGame)
        return ConvertResult::Success;

    ConvertResult r = checkOutbound(src, srcGame);
    if (r != ConvertResult::Success)
        return r;

    r = checkInbound(src, destGame);
    if (r != ConvertResult::Success)
        return r;

    return ConvertResult::Success;
}

const char* EntityConverter::resultString(ConvertResult r) {
    switch (r) {
        case ConvertResult::Success:             return "Success";
        case ConvertResult::IncompatibleSpecies:  return "Species not available in target game";
        case ConvertResult::IncompatibleForm:     return "Form not compatible with transfer";
        case ConvertResult::NoTransferRoute:      return "No transfer route available";
    }
    return "Unknown error";
}

// --- Conversion helpers ---

// Read/write helpers for arbitrary buffers
static uint16_t rd16(const uint8_t* d, int ofs) {
    uint16_t v; std::memcpy(&v, d + ofs, 2); return v;
}
static uint32_t rd32(const uint8_t* d, int ofs) {
    uint32_t v; std::memcpy(&v, d + ofs, 4); return v;
}
static void wr16(uint8_t* d, int ofs, uint16_t v) {
    std::memcpy(d + ofs, &v, 2);
}
static void wr32(uint8_t* d, int ofs, uint32_t v) {
    std::memcpy(d + ofs, &v, 4);
}

// --- Dummied move bitfields per game (from PKHeX.Core MoveInfo*.cs) ---
// If a bit is set, the move is not usable in that game.

// SwSh (Gen8) — MoveInfo8.cs
static const uint8_t SWSH_DUMMIED_MOVES[93] = {
    0x1C, 0x20, 0x00, 0x0C, 0x00, 0x02, 0x02, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x09, 0x00, 0xA1, 0x22, 0x19, 0x10, 0x36, 0xC0,
    0x40, 0x0A, 0x00, 0x02, 0x02, 0x00, 0x00, 0x45, 0x10, 0x20,
    0x00, 0x00, 0x00, 0x02, 0x04, 0x80, 0x66, 0x70, 0x00, 0x50,
    0x91, 0x00, 0x00, 0x04, 0x64, 0x08, 0x20, 0x67, 0x84, 0x00,
    0x00, 0x00, 0x00, 0xA4, 0x00, 0x28, 0x03, 0x01, 0x07, 0x20,
    0x22, 0x00, 0x04, 0x08, 0x10, 0x00, 0x08, 0x02, 0x08, 0x00,
    0x08, 0x02, 0x00, 0x00, 0x02, 0x01, 0x00, 0xE2, 0xFF, 0xFF,
    0xFF, 0xFF, 0x07, 0x82, 0x01, 0x40, 0x84, 0xFF, 0x00, 0x80,
    0xF8, 0xFF, 0x3F,
};

// BDSP (Gen8b) — MoveInfo8b.cs
static const uint8_t BDSP_DUMMIED_MOVES[104] = {
    0x1C, 0x20, 0x00, 0x0C, 0x00, 0x02, 0x02, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x09, 0x00, 0xA1, 0x22, 0x19, 0x10, 0x26, 0xC0,
    0x00, 0x0A, 0x00, 0x02, 0x02, 0x00, 0x00, 0x45, 0x10, 0x00,
    0x00, 0x00, 0x00, 0x02, 0x04, 0x80, 0x26, 0x70, 0x00, 0x50,
    0x91, 0x00, 0x00, 0x04, 0x60, 0x08, 0x20, 0x67, 0x04, 0x00,
    0x00, 0x00, 0x00, 0x24, 0x00, 0x28, 0x00, 0x01, 0x04, 0x20,
    0x22, 0x00, 0x04, 0x18, 0xD0, 0x81, 0xB8, 0xAA, 0xFF, 0xE7,
    0x8B, 0x0E, 0x45, 0x98, 0x07, 0xCB, 0xE4, 0xE3, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xA7, 0x74, 0xEB, 0xAF, 0xFF, 0xB7, 0xF7,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0x7F, 0xFF,
    0xFF, 0xFF, 0xFF, 0x07,
};

// PLA (Gen8a) — MoveInfo8a.cs
static const uint8_t PLA_DUMMIED_MOVES[104] = {
    0x7E, 0xBC, 0xFE, 0xFF, 0xBD, 0xEA, 0xCF, 0x72, 0x7F, 0x1F,
    0x0E, 0x1F, 0xAB, 0xFD, 0xEF, 0xBE, 0x7D, 0xD7, 0x35, 0xCF,
    0xD5, 0xEF, 0x5F, 0x0F, 0xEF, 0x9E, 0xFD, 0xFF, 0x7E, 0x5F,
    0x3B, 0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xDF, 0xBF, 0xB3,
    0xBF, 0xAF, 0xF5, 0xE4, 0xF6, 0xFF, 0xFB, 0xFF, 0xFF, 0x2B,
    0x84, 0x8C, 0x08, 0xA0, 0xDB, 0xAA, 0xC5, 0x21, 0xF0, 0xFB,
    0xFF, 0xF7, 0xFF, 0xFB, 0xFF, 0xF3, 0xFE, 0xBF, 0xFF, 0xE7,
    0xFF, 0xFF, 0x7D, 0xFC, 0xF7, 0xDF, 0xFE, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xB7, 0xFF, 0xFF, 0xFF, 0xFF, 0xBF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xEF,
    0xFF, 0xFF, 0xFF, 0x07,
};

// SV/ZA (Gen9) — MoveInfo9.cs
static const uint8_t SV_DUMMIED_MOVES[100] = {
    0x1C, 0x20, 0x00, 0x0C, 0x00, 0x02, 0x02, 0x00, 0x04, 0x00,
    0x04, 0x00, 0x09, 0x00, 0xA1, 0x22, 0x5D, 0x50, 0x36, 0xC8,
    0x00, 0x0E, 0x00, 0x42, 0x02, 0x00, 0x00, 0x45, 0x10, 0x22,
    0x00, 0x00, 0x04, 0x0A, 0xA4, 0x80, 0x27, 0x70, 0x00, 0x51,
    0x91, 0x00, 0x00, 0x04, 0x60, 0x08, 0xA0, 0x67, 0x04, 0x00,
    0x00, 0x00, 0x00, 0xA4, 0x00, 0x28, 0x01, 0x01, 0x04, 0x28,
    0x23, 0x00, 0x04, 0x08, 0x10, 0x00, 0x0C, 0x83, 0x07, 0x00,
    0x8A, 0x02, 0x4C, 0x10, 0x80, 0x03, 0xF0, 0xC3, 0xFF, 0xFF,
    0xFF, 0xFF, 0x07, 0x80, 0x26, 0xA0, 0x80, 0xFF, 0x11, 0xE1,
    0xFB, 0xFF, 0xFF, 0x00, 0xEE, 0xFF, 0x7F, 0x08, 0x00, 0x0D,
};

// LGPE (Gen7b) — AllowedMoves whitelist from MoveInfo7b.cs (converted to bitfield)
// Moves 0-164 are all allowed, plus selected higher moves.
static const uint16_t LGPE_ALLOWED_MOVES[] = {
    182, 188, 200, 224, 227, 231, 242, 243, 247, 252,
    257, 261, 263, 269, 270, 276, 280, 281, 339, 347,
    355, 364, 369, 389, 394, 398, 399, 403, 404, 405,
    406, 417, 420, 430, 438, 446, 453, 483, 492, 499,
    503, 504, 525, 529, 583, 585, 603, 605, 606, 607,
    729, 730, 731, 733, 734, 735, 736, 737, 738, 739,
    740, 742,
};

static bool isMoveDummiedInBitfield(uint16_t moveId, const uint8_t* bf, int bfSize) {
    int byteIdx = moveId >> 3;
    int bitIdx  = moveId & 7;
    if (byteIdx >= bfSize)
        return true; // out of range = invalid
    return (bf[byteIdx] >> bitIdx) & 1;
}

static bool isMoveDummiedInPLA(uint16_t moveId) {
    return isMoveDummiedInBitfield(moveId, PLA_DUMMIED_MOVES, sizeof(PLA_DUMMIED_MOVES));
}

static bool isMoveAllowedInLGPE(uint16_t moveId) {
    if (moveId <= 164) return true;
    for (auto m : LGPE_ALLOWED_MOVES)
        if (m == moveId) return true;
    return false;
}

static bool isMoveDummiedForGame(uint16_t moveId, GameType game) {
    if (moveId == 0) return false; // empty slot is fine
    if (isSwSh(game))
        return isMoveDummiedInBitfield(moveId, SWSH_DUMMIED_MOVES, sizeof(SWSH_DUMMIED_MOVES));
    if (isBDSP(game))
        return isMoveDummiedInBitfield(moveId, BDSP_DUMMIED_MOVES, sizeof(BDSP_DUMMIED_MOVES));
    if (game == GameType::LA)
        return isMoveDummiedInBitfield(moveId, PLA_DUMMIED_MOVES, sizeof(PLA_DUMMIED_MOVES));
    if (isLGPE(game))
        return !isMoveAllowedInLGPE(moveId);
    // SV/ZA (Gen9)
    if (isSV(game) || game == GameType::ZA)
        return isMoveDummiedInBitfield(moveId, SV_DUMMIED_MOVES, sizeof(SV_DUMMIED_MOVES));
    return false;
}

// Filter dummied moves for any game (PK9-format: moves at moveOfs, PP at moveOfs+8)
static void filterDummiedMoves(uint8_t* data, int moveOfs, GameType game) {
    for (int i = 0; i < 4; i++) {
        uint16_t move = rd16(data, moveOfs + i * 2);
        if (move != 0 && isMoveDummiedForGame(move, game)) {
            wr16(data, moveOfs + i * 2, 0);
            data[moveOfs + 8 + i] = 0;
        }
    }
    // Compact: shift non-zero moves to front
    uint16_t moves[4];
    uint8_t pp[4];
    for (int i = 0; i < 4; i++) {
        moves[i] = rd16(data, moveOfs + i * 2);
        pp[i] = data[moveOfs + 8 + i];
    }
    int w = 0;
    for (int r = 0; r < 4; r++) {
        if (moves[r] != 0) {
            moves[w] = moves[r];
            pp[w] = pp[r];
            w++;
        }
    }
    for (int i = w; i < 4; i++) { moves[i] = 0; pp[i] = 0; }
    for (int i = 0; i < 4; i++) {
        wr16(data, moveOfs + i * 2, moves[i]);
        data[moveOfs + 8 + i] = pp[i];
    }
}

// PLA PP table (from PKHeX MoveInfo8a.cs) — correct PP for PLA context
static const uint8_t PLA_MOVE_PP[] = {
    0, 35, 25, 10, 15, 20, 20, 10, 10, 10, 35, 30,  5, 10, 20, 30, 25, 35, 20, 15,
   20, 20, 25, 20, 30,  5, 10, 15, 15, 15, 25, 20,  5, 30, 15, 20, 20, 10,  5, 30,
   20, 20, 20, 30, 20, 40, 20, 15, 20, 20, 20, 30, 25, 10, 30, 25,  5, 15, 10,  5,
   20, 20, 20,  5, 35, 20, 20, 20, 20, 20, 15, 20, 15, 10, 20, 25, 10, 20, 20, 20,
   10, 40, 10, 15, 25, 10, 20,  5, 15, 10,  5, 10, 10, 20, 10, 20, 40, 30, 20, 20,
   20, 15, 10, 40, 15, 10, 30, 10, 20, 10, 40, 40, 20, 30, 30, 20, 20, 10, 10, 20,
    5, 10, 30, 20, 20, 20,  5, 15, 15, 20, 10, 15, 35, 20, 15, 10, 10, 30, 15, 20,
   20, 10, 10,  5, 10, 25, 10, 10, 20, 15, 40, 20, 10,  5, 15, 10, 10, 10, 15, 30,
   30, 10, 10, 15, 10,  1,  1, 10, 25, 10,  5, 15, 20, 15, 10, 15, 30,  5, 40, 15,
   10, 25, 10, 20, 10, 20, 10, 10, 10, 20, 15, 20,  5, 40,  5,  5, 20,  5, 10,  5,
   10, 10, 10, 10, 20, 20, 30, 15, 10, 20, 20, 25,  5, 15, 10,  5, 20, 15, 20, 25,
   20,  5, 30,  5,  5, 20, 40,  5, 20, 40, 20,  5, 35, 10,  5,  5,  5, 15,  5, 25,
    5,  5, 10, 20, 10,  5, 15, 10, 10, 20, 15, 10, 10, 10, 20, 10, 10, 10, 10, 15,
   15, 15, 10, 20, 20, 10, 20, 20, 20, 20, 20, 10, 10, 10, 20, 20,  5, 15, 10, 10,
   15, 10, 20,  5,  5, 10, 10, 20,  5, 10, 20, 10, 20, 20, 20,  5,  5, 15, 20, 10,
   15, 20, 15, 10, 10, 15, 10,  5,  5, 10, 25, 10,  5, 20, 15,  5, 40, 15, 15, 40,
   15, 20, 20,  5, 15, 20, 15, 15, 15,  5, 10, 30, 20, 30, 20,  5, 40, 10,  5, 10,
    5, 15, 25, 25,  5, 20, 15, 10, 10, 20, 10, 20, 20,  5,  5, 10,  5, 40, 10, 10,
    5, 10, 10, 15, 10, 20, 15, 30, 10, 20,  5, 10, 10, 15, 10, 10,  5, 15,  5, 10,
   10, 30, 20, 20, 10, 10,  5,  5, 10,  5, 20, 10, 20, 10,  5, 10, 10, 20, 10, 10,
};

// Get the correct PLA PP for a move
static uint8_t getMovePP_PLA(uint16_t moveId) {
    if (moveId == 0) return 0;
    if (moveId < static_cast<uint16_t>(sizeof(PLA_MOVE_PP)))
        return PLA_MOVE_PP[moveId];
    return 0;
}

// Sanitize moves for PLA: remove dummied moves, fix PP to PLA values
static void sanitizeMovesForPLA(uint8_t* data, int moveOfs) {
    for (int i = 0; i < 4; i++) {
        uint16_t move = rd16(data, moveOfs + i * 2);
        if (move == 0) {
            data[moveOfs + 8 + i] = 0;
            continue;
        }
        if (move > 826 || isMoveDummiedInPLA(move)) {
            wr16(data, moveOfs + i * 2, 0);
            data[moveOfs + 8 + i] = 0;
        } else {
            // Fix PP to PLA values
            data[moveOfs + 8 + i] = getMovePP_PLA(move);
        }
    }
    // Compact moves: shift non-zero moves to front
    uint16_t moves[4];
    uint8_t pp[4];
    for (int i = 0; i < 4; i++) {
        moves[i] = rd16(data, moveOfs + i * 2);
        pp[i] = data[moveOfs + 8 + i];
    }
    int w = 0;
    for (int r = 0; r < 4; r++) {
        if (moves[r] != 0) {
            moves[w] = moves[r];
            pp[w] = pp[r];
            w++;
        }
    }
    for (int i = w; i < 4; i++) {
        moves[i] = 0;
        pp[i] = 0;
    }
    for (int i = 0; i < 4; i++) {
        wr16(data, moveOfs + i * 2, moves[i]);
        data[moveOfs + 8 + i] = pp[i];
    }
}

// Sanitize moves: clear any move > maxMoveID
static void sanitizeMoves(uint8_t* data, int moveOfs, uint16_t maxMoveID) {
    for (int i = 0; i < 4; i++) {
        uint16_t move = rd16(data, moveOfs + i * 2);
        if (move > maxMoveID) {
            wr16(data, moveOfs + i * 2, 0);
            // Clear PP too (PP is at moveOfs + 8 + i for Gen8+/PA9)
            data[moveOfs + 8 + i] = 0;
        }
    }
}

// Sanitize held item
static void sanitizeItem(uint8_t* data, int itemOfs, uint16_t maxItemID) {
    uint16_t item = rd16(data, itemOfs);
    if (item > maxItemID)
        wr16(data, itemOfs, 0);
}

// --- Phase 1: PK8/PB8 <-> PK9/PA9 conversions ---
// All are 0x158 party / 0x148 stored with 0x50-byte blocks.
// Block A and B are mostly shared, but Block C fields diverge after 0xCD.
// Block D (0xF8-0x147) and party stats (0x148+) are identical.
//
// G8PKM (PK8/PB8) Block C:  Version 0xDE, Language 0xE2, FormArg 0xE4, Affixed 0xE8
// PK9/PA9 Block C:           Version 0xCE, Language 0xD5, FormArg 0xD0, Affixed 0xD4
// Gender encoding: G8PKM bits 2-3, PK9/PA9 bits 1-2 at byte 0x22
// HeightScalar/WeightScalar: G8PKM 0x50/0x51, PK9/PA9 0x48/0x49

// Helper: remap Block C fields between Gen8 (G8PKM) and Gen9 (PK9/PA9) layouts.
// Both share HT fields at 0xA8-0xCD. After that the layouts diverge.
// Gen8 has PokeJob/Fullness/Enjoyment before Version; Gen9 puts Version right after HT Memory.
static void remapBlockC_Gen8toGen9(uint8_t* d, const uint8_t* s) {
    // Shared HT fields 0xA8-0xCD already correct from memcpy
    // Clear the divergent region 0xCE-0xF7 in dest (Gen9 layout)
    std::memset(d + 0xCE, 0, 0xF8 - 0xCE);

    // Gen8 0xDE -> Gen9 0xCE: Version
    d[0xCE] = s[0xDE];
    // Gen8 0xDF -> Gen9 0xCF: BattleVersion
    d[0xCF] = s[0xDF];
    // Gen8 0xE4 -> Gen9 0xD0: FormArgument (4 bytes)
    std::memcpy(d + 0xD0, s + 0xE4, 4);
    // Gen8 0xE8 -> Gen9 0xD4: AffixedRibbon
    d[0xD4] = s[0xE8];
    // Gen8 0xE2 -> Gen9 0xD5: Language
    d[0xD5] = s[0xE2];
}

static void remapBlockC_Gen9toGen8(uint8_t* d, const uint8_t* s) {
    // Shared HT fields 0xA8-0xCD already correct from memcpy
    // Clear the divergent region 0xCE-0xF7 in dest (Gen8 layout)
    std::memset(d + 0xCE, 0, 0xF8 - 0xCE);

    // Gen9 0xCE -> Gen8 0xDE: Version
    d[0xDE] = s[0xCE];
    // Gen9 0xCF -> Gen8 0xDF: BattleVersion
    d[0xDF] = s[0xCF];
    // Gen9 0xD0 -> Gen8 0xE4: FormArgument (4 bytes)
    std::memcpy(d + 0xE4, s + 0xD0, 4);
    // Gen9 0xD4 -> Gen8 0xE8: AffixedRibbon
    d[0xE8] = s[0xD4];
    // Gen9 0xD5 -> Gen8 0xE2: Language
    d[0xE2] = s[0xD5];
}

static Pokemon convertSameSize(const Pokemon& src, GameType srcGame, GameType destGame) {
    Pokemon dst;
    // Start with full copy — same total size (0x158)
    std::memcpy(dst.data.data(), src.data.data(), PokeCrypto::SIZE_9PARTY);
    dst.gameType_ = destGame;

    uint8_t* d = dst.data.data();
    const uint8_t* s = src.data.data();

    bool srcIsGen8 = isSwSh(srcGame) || isBDSP(srcGame);
    bool dstIsGen8 = isSwSh(destGame) || isBDSP(destGame);

    // --- Species ID conversion ---
    // Gen8 (PK8/PB8) stores national dex ID, Gen9 (PK9/PA9) stores Gen9 internal ID
    if (srcIsGen8 && !dstIsGen8) {
        uint16_t nationalId = rd16(s, 0x08);
        wr16(d, 0x08, SpeciesConverter::getInternal9(nationalId));
    } else if (!srcIsGen8 && dstIsGen8) {
        uint16_t internal9 = rd16(s, 0x08);
        wr16(d, 0x08, SpeciesConverter::getNational9(internal9));
    }

    // --- Block A fixups ---

    // Gender encoding: G8PKM bits 2-3, PK9/PA9 bits 1-2 at byte 0x22
    if (srcIsGen8 && !dstIsGen8) {
        uint8_t flags = s[0x22];
        bool fateful = flags & 1;
        uint8_t gender = (flags >> 2) & 3;
        d[0x22] = (fateful ? 1 : 0) | (gender << 1);
    } else if (!srcIsGen8 && dstIsGen8) {
        uint8_t flags = s[0x22];
        bool fateful = flags & 1;
        uint8_t gender = (flags >> 1) & 3;
        d[0x22] = (fateful ? 1 : 0) | (gender << 2);
    }

    // IsAlpha at 0x23: only PA9/ZA has it
    if (destGame == GameType::ZA) {
        if (srcIsGen8) d[0x23] = 0;
    } else {
        d[0x23] = 0; // Clear for non-ZA destinations
    }

    // HeightScalar/WeightScalar: G8PKM 0x50/0x51, PK9/PA9 0x48/0x49
    // Scale: only PK9/PA9 at 0x4A
    if (srcIsGen8 && !dstIsGen8) {
        d[0x48] = s[0x50];
        d[0x49] = s[0x51];
        d[0x4A] = 0; // Scale — Gen8 doesn't have it
        d[0x50] = 0;
        d[0x51] = 0;
    } else if (!srcIsGen8 && dstIsGen8) {
        d[0x50] = s[0x48];
        d[0x51] = s[0x49];
        d[0x48] = 0;
        d[0x49] = 0;
        d[0x4A] = 0;
    }

    // CanGigantamax at 0x16 bit 4 — only SwSh
    if (!isSwSh(destGame))
        d[0x16] &= ~(1 << 4);

    // --- Block B fixups ---

    // DynamaxLevel: G8PKM 0x90, unused in PK9/PA9
    if (!dstIsGen8)
        d[0x90] = 0;

    // TeraType: PK9/PA9 0x94-0x95, G8PKM has Status_Condition at 0x94
    // Clear destination-specific fields
    if (dstIsGen8) {
        // Going to Gen8: 0x94 is Status_Condition — clear (transient)
        wr32(d, 0x94, 0);
    } else {
        // Going to Gen9: 0x94-0x95 is TeraType — clear (will be default)
        d[0x94] = 0;
        d[0x95] = 0;
    }

    // --- Block C fixups (layouts diverge after 0xCD) ---
    if (srcIsGen8 && !dstIsGen8)
        remapBlockC_Gen8toGen9(d, s);
    else if (!srcIsGen8 && dstIsGen8)
        remapBlockC_Gen9toGen8(d, s);

    // --- Sanitize moves and items ---
    uint16_t maxMove = 919;
    uint16_t maxItem = 2557;
    if (isSwSh(destGame)) { maxMove = 826; maxItem = 1607; }
    else if (isBDSP(destGame)) { maxMove = 826; maxItem = 1822; }

    sanitizeMoves(d, 0x72, maxMove);
    filterDummiedMoves(d, 0x72, destGame);
    sanitizeItem(d, 0x0A, maxItem);

    dst.refreshChecksum();
    return dst;
}

// --- Phase 2: PA8 <-> PA9 (different block sizes) ---
// PA8: 0x178 party, 0x58 blocks
// PA9: 0x158 party, 0x50 blocks
// Field offsets differ significantly.

// PA8 -> PA9: field-by-field remap (verified against PKHeX PA8.cs / PK9.cs)
// PA8: 0x58-byte blocks, PA9: 0x50-byte blocks
static Pokemon convertPA8toPA9(const Pokemon& src) {
    Pokemon dst;
    dst.gameType_ = GameType::ZA;
    uint8_t* d = dst.data.data();
    const uint8_t* s = src.data.data();

    // --- Block A: 0x00-0x4F (PA9) / 0x00-0x5F (PA8) ---
    // Header + core fields share offsets 0x00-0x21
    std::memcpy(d, s, 0x22); // EC, sanity, checksum, species, item, TID/SID, EXP, ability, flags, PID, nature, statnature

    // Species: PA8 stores national dex ID, PA9 stores Gen9 internal ID
    {
        uint16_t nationalId = rd16(s, 0x08); // PA8 = national
        wr16(d, 0x08, SpeciesConverter::getInternal9(nationalId));
    }

    // Gender encoding differs: PA8 bits 2-3, PA9 bits 1-2 at byte 0x22
    {
        uint8_t pa8flags = s[0x22];
        bool fateful = pa8flags & 1;
        uint8_t gender = (pa8flags >> 2) & 3;  // PA8: bits 2-3
        d[0x22] = (fateful ? 1 : 0) | (gender << 1);  // PA9: bit 0 fateful, bits 1-2 gender
    }

    // IsAlpha: PA8 0x16 bit 5 -> PA9 0x23
    bool isAlpha = (s[0x16] >> 5) & 1;
    d[0x16] &= ~((1 << 5) | (1 << 6)); // Clear IsAlpha + IsNoble bits
    d[0x23] = isAlpha ? 1 : 0;

    // Form: same offset 0x24 (u16)
    std::memcpy(d + 0x24, s + 0x24, 2);

    // EVs: 0x26-0x2B (same in both)
    std::memcpy(d + 0x26, s + 0x26, 6);

    // Contest stats: 0x2C-0x31 (same)
    std::memcpy(d + 0x2C, s + 0x2C, 6);

    // Pokerus: 0x32 (same)
    d[0x32] = s[0x32];

    // Ribbon flags: 0x34-0x47 (same range, PA8 has AlphaMove at 0x3E)
    std::memcpy(d + 0x34, s + 0x34, 20);

    // HeightScalar/WeightScalar/Scale: PA8 0x50/0x51/0x52 -> PA9 0x48/0x49/0x4A
    d[0x48] = s[0x50];
    d[0x49] = s[0x51];
    d[0x4A] = s[0x52];

    // Nickname: PA8 0x60 (26 bytes) -> PA9 0x58
    std::memcpy(d + 0x58, s + 0x60, 26);

    // --- Block B ---
    // Moves: PA8 0x54-0x5B -> PA9 0x72-0x79
    std::memcpy(d + 0x72, s + 0x54, 8);

    // Move PP: PA8 0x5C-0x5F -> PA9 0x7A-0x7D
    std::memcpy(d + 0x7A, s + 0x5C, 4);

    // Move PP Ups: PA8 0x86-0x89 -> PA9 0x7E-0x81
    std::memcpy(d + 0x7E, s + 0x86, 4);

    // Relearn moves: PA8 0x8A-0x91 -> PA9 0x82-0x89
    std::memcpy(d + 0x82, s + 0x8A, 8);

    // HP Current: PA8 0x92 -> PA9 0x8A
    std::memcpy(d + 0x8A, s + 0x92, 2);

    // IV32: PA8 0x94 -> PA9 0x8C
    std::memcpy(d + 0x8C, s + 0x94, 4);

    // PA9 0x90-0x93 is unused (no DynamaxLevel or Status in PA9 box data)

    // --- Block C: Handler info ---
    // HT Name: PA8 0xB8 (26 bytes) -> PA9 0xA8
    std::memcpy(d + 0xA8, s + 0xB8, 26);

    // HT Gender: PA8 0xD2 -> PA9 0xC2
    d[0xC2] = s[0xD2];
    // HT Language: PA8 0xD3 -> PA9 0xC3
    d[0xC3] = s[0xD3];
    // CurrentHandler: PA8 0xD4 -> PA9 0xC4
    d[0xC4] = s[0xD4];
    // HT ID: PA8 0xD6 -> PA9 0xC6
    std::memcpy(d + 0xC6, s + 0xD6, 2);
    // HT Friendship: PA8 0xD8 -> PA9 0xC8
    d[0xC8] = s[0xD8];
    // HT Memory (Intensity, Memory, Feeling, Variable): PA8 0xD9-0xDD -> PA9 0xC9-0xCD
    std::memcpy(d + 0xC9, s + 0xD9, 5);

    // Version: PA8 0xEE -> PA9 0xCE
    d[0xCE] = s[0xEE];
    // BattleVersion: PA8 0xEF -> PA9 0xCF
    d[0xCF] = s[0xEF];

    // FormArgument: PA8 0xF4 -> PA9 0xD0
    std::memcpy(d + 0xD0, s + 0xF4, 4);

    // AffixedRibbon: PA8 0xF8 -> PA9 0xD4
    d[0xD4] = s[0xF8];

    // Language: PA8 0xF2 -> PA9 0xD5
    d[0xD5] = s[0xF2];

    // --- Block D: OT info ---
    // OT Name: PA8 0x110 (26 bytes) -> PA9 0xF8
    std::memcpy(d + 0xF8, s + 0x110, 26);

    // OT Friendship: PA8 0x12A -> PA9 0x112
    d[0x112] = s[0x12A];

    // OT Memory: PA8 layout [Intensity][Memory][gap][TextVar(2)][Feeling]
    //            PA9 layout [Intensity][Memory][Feeling][TextVar(2)]
    d[0x113] = s[0x12B]; // OT Intensity
    d[0x114] = s[0x12C]; // OT Memory
    d[0x115] = s[0x130]; // OT Feeling
    std::memcpy(d + 0x116, s + 0x12E, 2); // OT TextVar

    // Egg date: PA8 0x131-0x133 -> PA9 0x119-0x11B
    std::memcpy(d + 0x119, s + 0x131, 3);

    // Met date: PA8 0x134-0x136 -> PA9 0x11C-0x11E
    std::memcpy(d + 0x11C, s + 0x134, 3);

    // Egg location: PA8 0x138 -> PA9 0x120
    std::memcpy(d + 0x120, s + 0x138, 2);

    // Met location: PA8 0x13A -> PA9 0x122
    std::memcpy(d + 0x122, s + 0x13A, 2);

    // Ball: PA8 0x137 -> PA9 0x124
    d[0x124] = s[0x137];

    // MetLevel + OTGender: PA8 0x13D -> PA9 0x125
    d[0x125] = s[0x13D];

    // HyperTrainFlags: PA8 0x13E -> PA9 0x126
    d[0x126] = s[0x13E];

    // --- Party stats ---
    // Level: PA8 0x168 -> PA9 0x148
    d[0x148] = s[0x168];

    // Sanitize moves for Gen9
    sanitizeMoves(d, 0x72, 919);
    sanitizeItem(d, 0x0A, 2557);

    dst.refreshChecksum();
    return dst;
}

// PA9 -> PA8: reverse remap (verified against PKHeX PA8.cs / PK9.cs)
static Pokemon convertPA9toPA8(const Pokemon& src) {
    Pokemon dst;
    dst.gameType_ = GameType::LA;
    uint8_t* d = dst.data.data();
    const uint8_t* s = src.data.data();

    // Zero the full PA8 buffer
    std::memset(d, 0, PokeCrypto::SIZE_8APARTY);

    // --- Block A ---
    // Header + core 0x00-0x21
    std::memcpy(d, s, 0x22);

    // Species: PA9 stores Gen9 internal ID, PA8 stores national dex ID
    {
        uint16_t internal9 = rd16(s, 0x08);
        wr16(d, 0x08, SpeciesConverter::getNational9(internal9));
    }

    // Gender encoding: PA9 bits 1-2 -> PA8 bits 2-3 at byte 0x22
    {
        uint8_t pa9flags = s[0x22];
        bool fateful = pa9flags & 1;
        uint8_t gender = (pa9flags >> 1) & 3;  // PA9: bits 1-2
        d[0x22] = (fateful ? 1 : 0) | (gender << 2);  // PA8: bit 0 fateful, bits 2-3 gender
    }

    // IsAlpha: PA9 0x23 -> PA8 0x16 bit 5
    bool isAlpha = s[0x23] != 0;
    d[0x23] = 0; // Clear (unused in PA8)
    if (isAlpha) d[0x16] |= (1 << 5);

    // Form: 0x24 (u16, same)
    std::memcpy(d + 0x24, s + 0x24, 2);

    // EVs, Contest stats, Pokerus (same offsets)
    std::memcpy(d + 0x26, s + 0x26, 6);
    std::memcpy(d + 0x2C, s + 0x2C, 6);
    d[0x32] = s[0x32];

    // Ribbons: 0x34-0x47 (same)
    std::memcpy(d + 0x34, s + 0x34, 20);

    // HeightScalar/WeightScalar/Scale: PA9 0x48/0x49/0x4A -> PA8 0x50/0x51/0x52
    d[0x50] = s[0x48];
    d[0x51] = s[0x49];
    d[0x52] = s[0x4A];

    // Moves: PA9 0x72 -> PA8 0x54
    std::memcpy(d + 0x54, s + 0x72, 8);
    // Move PP: PA9 0x7A -> PA8 0x5C
    std::memcpy(d + 0x5C, s + 0x7A, 4);

    // Nickname: PA9 0x58 -> PA8 0x60
    std::memcpy(d + 0x60, s + 0x58, 26);

    // --- Block B ---
    // PP Ups: PA9 0x7E -> PA8 0x86
    std::memcpy(d + 0x86, s + 0x7E, 4);

    // Relearn: PA9 0x82 -> PA8 0x8A
    std::memcpy(d + 0x8A, s + 0x82, 8);

    // HP Current: PA9 0x8A -> PA8 0x92
    std::memcpy(d + 0x92, s + 0x8A, 2);

    // IV32: PA9 0x8C -> PA8 0x94
    std::memcpy(d + 0x94, s + 0x8C, 4);

    // DynamaxLevel: PA8 0x98 — set to 0 (PA9 doesn't have it)
    d[0x98] = 0;

    // --- Block C ---
    // HT Name: PA9 0xA8 -> PA8 0xB8
    std::memcpy(d + 0xB8, s + 0xA8, 26);
    d[0xD2] = s[0xC2]; // HT Gender
    d[0xD3] = s[0xC3]; // HT Language
    d[0xD4] = s[0xC4]; // CurrentHandler
    std::memcpy(d + 0xD6, s + 0xC6, 2); // HT ID
    d[0xD8] = s[0xC8]; // HT Friendship
    std::memcpy(d + 0xD9, s + 0xC9, 5); // HT Memory

    // Version: PA9 0xCE -> PA8 0xEE
    d[0xEE] = s[0xCE];
    // BattleVersion: PA9 0xCF -> PA8 0xEF
    d[0xEF] = s[0xCF];
    // Language: PA9 0xD5 -> PA8 0xF2
    d[0xF2] = s[0xD5];

    // FormArgument: PA9 0xD0 -> PA8 0xF4
    std::memcpy(d + 0xF4, s + 0xD0, 4);

    // AffixedRibbon: PA9 0xD4 -> PA8 0xF8
    d[0xF8] = s[0xD4];

    // --- Block D ---
    // OT Name: PA9 0xF8 -> PA8 0x110
    std::memcpy(d + 0x110, s + 0xF8, 26);
    d[0x12A] = s[0x112]; // OT Friendship
    // OT Memory: PA9 layout [Intensity][Memory][Feeling][TextVar(2)]
    //            PA8 layout [Intensity][Memory][gap][TextVar(2)][Feeling]
    d[0x12B] = s[0x113]; // OT Intensity
    d[0x12C] = s[0x114]; // OT Memory
    // 0x12D is unused alignment in PA8
    std::memcpy(d + 0x12E, s + 0x116, 2); // OT TextVar
    d[0x130] = s[0x115]; // OT Feeling
    std::memcpy(d + 0x131, s + 0x119, 3); // Egg date
    std::memcpy(d + 0x134, s + 0x11C, 3); // Met date
    std::memcpy(d + 0x138, s + 0x120, 2); // Egg location
    std::memcpy(d + 0x13A, s + 0x122, 2); // Met location
    d[0x137] = s[0x124]; // Ball
    d[0x13D] = s[0x125]; // MetLevel+OTGender
    d[0x13E] = s[0x126]; // HyperTrain

    // --- Party ---
    // Level: PA9 0x148 -> PA8 0x168
    d[0x168] = s[0x148];

    // Sanitize for LA: remove dummied moves, fix PP to PLA values
    sanitizeMovesForPLA(d, 0x54);
    sanitizeItem(d, 0x0A, 1828);

    dst.refreshChecksum();
    return dst;
}

// --- Phase 3: PB7 <-> PA9 ---

static Pokemon convertPB7toPA9(const Pokemon& src) {
    Pokemon dst;
    dst.gameType_ = GameType::ZA;
    uint8_t* d = dst.data.data();
    const uint8_t* s = src.data.data();

    // EC: 0x00 -> 0x00
    std::memcpy(d, s, 4);

    // Sanity + Checksum: 0x04 -> 0x04
    std::memcpy(d + 0x04, s + 0x04, 4);

    // Species: PB7 stores national dex ID, PA9 stores Gen9 internal ID
    {
        uint16_t nationalId = rd16(s, 0x08);
        wr16(d, 0x08, SpeciesConverter::getInternal9(nationalId));
    }

    // Held item: 0x0A -> 0x0A
    std::memcpy(d + 0x0A, s + 0x0A, 2);

    // TID/SID: 0x0C -> 0x0C
    std::memcpy(d + 0x0C, s + 0x0C, 4);

    // EXP: 0x10 -> 0x10
    std::memcpy(d + 0x10, s + 0x10, 4);

    // Ability: PB7 0x14 (u8) -> PA9 0x14 (u16)
    wr16(d, 0x14, (uint16_t)s[0x14]);

    // AbilityNumber: PB7 0x15, PA9 0x16
    d[0x16] = s[0x15];

    // Marking: PB7 0x16 (u16) -> PA9 0x18 (u16)
    std::memcpy(d + 0x18, s + 0x16, 2);

    // PID: PB7 0x18 -> PA9 0x1C
    std::memcpy(d + 0x1C, s + 0x18, 4);

    // Nature: PB7 0x1C -> PA9 0x20
    d[0x20] = s[0x1C];
    d[0x21] = s[0x1C]; // StatNature = Nature for PB7

    // FatefulEncounter + Gender + Form: PB7 0x1D packed -> PA9 separate fields
    uint8_t flagByte = s[0x1D];
    d[0x22] = flagByte & 0x07; // Fateful (bit 0) + Gender (bits 1-2)
    d[0x24] = (flagByte >> 3) & 0x1F; // Form (bits 3-7)

    // EVs: PB7 0x1E-0x23 -> PA9 0x26-0x2B
    std::memcpy(d + 0x26, s + 0x1E, 6);

    // Pokerus: PB7 0x2B -> PA9 0x32
    d[0x32] = s[0x2B];

    // HeightScalar/WeightScalar: PB7 0x3A/0x3B -> PA9 0x48/0x49
    d[0x48] = s[0x3A];
    d[0x49] = s[0x3B];

    // FormArgument: PB7 0x3C -> PA9 0xD0
    std::memcpy(d + 0xD0, s + 0x3C, 4);

    // Nickname: PB7 0x40 (24 bytes) -> PA9 0x58 (26 bytes)
    std::memcpy(d + 0x58, s + 0x40, 24);

    // Moves: PB7 0x5A-0x60 -> PA9 0x72-0x78
    std::memcpy(d + 0x72, s + 0x5A, 8);

    // Move PP: PB7 0x62-0x65 -> PA9 0x7A-0x7D
    std::memcpy(d + 0x7A, s + 0x62, 4);

    // Move PP Ups: PB7 0x66-0x69 -> PA9 0x7E-0x81
    std::memcpy(d + 0x7E, s + 0x66, 4);

    // Relearn moves: PB7 0x6A-0x70 -> PA9 0x82-0x88
    std::memcpy(d + 0x82, s + 0x6A, 8);

    // IV32: PB7 0x74 -> PA9 0x8C
    std::memcpy(d + 0x8C, s + 0x74, 4);

    // HT Name: PB7 0x78 (26 bytes) -> PA9 0xA8
    std::memcpy(d + 0xA8, s + 0x78, 26);
    // HT Gender: PB7 0x92 -> PA9 0xC2
    d[0xC2] = s[0x92];
    // CurrentHandler: PB7 0x93 -> PA9 0xC4
    d[0xC4] = s[0x93];

    // HT Friendship: PB7 0xA2 -> PA9 0xC8
    d[0xC8] = s[0xA2];

    // HT Memory: PB7 Intensity 0xA4, Memory 0xA5, Feeling 0xA6, TextVar 0xA8
    //         -> PA9 0xC9, 0xCA, 0xCB, 0xCC
    d[0xC9] = s[0xA4]; // Intensity
    d[0xCA] = s[0xA5]; // Memory
    d[0xCB] = s[0xA6]; // Feeling
    std::memcpy(d + 0xCC, s + 0xA8, 2); // TextVar (u16)

    // OT Name: PB7 0xB0 (24 bytes) -> PA9 0xF8 (26 bytes)
    std::memcpy(d + 0xF8, s + 0xB0, 24);

    // OT Friendship: PB7 0xCA -> PA9 0x112
    d[0x112] = s[0xCA];

    // Ball: PB7 0xDC -> PA9 0x124
    d[0x124] = s[0xDC];

    // Met level + OT Gender: PB7 0xDD -> PA9 0x125
    d[0x125] = s[0xDD];

    // Egg location: PB7 0xD8 (u16) -> PA9 0x120
    std::memcpy(d + 0x120, s + 0xD8, 2);

    // Met location: PB7 0xDA (u16) -> PA9 0x122
    std::memcpy(d + 0x122, s + 0xDA, 2);

    // Egg date: PB7 0xD1-0xD3 -> PA9 0x119-0x11B
    std::memcpy(d + 0x119, s + 0xD1, 3);
    // Met date: PB7 0xD4-0xD6 -> PA9 0x11C-0x11E
    std::memcpy(d + 0x11C, s + 0xD4, 3);

    // HyperTrainFlags: PB7 0xDE -> PA9 0x126
    d[0x126] = s[0xDE];

    // Version: PB7 0xDF -> PA9 0xCE
    d[0xCE] = s[0xDF];

    // Language: PB7 0xE3 -> PA9 0xD5
    d[0xD5] = s[0xE3];

    // Level: set from PB7 0xEC if available
    d[0x148] = s[0xEC];

    sanitizeMoves(d, 0x72, 919);
    sanitizeItem(d, 0x0A, 2557);

    dst.refreshChecksum();
    return dst;
}

static Pokemon convertPA9toPB7(const Pokemon& src) {
    Pokemon dst;
    dst.gameType_ = GameType::GP;
    uint8_t* d = dst.data.data();
    const uint8_t* s = src.data.data();
    std::memset(d, 0, PokeCrypto::SIZE_6PARTY);

    // EC
    std::memcpy(d, s, 4);
    // Sanity + Checksum
    std::memcpy(d + 0x04, s + 0x04, 4);
    // Species: PA9 stores Gen9 internal ID, PB7 stores national dex ID
    {
        uint16_t internal9 = rd16(s, 0x08);
        wr16(d, 0x08, SpeciesConverter::getNational9(internal9));
    }
    // Held item
    std::memcpy(d + 0x0A, s + 0x0A, 2);
    // TID/SID
    std::memcpy(d + 0x0C, s + 0x0C, 4);
    // EXP
    std::memcpy(d + 0x10, s + 0x10, 4);
    // Ability
    d[0x14] = (uint8_t)rd16(s, 0x14);
    // AbilityNumber
    d[0x15] = s[0x16];
    // Marking
    std::memcpy(d + 0x16, s + 0x18, 2);
    // PID
    std::memcpy(d + 0x18, s + 0x1C, 4);
    // Nature
    d[0x1C] = s[0x20];
    // Fateful + Gender + Form packed
    d[0x1D] = (s[0x22] & 0x07) | ((s[0x24] & 0x1F) << 3);
    // EVs
    std::memcpy(d + 0x1E, s + 0x26, 6);
    // Pokerus
    d[0x2B] = s[0x32];
    // Scale
    d[0x3A] = s[0x48];
    d[0x3B] = s[0x49];
    // FormArgument
    std::memcpy(d + 0x3C, s + 0xD0, 4);
    // Nickname
    std::memcpy(d + 0x40, s + 0x58, 24);
    // Moves
    std::memcpy(d + 0x5A, s + 0x72, 8);
    std::memcpy(d + 0x62, s + 0x7A, 4);
    std::memcpy(d + 0x66, s + 0x7E, 4);
    std::memcpy(d + 0x6A, s + 0x82, 8);
    // IV32
    std::memcpy(d + 0x74, s + 0x8C, 4);
    // HT Name: PA9 0xA8 (26 bytes) -> PB7 0x78
    std::memcpy(d + 0x78, s + 0xA8, 26);
    d[0x92] = s[0xC2]; // HT Gender
    d[0x93] = s[0xC4]; // CurrentHandler
    d[0xA2] = s[0xC8]; // HT Friendship
    d[0xA4] = s[0xC9]; // HT Memory Intensity
    d[0xA5] = s[0xCA]; // HT Memory
    d[0xA6] = s[0xCB]; // HT Memory Feeling
    std::memcpy(d + 0xA8, s + 0xCC, 2); // HT Memory TextVar
    // OT Name
    std::memcpy(d + 0xB0, s + 0xF8, 24);
    // OT Friendship
    d[0xCA] = s[0x112];
    // Ball
    d[0xDC] = s[0x124];
    // Met level + OT Gender
    d[0xDD] = s[0x125];
    // HyperTrainFlags: PA9 0x126 -> PB7 0xDE
    d[0xDE] = s[0x126];
    // Egg location: PA9 0x120 -> PB7 0xD8
    std::memcpy(d + 0xD8, s + 0x120, 2);
    // Met location: PA9 0x122 -> PB7 0xDA
    std::memcpy(d + 0xDA, s + 0x122, 2);
    // Egg date: PA9 0x119-0x11B -> PB7 0xD1-0xD3
    std::memcpy(d + 0xD1, s + 0x119, 3);
    // Met date: PA9 0x11C-0x11E -> PB7 0xD4-0xD6
    std::memcpy(d + 0xD4, s + 0x11C, 3);
    // Version: PA9 0xCE -> PB7 0xDF
    d[0xDF] = s[0xCE];
    // Language: PA9 0xD5 -> PB7 0xE3
    d[0xE3] = s[0xD5];
    // Level
    d[0xEC] = s[0x148];

    // Sanitize for LGPE (move max ~742, item max ~959)
    // PB7 moves are at 0x5A, PP at 0x62
    for (int i = 0; i < 4; i++) {
        uint16_t move = rd16(d, 0x5A + i * 2);
        if (move > 742) {
            wr16(d, 0x5A + i * 2, 0);
            d[0x62 + i] = 0; // PP
        }
    }
    filterDummiedMoves(d, 0x5A, GameType::GP);
    sanitizeItem(d, 0x0A, 959);

    dst.refreshChecksum();
    return dst;
}

// --- Phase 3: PK3 <-> PA9 ---

static Pokemon convertPK3toPA9(const Pokemon& src) {
    Pokemon dst;
    dst.gameType_ = GameType::ZA;
    uint8_t* d = dst.data.data();
    const uint8_t* s = src.data.data();

    // PK3 is stored decrypted in bank
    uint32_t pid = rd32(s, 0x00);

    // EC = PID for Gen3 origin
    wr32(d, 0x00, pid);

    // Species: Gen3 internal at 0x20 -> convert to national -> write as Gen9 internal at 0x08
    uint16_t speciesInternal3 = rd16(s, 0x20);
    uint16_t speciesNational = SpeciesConverter::getNational3(speciesInternal3);
    uint16_t speciesInternal9 = SpeciesConverter::getInternal9(speciesNational);
    wr16(d, 0x08, speciesInternal9);

    // Held item: 0x22 -> 0x0A (items may not map 1:1, clear if > Gen9 max)
    wr16(d, 0x0A, rd16(s, 0x22));

    // TID/SID: PK3 0x04/0x06 -> PA9 0x0C/0x0E
    wr16(d, 0x0C, rd16(s, 0x04));
    wr16(d, 0x0E, rd16(s, 0x06));

    // EXP: PK3 0x24 -> PA9 0x10
    wr32(d, 0x10, rd32(s, 0x24));

    // PID: PA9 0x1C
    wr32(d, 0x1C, pid);

    // Nature: PID % 25 -> PA9 0x20, 0x21
    uint8_t nature = pid % 25;
    d[0x20] = nature;
    d[0x21] = nature;

    // Gender: derived from PID vs species gender ratio
    // For simplicity, set to 0 (male) — proper derivation would need PersonalInfo
    // The form is 0 for all Gen3
    d[0x22] = 0; // Will be refined below
    d[0x24] = 0; // Form = 0

    // EVs: PK3 0x38-0x3D -> PA9 0x26-0x2B
    std::memcpy(d + 0x26, s + 0x38, 6);

    // Contest stats: PK3 0x3E-0x43 -> PA9 0x2C-0x31
    std::memcpy(d + 0x2C, s + 0x3E, 6);

    // Pokerus: PK3 0x44 -> PA9 0x32
    d[0x32] = s[0x44];

    // Moves: PK3 0x2C-0x33 -> PA9 0x72-0x79
    std::memcpy(d + 0x72, s + 0x2C, 8);

    // Move PP: PK3 0x34-0x37 -> PA9 0x7A-0x7D
    std::memcpy(d + 0x7A, s + 0x34, 4);

    // PP Ups: PK3 0x28 (packed 2-bit) -> PA9 0x7E-0x81
    uint8_t ppUps = s[0x28];
    d[0x7E] = ppUps & 3;
    d[0x7F] = (ppUps >> 2) & 3;
    d[0x80] = (ppUps >> 4) & 3;
    d[0x81] = (ppUps >> 6) & 3;

    // IV32: PK3 0x48 -> PA9 0x8C (same bit packing)
    wr32(d, 0x8C, rd32(s, 0x48));

    // Nickname: PK3 uses special Gen3 encoding at 0x08 (10 bytes)
    // For now, copy raw bytes into PA9 nickname field — ideally would
    // transcode Gen3 charset to UTF-16LE. The Pokemon::nickname() accessor
    // already handles Gen3 decoding, so we mark it as nicknamed and let
    // the display layer handle it. For universal bank storage this is fine.
    // TODO: proper Gen3 -> UTF-16LE transcoding
    // Leave nickname area zeroed for now (species name will be used)

    // OT Name: PK3 0x14 (7 bytes Gen3 encoded)
    // Same situation as nickname — leave zeroed for now
    // TODO: proper Gen3 -> UTF-16LE transcoding

    // OT Friendship: PK3 0x29 -> PA9 0x112
    d[0x112] = s[0x29];

    // Ball: PK3 bits 11-14 of u16@0x46 -> PA9 0x124
    d[0x124] = (rd16(s, 0x46) >> 11) & 0xF;

    // Met level: PK3 bits 0-6 of u16@0x46 -> PA9 0x125
    // OT Gender: PK3 bit 15 of u16@0x46
    uint16_t origins = rd16(s, 0x46);
    uint8_t metLevel = origins & 0x7F;
    uint8_t otGender = (origins >> 15) & 1;
    d[0x125] = metLevel | (otGender << 7);

    // Met location: PK3 0x45 (single byte) -> PA9 0x122
    wr16(d, 0x122, (uint16_t)s[0x45]);

    // Version: PK3 bits 7-10 of origins -> PA9 0xCE
    d[0xCE] = (origins >> 7) & 0xF;

    // Language: PK3 0x12 -> PA9 0xD5
    d[0xD5] = s[0x12];

    // Level: PK3 party 0x54 -> PA9 0x148
    d[0x148] = s[0x54];

    sanitizeMoves(d, 0x72, 919);
    sanitizeItem(d, 0x0A, 2557);

    dst.refreshChecksum();
    return dst;
}

static Pokemon convertPA9toPK3(const Pokemon& src) {
    Pokemon dst;
    dst.gameType_ = GameType::FR;
    uint8_t* d = dst.data.data();
    const uint8_t* s = src.data.data();
    std::memset(d, 0, PokeCrypto::SIZE_3PARTY);

    uint32_t pid = rd32(s, 0x1C);
    wr32(d, 0x00, pid);

    // TID/SID
    wr16(d, 0x04, rd16(s, 0x0C));
    wr16(d, 0x06, rd16(s, 0x0E));

    // Species: PA9 Gen9 internal -> national -> Gen3 internal
    uint16_t speciesInternal9 = rd16(s, 0x08);
    uint16_t speciesNational = SpeciesConverter::getNational9(speciesInternal9);
    uint16_t speciesInternal3 = SpeciesConverter::getInternal3(speciesNational);
    wr16(d, 0x20, speciesInternal3);

    // Held item
    wr16(d, 0x22, rd16(s, 0x0A));

    // EXP
    wr32(d, 0x24, rd32(s, 0x10));

    // PP Ups (packed)
    d[0x28] = (s[0x7E] & 3) | ((s[0x7F] & 3) << 2) | ((s[0x80] & 3) << 4) | ((s[0x81] & 3) << 6);

    // Friendship
    d[0x29] = s[0x112];

    // Moves
    std::memcpy(d + 0x2C, s + 0x72, 8);

    // Move PP
    std::memcpy(d + 0x34, s + 0x7A, 4);

    // EVs
    std::memcpy(d + 0x38, s + 0x26, 6);

    // Contest stats
    std::memcpy(d + 0x3E, s + 0x2C, 6);

    // Pokerus
    d[0x44] = s[0x32];

    // Met location
    d[0x45] = (uint8_t)rd16(s, 0x122);

    // Origins: MetLevel (0-6), Version (7-10), Ball (11-14), OTGender (15)
    uint8_t metLevel = s[0x125] & 0x7F;
    uint8_t otGender = (s[0x125] >> 7) & 1;
    uint8_t ball = s[0x124] & 0xF;
    uint8_t version = s[0xCE] & 0xF;
    uint16_t origins = metLevel | ((uint16_t)version << 7) | ((uint16_t)ball << 11) | ((uint16_t)otGender << 15);
    wr16(d, 0x46, origins);

    // IV32
    wr32(d, 0x48, rd32(s, 0x8C));

    // Language
    d[0x12] = s[0xD5];

    // HasSpecies flag at 0x13
    if (speciesNational != 0)
        d[0x13] |= (1 << 1);

    // Level (party)
    d[0x54] = s[0x148];

    // Sanitize moves for Gen3 (max move ~354)
    for (int i = 0; i < 4; i++) {
        uint16_t move = rd16(d, 0x2C + i * 2);
        if (move > 354) {
            wr16(d, 0x2C + i * 2, 0);
            d[0x34 + i] = 0;
        }
    }
    // Sanitize item for Gen3 (max ~376)
    sanitizeItem(d, 0x22, 376);

    dst.refreshChecksum();
    return dst;
}

// --- Canonical conversions (PA9 as hub) ---

Pokemon EntityConverter::toCanonical(const Pokemon& src, GameType srcGame) {
    if (srcGame == GameType::ZA)
        return src; // Already canonical

    if (isSV(srcGame) || isSwSh(srcGame) || isBDSP(srcGame))
        return convertSameSize(src, srcGame, GameType::ZA);

    if (srcGame == GameType::LA)
        return convertPA8toPA9(src);

    if (isLGPE(srcGame))
        return convertPB7toPA9(src);

    if (isFRLG(srcGame))
        return convertPK3toPA9(src);

    // Fallback: return as-is
    return src;
}

Pokemon EntityConverter::fromCanonical(const Pokemon& src, GameType destGame) {
    if (destGame == GameType::ZA)
        return src; // Already canonical

    if (isSV(destGame) || isSwSh(destGame) || isBDSP(destGame))
        return convertSameSize(src, GameType::ZA, destGame);

    if (destGame == GameType::LA)
        return convertPA9toPA8(src);

    if (isLGPE(destGame))
        return convertPA9toPB7(src);

    if (isFRLG(destGame))
        return convertPA9toPK3(src);

    return src;
}

// --- Full conversion ---

Pokemon EntityConverter::convert(const Pokemon& src, GameType srcGame, GameType destGame) {
    if (srcGame == destGame) {
        Pokemon dst = src;
        dst.gameType_ = destGame;
        return dst;
    }

    // Convert to canonical (PA9), then to destination
    Pokemon canonical = toCanonical(src, srcGame);
    return fromCanonical(canonical, destGame);
}
