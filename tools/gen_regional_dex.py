#!/usr/bin/env python3
"""Generate the regional Pokedex tables (include/<game>_dex_<dex>.inc) from the
PKHeX personal binaries, in the format of the existing sv_dex_*.inc.

Each table has one value per entry of the game's personal table (species, then
the extra form entries, as FORM_STATS_INDEX addresses them): the entry's number
in that Pokedex, or 0 when it is not in it. A table is included in a C array:

    static constexpr uint16_t ZA_DEX[1445] = {
    #include "za_dex.inc"
    };

The fields, per PKHeX.Core/PersonalInfo/Info:
  PersonalInfo9SV    DexPaldea u16 @0x1E, DexKitakami u8 @0x4A, DexBlueberry u8 @0x4B
  PersonalInfo9ZA    DexIndex  u16 @0x1E - 1..232 Lumiose, above 232 Hyperspace (the DLC)
  PersonalInfo8SWSH  PokeDexIndex u16 @0x5C (Galar), ArmorDexIndex u16 @0xAC, CrownDexIndex u16 @0xAE
  PersonalInfo8LA    DexIndexHisui u16 @0x60
  PersonalInfo8BDSP  PokeDexIndex u16 @0x42 (Sinnoh)
Let's Go and FireRed / LeafGreen have no regional field: their order is the
national one (Kanto first).

Only entries present in the game get a number, as PKHeX registers them
(Zukan9a / Zukan9 check IsPresentInGame before any Pokedex field): Z-A's table
carries leftover Pokedex numbers on 279 entries (species and forms) that are not in the game -
Diglett, Torkoal, Bronzor... with the same numbers as Bulbasaur, Venusaur,
Charizard - which would otherwise make two species share one number. The
presence flag is PersonalInfo9SV/9ZA byte 0x1C, and bit 6 of byte 0x21 in
PersonalInfo8SWSH/8LA/8BDSP; it changes nothing in the other tables.

The three Scarlet / Violet tables already in include/ were extracted the same
way; the script regenerates them first and stops unless they come out byte for
byte identical, which checks the reading and the format before anything new is
written. Every table is then checked before it is written, and the script
stops when one fails: numbers run from 1 with no gap, each number belongs to a
single species (its forms may share it), and only species and form entries
carry one. Where PKHeX states a size elsewhere, it is checked too (Zukan8's
Galar / Armor / Crown counts, the Hisui research tasks, Z-A's Lumiose boundary).

It also writes include/living_dex_data.h: each game's Living Dex layout, as
the national species of each Pokedex section in order - one per species, at
its first appearance across the game's Pokedexes, regional forms counting for
their species (Hisuian Growlithe puts Growlithe in the Hisui dex). The sort
(source/bank_sort.cpp) needs nothing else.

Usage: python3 tools/gen_regional_dex.py
"""
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PERSONAL = os.path.join(ROOT, "external/PKHeX.Core/Resources/byte/personal")
INCLUDE = os.path.join(ROOT, "include")

# The presence flag: a byte, or bit 6 of a byte.
PRESENT_BYTE = ("byte", 0x1C)   # PersonalInfo9SV / 9ZA
PRESENT_BIT6 = ("bit6", 0x21)   # PersonalInfo8SWSH / 8LA / 8BDSP

# FormStatsIndex / FormCount offsets, per entry size (PersonalInfo*): the form
# entry of (species, form > 0) is FormStatsIndex + form - 1 (PersonalInfo.FormIndex).
FORM_FIELDS = {0x50: (0x18, 0x1A), 0xB0: (0x1E, 0x20), 0x44: (0x1E, 0x20)}

# Sizes PKHeX states outside the personal data (highest number expected), or
# for Z-A the Lumiose / Hyperspace boundary (PersonalInfo9ZA.IsLumioseNative).
EXPECTED_MAX = {
    "swsh_dex_galar": 400,   # Zukan8.GalarCount
    "swsh_dex_armor": 211,   # Zukan8.Rigel1Count
    "swsh_dex_crown": 210,   # Zukan8.Rigel2Count
    "la_dex_hisui": 242,     # researchtask_la.pkl: one task list per Hisui number
}
ZA_LUMIOSE_LAST = 232

