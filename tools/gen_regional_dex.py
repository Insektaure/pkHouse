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

Each species is followed by its variants for the Complete Living Dex (the
first entry of a species is its Living Dex slot and Pokedex place), those
that are in the game: a male and a female slot for each form that can be
either, flagged `visible` when they look different (PKHeX
QR7.GenderDifferences, plus Eevee) - the Living Dex with gender differences
splits only those, the all-genders one every one; its regional forms
(REGIONAL, checked against the forms the personal data has and SWSH's
regional flags); and the female form of Meowstic, Indeedee, Basculegion and
Oinkologne. Other forms (battle-only, cosmetic) are left out.

Usage: python3 tools/gen_regional_dex.py
"""
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PERSONAL = os.path.join(ROOT, "external/PKHeX/PKHeX.Core/Resources/byte/personal")
INCLUDE = os.path.join(ROOT, "include")

# The presence flag: a byte, or bit 6 of a byte.
PRESENT_BYTE = ("byte", 0x1C)   # PersonalInfo9SV / 9ZA
PRESENT_BIT6 = ("bit6", 0x21)   # PersonalInfo8SWSH / 8LA / 8BDSP

# FormStatsIndex / FormCount offsets, per entry size (PersonalInfo*): the form
# entry of (species, form > 0) is FormStatsIndex + form - 1 (PersonalInfo.FormIndex).
FORM_FIELDS = {0x50: (0x18, 0x1A), 0xB0: (0x1E, 0x20), 0x44: (0x1E, 0x20), 0x54: (0x1C, 0x20)}

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
    games = living_dex(tables)
    write_living_dex(games, complete_dex(games))


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


# Complete Living Dex: the same sections and species order, each species
# followed by its variants - a female slot for a visible gender difference,
# its regional forms, and the female form of the species whose genders are
# forms. Battle-only, cosmetic and other forms are left out.

# Regional forms, national numbering (the same in every game). PKHeX's
# RegionalFlags (PersonalInfo8SWSH 0x5A) is not usable: it misses Galarian
# Weezing / Stunfisk / birds and flags Galarian Zen Darmanitan.
REGIONAL = {
    # Alola
    19: [1], 20: [1], 26: [1], 27: [1], 28: [1], 37: [1], 38: [1], 50: [1], 51: [1],
    52: [1, 2], 53: [1], 74: [1], 75: [1], 76: [1], 88: [1], 89: [1], 103: [1], 105: [1],
    # Galar (Meowth 2 above)
    77: [1], 78: [1], 79: [1], 80: [2], 83: [1], 110: [1], 122: [1], 144: [1], 145: [1],
    146: [1], 199: [1], 222: [1], 263: [1], 264: [1], 554: [1], 555: [2], 562: [1], 618: [1],
    # Hisui
    58: [1], 59: [1], 100: [1], 101: [1], 157: [1], 211: [1], 215: [1], 503: [1], 549: [1],
    570: [1], 571: [1], 628: [1], 705: [1], 706: [1], 713: [1], 724: [1],
    # Paldea: Tauros' three breeds, Wooper
    128: [1, 2, 3], 194: [1],
}
# The SWSH RegionalFlags forms that are not regional forms of their own.
SWSH_FLAGGED_NOT_REGIONAL = {(555, 3)}   # Galarian Zen Darmanitan, battle-only

# Species whose gender is a form: form 1 is the female.
GENDER_FORMS = {678, 876, 902, 916}      # Meowstic, Indeedee, Basculegion, Oinkologne

# Visible gender differences, from PKHeX QR7.GenderDifferences (one bit per
# species, up to Gen 7; later ones are forms). Meowstic is in it but is a
# GENDER_FORMS species. Eevee's came with Let's Go, after that table.
_QR7_GENDER_DIFFERENCES = bytes([
    0x08, 0x10, 0x18, 0x06, 0x00, 0x36, 0x00, 0x00, 0x03, 0x00,
    0x30, 0x00, 0x02, 0x80, 0xC1, 0x08, 0x06, 0x00, 0x00, 0x04,
    0x60, 0x00, 0x04, 0x46, 0x4C, 0x8C, 0xD1, 0x22, 0x21, 0x01,
    0x00, 0x80, 0x03, 0x28, 0x0D, 0x00, 0x00, 0x00, 0x18, 0x38,
    0x0C, 0x10, 0x00, 0x40, 0x00, 0x00, 0x02, 0x00, 0x00, 0xF0,
    0xBF, 0x80, 0x0E, 0x01, 0x00, 0x38, 0x66, 0x3B, 0x03, 0x02,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x10, 0x40,
])
GENDER_DIFFERENCES = ({sp for sp in range(len(_QR7_GENDER_DIFFERENCES) * 8)
                       if _QR7_GENDER_DIFFERENCES[sp >> 3] >> (sp & 7) & 1}
                      - GENDER_FORMS) | {133}
# Regional forms with a gender difference of their own.
GENDERED_FORMS = {(215, 1)}              # Hisuian Sneasel

MALE, FEMALE, ANY_GENDER = 0, 1, 255     # Entry.gender, as PKM gender values

# Game -> personal binary, entry size, presence flag. Let's Go has no presence
# flag: everything of its Kanto dex is in it, Alolan forms included. FireRed /
# LeafGreen (Gen 3) have no gender differences nor regional forms, only the
# genders (PKHeX's Gen 3 table is in national order, no forms).
GAME_PERSONAL = {
    "SV":   ("personal_sv",   0x50, PRESENT_BYTE),
    "ZA":   ("personal_za",   0x50, PRESENT_BYTE),
    "SWSH": ("personal_swsh", 0xB0, PRESENT_BIT6),
    "LA":   ("personal_la",   0xB0, PRESENT_BIT6),
    "BDSP": ("personal_bdsp", 0x44, PRESENT_BIT6),
    "LGPE": ("personal_gg",   0x54, None),
    "FRLG": ("personal_fr",   0x1C, None),
}
GENDER_RATIO = {0x50: 0x0C, 0xB0: 0x12, 0x44: 0x12, 0x54: 0x12, 0x1C: 0x10}
SINGLE_GENDER = {0, 254, 255}            # male only, female only, genderless


class Personal:
    """One game's personal table: form entries, presence, gender ratio."""
    def __init__(self, binary, size, presence):
        self.data = open(os.path.join(PERSONAL, binary), "rb").read()
        self.size, self.presence = size, presence
        self.fsi_off, self.fc_off = FORM_FIELDS.get(size, (None, None))   # Gen 3: none

    def entry(self, species, form):
        """The entry index of (species, form), or None when it has no such form."""
        d, z = self.data, self.size
        if form == 0:
            return species
        if self.fsi_off is None:
            return None
        fsi = struct.unpack_from("<H", d, species * z + self.fsi_off)[0]
        if not fsi or form >= d[species * z + self.fc_off]:
            return None
        return fsi + form - 1

    def present(self, species, form):
        i = self.entry(species, form)
        if i is None:
            return False
        if self.presence is None:
            return True
        kind, off = self.presence
        flag = self.data[i * self.size + off]
        return flag != 0 if kind == "byte" else (flag >> 6) & 1 == 1

    def both_genders(self, species, form):
        i = self.entry(species, form)
        return i is not None and self.data[i * self.size + GENDER_RATIO[self.size]] not in SINGLE_GENDER


