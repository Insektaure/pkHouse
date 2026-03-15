#!/usr/bin/env python3
"""
Copy form-variant sprites from pokesprite into romfs/sprites/ and romfs/sprites_shiny/.

Naming convention: {dex}-{form_number}.png
  e.g. 019-1.png = Alolan Rattata (species 19, form 1)

Source: /home/prs/dev/pokesprite/images/
  pokesprite uses text suffixes like 019-alola.png, 487-origin.png, etc.

This script maps (species, form_number) -> pokesprite suffix, then copies the files.
"""

import shutil
import os

SRC = "/home/prs/dev/pokesprite/images"
DST_NORMAL = "/home/prs/dev/switch/pkHouse/romfs/sprites"
DST_SHINY  = "/home/prs/dev/switch/pkHouse/romfs/sprites_shiny"

# Mapping: (species, form_number) -> pokesprite suffix (without shiny prefix)
# Only includes forms that have distinct sprites in pokesprite.
FORM_MAP = {}

def add(species, form, suffix):
    FORM_MAP[(species, form)] = suffix

def add_range(species, forms):
    """forms is a dict {form_number: suffix}"""
    for f, s in forms.items():
        add(species, f, s)

# --- Regional forms (form 1) ---
for sp in [19,20,26,27,28,37,38,50,51,52,53,74,75,76,88,89,103,105]:
    add(sp, 1, "alola")
for sp in [77,78,79,80,83,110,122,144,145,146,199,222,263,264,554,555,562,618]:
    add(sp, 1, "galar")
for sp in [58,59,100,101,157,211,215,503,549,570,571,628,705,706,713,724]:
    add(sp, 1, "hisui")
for sp in [128, 194]:
    add(sp, 1, "paldea")

# Meowth form 2 = Galarian
add(52, 2, "galar")

# --- Species-specific forms ---

# Tauros 128: form 1 already set to paldea above; forms 2,3 have same sprite in pokesprite
# (pokesprite only has 128-paldea.png, no blaze/aqua variants)

# Unown 201 (form 0=A is base, forms 1-25=B-Z, 26=!, 27=?)
for i in range(1, 26):
    add(201, i, chr(ord('b') + i - 1))
add(201, 26, "exclamation")
add(201, 27, "question")

# Castform 351
add_range(351, {1: "sunny", 2: "rainy", 3: "snowy"})

# Deoxys 386
add_range(386, {1: "attack", 2: "defense", 3: "speed"})

# Burmy 412 / Wormadam 413
for sp in [412, 413]:
    add_range(sp, {1: "sandy", 2: "trash"})

# Cherrim 421
add(421, 1, "sunshine")

# Shellos 422 / Gastrodon 423
# pokesprite base = East Sea (form 0), -west = West Sea
# Actually form 0 = West, form 1 = East in game data.
# pokesprite has {dex}.png (base) and {dex}-west.png — no -east.
# So form 1 (East) has no distinct sprite in pokesprite beyond base. Skip.

# Rotom 479
add_range(479, {1: "heat", 2: "wash", 3: "frost", 4: "fan", 5: "mow"})

# Dialga 483 / Palkia 484 — pokesprite has no -origin, only -legends_arceus
# Skip (no standard origin sprite available)

# Giratina 487
add(487, 1, "origin")

# Shaymin 492
add(492, 1, "sky")

# Arceus 493 / Silvally 773 — type forms look identical in sprites, skip

# Basculin 550
add_range(550, {1: "blue-striped", 2: "white-striped"})

# Darmanitan 555 (form 1 = galar already set above)
add(555, 2, "zen")
add(555, 3, "galar-zen")

# Deerling 585 / Sawsbuck 586
for sp in [585, 586]:
    add_range(sp, {1: "summer", 2: "autumn", 3: "winter"})

# Tornadus 641 / Thundurus 642 / Landorus 645 / Enamorus 905
for sp in [641, 642, 645, 905]:
    add(sp, 1, "therian")

# Kyurem 646
add_range(646, {1: "white", 2: "black"})