# output name, binary, entry size, expected entry count, field offset, field size, presence
TABLES = [
    ("sv_dex_paldea",    "personal_sv",   0x50, 1424, 0x1E, 2, PRESENT_BYTE),
    ("sv_dex_kitakami",  "personal_sv",   0x50, 1424, 0x4A, 1, PRESENT_BYTE),
    ("sv_dex_blueberry", "personal_sv",   0x50, 1424, 0x4B, 1, PRESENT_BYTE),
    ("za_dex",           "personal_za",   0x50, 1445, 0x1E, 2, PRESENT_BYTE),
    ("swsh_dex_galar",   "personal_swsh", 0xB0, 1192, 0x5C, 2, PRESENT_BIT6),
    ("swsh_dex_armor",   "personal_swsh", 0xB0, 1192, 0xAC, 2, PRESENT_BIT6),
    ("swsh_dex_crown",   "personal_swsh", 0xB0, 1192, 0xAE, 2, PRESENT_BIT6),
    ("la_dex_hisui",     "personal_la",   0xB0, 1276, 0x60, 2, PRESENT_BIT6),
    ("bdsp_dex_sinnoh",  "personal_bdsp", 0x44,  560, 0x42, 2, PRESENT_BIT6),
]
# Already in the repository: regenerated only to check the script against them.
CHECK_ONLY = {"sv_dex_paldea", "sv_dex_kitakami", "sv_dex_blueberry"}


def read_table(binary, size, count, offset, width, presence):
    data = open(os.path.join(PERSONAL, binary), "rb").read()
    if len(data) != size * count:
        sys.exit(f"{binary}: {len(data)} bytes, expected {count} entries of {size:#x}")
    fmt = "<H" if width == 2 else "<B"
    kind, poff = presence
    values = []
    for i in range(count):
        flag = data[i * size + poff]
        present = flag != 0 if kind == "byte" else (flag >> 6) & 1 == 1
        values.append(struct.unpack_from(fmt, data, i * size + offset)[0] if present else 0)
    return values


def check(name, binary, size, values):
    """Stops the script unless the table is a clean Pokedex order."""
    data = open(os.path.join(PERSONAL, binary), "rb").read()
    fsi_off, fc_off = FORM_FIELDS[size]
    u16 = lambda i, o: struct.unpack_from("<H", data, i * size + o)[0]
    count = len(values)
    # Base entries run up to the first form entry (the lowest FormStatsIndex).
    last_species = min(u16(i, fsi_off) for i in range(1, count) if u16(i, fsi_off)) - 1
    owner = {}
    for sp in range(1, last_species + 1):
        owner[sp] = sp
        fsi, fc = u16(sp, fsi_off), data[sp * size + fc_off]
        for form in range(1, fc):
            if fsi:
                owner.setdefault(fsi + form - 1, sp)
    by_number = {}
    for i, v in enumerate(values):
        if not v:
            continue
        if i not in owner:
            sys.exit(f"{name}: entry {i} is no species or form but has #{v} - stopping")
        by_number.setdefault(v, set()).add(owner[i])
    top = max(by_number)
    gaps = [n for n in range(1, top + 1) if n not in by_number]
    shared = {n: s for n, s in by_number.items() if len(s) > 1}
    if gaps:
        sys.exit(f"{name}: no species has {gaps[:10]} - stopping")
    if shared:
        sys.exit(f"{name}: numbers held by several species {dict(list(shared.items())[:5])} - stopping")
    if name in EXPECTED_MAX and top != EXPECTED_MAX[name]:
        sys.exit(f"{name}: highest #{top}, PKHeX says {EXPECTED_MAX[name]} - stopping")
    if name == "za_dex" and top <= ZA_LUMIOSE_LAST:
        sys.exit(f"{name}: nothing past Lumiose #{ZA_LUMIOSE_LAST}: the Hyperspace (DLC) entries are missing - stopping")
    return top


def owners(binary, size):
    """Entry index -> national species, for the base entries and the forms."""
    data = open(os.path.join(PERSONAL, binary), "rb").read()
    fsi_off, fc_off = FORM_FIELDS[size]
    count = len(data) // size
    u16 = lambda i, o: struct.unpack_from("<H", data, i * size + o)[0]
    last_species = min(u16(i, fsi_off) for i in range(1, count) if u16(i, fsi_off)) - 1
    own = {}
    for sp in range(1, last_species + 1):
        own[sp] = sp
        fsi, fc = u16(sp, fsi_off), data[sp * size + fc_off]
        for form in range(1, fc):
            if fsi:
                own.setdefault(fsi + form - 1, sp)
    return own


def species_by_number(values, own, lo=1, hi=10 ** 6):
    """The species of each number in [lo, hi], in number order."""
    first = {}
    for i, v in enumerate(values):
        if v and lo <= v <= hi and i in own:
            first.setdefault(v, own[i])
    return [first[n] for n in sorted(first)]