def check_regional():
    """Stops unless every regional form exists, and SWSH's flags agree."""
    sv = Personal("personal_sv", 0x50, PRESENT_BYTE)   # knows every form up to Gen 9
    for sp, forms in REGIONAL.items():
        for f in forms:
            if sv.entry(sp, f) is None:
                sys.exit(f"regional form {sp}-{f}: no such form in personal_sv - stopping")
    swsh = Personal("personal_swsh", 0xB0, PRESENT_BIT6)
    for sp in range(1, 899):
        for f in range(1, swsh.data[sp * 0xB0 + 0x20]):
            i = swsh.entry(sp, f)
            if i is None or not swsh.present(sp, f):
                continue
            flagged = struct.unpack_from("<H", swsh.data, i * 0xB0 + 0x5A)[0] & 1
            if flagged and f not in REGIONAL.get(sp, []) and (sp, f) not in SWSH_FLAGGED_NOT_REGIONAL:
                sys.exit(f"SWSH flags {sp}-{f} as regional, not in REGIONAL - stopping")


def variants(pi, game, sp):
    """The slots of one species in a Complete Living Dex, in order:
    (species, form, gender, visible). A form that can be either gender gets a
    male and a female slot; `visible` when they look different - the Living
    Dex with gender differences only splits those, the all-genders one all."""
    forms = [0] + REGIONAL.get(sp, []) + ([1] if sp in GENDER_FORMS else [])
    if game == "LGPE":                    # Alolan forms only, no presence flag
        forms = [f for f in forms if f == 0 or (sp <= 151 and f in REGIONAL.get(sp, []))]
    if game == "FRLG":                    # Gen 3: no forms, no gender differences
        forms = [0]
    out = []
    for f in sorted(set(forms)):
        if not pi.present(sp, f):
            continue
        visible = game != "FRLG" and ((f == 0 and sp in GENDER_DIFFERENCES) or (sp, f) in GENDERED_FORMS)
        # Meowstic & co: the form is the gender already.
        if sp not in GENDER_FORMS and pi.both_genders(sp, f):
            out += [(sp, f, MALE, int(visible)), (sp, f, FEMALE, int(visible))]
        else:
            out.append((sp, f, ANY_GENDER, 0))
    # In the dex through a form left out here (none today): one slot anyway.
    return out or [(sp, 0, ANY_GENDER, 0)]