# Vivillon 666
add_range(666, {
    1: "polar", 2: "tundra", 3: "continental", 4: "garden",
    5: "elegant", 6: "meadow", 7: "modern", 8: "marine",
    9: "archipelago", 10: "high-plains", 11: "sandstorm", 12: "river",
    13: "monsoon", 14: "savanna", 15: "sun", 16: "ocean",
    17: "jungle", 18: "fancy", 19: "poke-ball"
})

# Flabebe 669 / Floette 670 / Florges 671
for sp in [669, 670, 671]:
    add_range(sp, {1: "yellow", 2: "orange", 3: "blue", 4: "white"})
    # form 5 "Eternal" — only for Floette, pokesprite doesn't have it

# Furfrou 676
add_range(676, {
    1: "heart", 2: "star", 3: "diamond", 4: "debutante",
    5: "matron", 6: "dandy", 7: "la-reine", 8: "kabuki", 9: "pharaoh"
})

# Meowstic 678 / Indeedee 876 / Basculegion 902 / Oinkologne 916
# pokesprite doesn't have -female for these. Skip.

# Aegislash 681
add(681, 1, "blade")

# Pumpkaboo 710 / Gourgeist 711 — pokesprite has no size variants. Skip.

# Zygarde 718 — pokesprite has no form variants. Skip.

# Hoopa 720
add(720, 1, "unbound")

# Oricorio 741 — pokesprite has no form variants. Skip.

# Lycanroc 745 — pokesprite has no form variants. Skip.

# Wishiwashi 746 — pokesprite has no -school. Skip.

# Minior 774 — pokesprite has no form variants. Skip.

# Necrozma 800 — pokesprite has no form variants. Skip.

# Cramorant 845
add_range(845, {1: "gulping", 2: "gorging"})

# Toxtricity 849
add(849, 1, "low-key")

# Sinistea 854 / Polteageist 855
add(854, 1, "antique")
add(855, 1, "antique")

# Alcremie 869
add_range(869, {
    1: "ruby-cream", 2: "matcha-cream", 3: "mint-cream",
    4: "lemon-cream", 5: "salted-cream", 6: "ruby-swirl",
    7: "caramel-swirl", 8: "rainbow-swirl"
})

# Eiscue 875
add(875, 1, "noice")

# Morpeko 877
add(877, 1, "hangry")

# Zacian 888 / Zamazenta 889
add(888, 1, "crowned")
add(889, 1, "crowned")

# Eternatus 890
add(890, 1, "eternamax")

# Urshifu 892
add(892, 1, "rapid")

# Calyrex 898 — pokesprite has no rider forms. Skip.

# Ogerpon 1017 — pokesprite has no form variants. Skip.

# Terapagos 1024 — pokesprite has no form variants. Skip.


def copy_sprite(species, form, suffix):
    dex = f"{species:03d}"
    dst_name = f"{dex}-{form}.png"

    # Normal sprite
    src_normal = os.path.join(SRC, f"{dex}-{suffix}.png")
    dst_normal = os.path.join(DST_NORMAL, dst_name)
    if os.path.exists(src_normal):
        shutil.copy2(src_normal, dst_normal)
    else:
        print(f"  MISSING normal: {src_normal}")

    # Shiny sprite
    src_shiny = os.path.join(SRC, f"{dex}-shiny-{suffix}.png")
    dst_shiny = os.path.join(DST_SHINY, dst_name)
    if os.path.exists(src_shiny):
        shutil.copy2(src_shiny, dst_shiny)
    # No warning for missing shiny — many forms don't have shiny sprites


def main():
    copied = 0
    missing = 0

    for (species, form), suffix in sorted(FORM_MAP.items()):
        dex = f"{species:03d}"
        src_normal = os.path.join(SRC, f"{dex}-{suffix}.png")
        if os.path.exists(src_normal):
            copy_sprite(species, form, suffix)
            copied += 1
        else:
            print(f"SKIP {dex} form {form} ({suffix}): no pokesprite file")
            missing += 1

    print(f"\nDone: {copied} form sprites copied, {missing} skipped")


if __name__ == "__main__":
    main()