def render(values):
    lines = []
    for i in range(0, len(values), 20):
        lines.append("    " + "".join(f"{v}," for v in values[i:i + 20]))
    return "\n".join(lines) + "\n"


def main():
    tables = {}
    for name, binary, size, count, offset, width, presence in TABLES:
        values = read_table(binary, size, count, offset, width, presence)
        tables[name] = values
        text = render(values)
        path = os.path.join(INCLUDE, name + ".inc")
        if name in CHECK_ONLY:
            if open(path).read() != text:
                sys.exit(f"{name}.inc: regenerated table differs from the one in include/ - stopping")
            print(f"  {name}.inc: identical to the existing table")
            continue
        top = check(name, binary, size, values)
        with open(path, "w", newline="\n") as f:
            f.write(text)
        nonzero = [v for v in values if v]
        print(f"  {name}.inc: {count} entries, {len(nonzero)} in the dex, #1 to #{top}, checked")
    write_living_dex(living_dex(tables))


# Living Dex layouts: game, then its sections (C name, table or range source).
def living_dex(tables):
    own = {}
    def owner_of(binary, size):
        if binary not in own:
            own[binary] = owners(binary, size)
        return own[binary]
    info = {name: (binary, size) for name, binary, size, *_ in TABLES}
    def sec(name, lo=1, hi=10 ** 6):
        binary, size = info[name]
        return species_by_number(tables[name], owner_of(binary, size), lo, hi)
    games = [
        ("SV",   [("PALDEA", sec("sv_dex_paldea")), ("KITAKAMI", sec("sv_dex_kitakami")),
                  ("BLUEBERRY", sec("sv_dex_blueberry"))]),
        ("SWSH", [("GALAR", sec("swsh_dex_galar")), ("ARMOR", sec("swsh_dex_armor")),
                  ("CROWN", sec("swsh_dex_crown"))]),
        ("ZA",   [("LUMIOSE", sec("za_dex", 1, ZA_LUMIOSE_LAST)),
                  ("HYPERSPACE", sec("za_dex", ZA_LUMIOSE_LAST + 1))]),
        ("LA",   [("HISUI", sec("la_dex_hisui"))]),
        ("BDSP", [("SINNOH", sec("bdsp_dex_sinnoh"))]),
        ("LGPE", [("KANTO", list(range(1, 152)) + [808, 809])]),   # Meltan, Melmetal
        ("FRLG", [("NATIONAL", list(range(1, 387)))]),
    ]
    # BD / SP: Sinnoh, then the rest of the national dex, as the game does.
    sinnoh = set(games[4][1][0][1])
    games[4][1].append(("NATIONAL", [sp for sp in range(1, 494) if sp not in sinnoh]))
    for game, sections in games:
        seen = set()
        for i, (label, species) in enumerate(sections):
            fresh = [sp for sp in species if sp not in seen]   # first appearance wins
            if not fresh:
                sys.exit(f"living dex {game} {label}: empty section - stopping")
            seen.update(fresh)
            sections[i] = (label, fresh)
    return games


def write_living_dex(games):
    out = ["// Generated by tools/gen_regional_dex.py from the PKHeX personal data - do not edit.",
           "// Each game's Living Dex layout: its Pokedex sections in order, each the national",
           "// species numbers in that Pokedex's order, one per species at its first appearance.",
           "#pragma once", "#include <cstdint>", "", "namespace LivingDexData {", "",
           "struct Section { const uint16_t* species; uint16_t count; };", ""]
    for game, sections in games:
        for label, species in sections:
            out.append(f"inline constexpr uint16_t {game}_{label}[{len(species)}] = {{")
            for i in range(0, len(species), 20):
                out.append("    " + "".join(f"{sp}," for sp in species[i:i + 20]))
            out.append("};")
        out.append(f"inline constexpr Section {game}[{len(sections)}] = {{")
        out.append("    " + ", ".join(f"{{{game}_{label}, {len(species)}}}" for label, species in sections))
        out.append("};")
        out.append("")
    out.append("} // namespace LivingDexData")
    with open(os.path.join(INCLUDE, "living_dex_data.h"), "w", newline="\n") as f:
        f.write("\n".join(out) + "\n")
    for game, sections in games:
        print(f"  living_dex_data.h {game}: " + " + ".join(f"{label.lower()} {len(sp)}" for label, sp in sections))


if __name__ == "__main__":
    main()