def complete_dex(games):
    check_regional()
    out = []
    for game, sections in games:
        pi = Personal(*GAME_PERSONAL[game])
        out.append((game, [(label, [e for sp in species for e in variants(pi, game, sp)])
                           for label, species in sections]))
    return out


def write_living_dex(games, complete):
    # The plain Living Dex and the game-Pokedex sort read their species order
    # off these entries: each species at its first entry. Stop unless that
    # gives back exactly the species lists.
    for (game, sections), (_, csections) in zip(games, complete):
        for (label, species), (_, entries) in zip(sections, csections):
            seen, derived = set(), []
            for sp, *_ in entries:
                if sp not in seen:
                    seen.add(sp)
                    derived.append(sp)
            if derived != species:
                sys.exit(f"living dex {game} {label}: entries do not give back the species order - stopping")
    out = ["// Generated by tools/gen_regional_dex.py from the PKHeX personal data - do not edit.",
           "// Each game's Living Dex layout: its Pokedex sections in order, each the national",
           "// species in that Pokedex's order, one per species at its first appearance,",
           "// each species' entries one after the other. The first entry of a species is",
           "// its slot in a Living Dex and its place in the game's Pokedex order; the next",
           "// are its variants, for a Complete Living Dex - a female slot (gender MALE then",
           "// FEMALE), its regional forms, the female form of the species whose genders",
           "// are forms.",
           "// gender is the PKM gender a slot takes (0 male, 1 female), or ANY_GENDER.",
           "// visible: the two genders look different. The Living Dex with gender",
           "// differences splits only those; for it the others are one ANY_GENDER slot.",
           "#pragma once", "#include <cstdint>", "", "namespace LivingDexData {", "",
           "struct Entry { uint16_t species; uint8_t form; uint8_t gender; uint8_t visible; };",
           f"inline constexpr uint8_t ANY_GENDER = {ANY_GENDER};",
           "struct Section { const Entry* entries; uint16_t count; };", ""]
    for game, sections in complete:
        for label, entries in sections:
            out.append(f"inline constexpr Entry {game}_{label}[{len(entries)}] = {{")
            for i in range(0, len(entries), 10):
                out.append("    " + "".join(f"{{{sp},{f},{g},{v}}}," for sp, f, g, v in entries[i:i + 10]))
            out.append("};")
        out.append(f"inline constexpr Section {game}[{len(sections)}] = {{")
        out.append("    " + ", ".join(f"{{{game}_{label}, {len(e)}}}" for label, e in sections))
        out.append("};")
        out.append("")
    out.append("} // namespace LivingDexData")
    with open(os.path.join(INCLUDE, "living_dex_data.h"), "w", newline="\n") as f:
        f.write("\n".join(out) + "\n")
    for (game, sections), (_, csections) in zip(games, complete):
        entries = [e for _, es in csections for e in es]
        visible = len({(sp, f, g if v else ANY_GENDER) for sp, f, g, v in entries})
        print(f"  living_dex_data.h {game}: " + " + ".join(f"{label.lower()} {len(sp)}" for label, sp in sections)
              + f" species; with gender differences {visible} slots; all genders {len(entries)} slots")

if __name__ == "__main__":
    main()
