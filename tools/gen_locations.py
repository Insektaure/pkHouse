#!/usr/bin/env python3
"""Generate romfs/data/locations_<family>_en.txt from the PKHeX location strings.

PKHeX splits met locations into banks: ids 0-29999 live in the _00000 file,
30000+ in _30000, 40000+ in _40000 and 60000+ in _60000, each indexed from the
start of its bank. This flattens them into one "id<TAB>name" file per game
family, skipping blanks and the "------" placeholder rows.

Usage: python3 tools/gen_locations.py
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "external/PKHeX.Core/Resources/text/locations")
OUT = os.path.join(ROOT, "romfs/data")

# bank folder name -> (pkhex subdir, file stem, banks present)
FAMILIES = {
    "ScarletViolet":     ("gen9",  "sv",      [0, 30000, 40000, 60000]),
    "LegendsZA":         ("gen9a", "za",      [0, 30000, 40000, 60000]),
    "SwordShield":       ("gen8",  "swsh",    [0, 30000, 40000, 60000]),
    "BDSP":              ("gen8b", "bdsp",    [0, 30000, 40000, 60000]),
    "LegendsArceus":     ("gen8a", "la",      [0, 30000, 40000, 60000]),
    "LetsGo":            ("gen7",  "gg",      [0, 40000]),
    "FireRedLeafGreen":  ("gen3",  "rsefrlg", [0]),
}

PLACEHOLDER = "—" * 6  # "——————" marks an unused slot


def main():
    os.makedirs(OUT, exist_ok=True)
    for family, (subdir, stem, banks) in FAMILIES.items():
        entries = []
        for bank in banks:
            path = os.path.join(SRC, subdir, "text_%s_%05d_en.txt" % (stem, bank))
            if not os.path.exists(path):
                sys.exit("missing %s" % path)
            with open(path, encoding="utf-8") as f:
                for i, line in enumerate(f):
                    name = line.rstrip("\r\n").strip()
                    if not name or name == PLACEHOLDER:
                        continue
                    entries.append((bank + i, name))

        dst = os.path.join(OUT, "locations_%s_en.txt" % family)
        with open(dst, "w", encoding="utf-8") as f:
            for loc_id, name in entries:
                f.write("%d\t%s\n" % (loc_id, name))
        print("%-18s %4d locations -> %s" % (family, len(entries),
                                             os.path.basename(dst)))


if __name__ == "__main__":
    main()
