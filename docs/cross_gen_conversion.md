# Cross-Generation Pokemon Conversion

## Overview

pkHouse supports transferring Pokemon between all supported games via a **hub-and-spoke** conversion model. PA9 (Legends: Z-A / Gen9) is the canonical hub format. Every cross-gen transfer goes through two steps:

```
Source Format → toCanonical() → PA9 → fromCanonical() → Destination Format
```

Same-game transfers are a no-op (data passes through unchanged).

---

## Supported Formats

| Format | Games               | Party Size | Stored Size | Block Size | Block Count |
|--------|---------------------|------------|-------------|------------|-------------|
| PK9/PA9| ZA, Scarlet, Violet | 0x158 (344)| 0x148 (328) | 0x50 (80)  | 4           |
| PK8    | Sword, Shield       | 0x158 (344)| 0x148 (328) | 0x50 (80)  | 4           |
| PB8    | Brilliant Diamond, Shining Pearl | 0x158 (344) | 0x148 (328) | 0x50 (80) | 4 |
| PA8    | Legends: Arceus     | 0x178 (376)| 0x168 (360) | 0x58 (88)  | 4           |
| PB7    | Let's Go Pikachu/Eevee | 0x104 (260) | 0xE8 (232) | 0x38 (56) | 4         |
| PK3    | FireRed, LeafGreen  | 100        | 80          | 12         | 4           |

PK8, PB8, PK9, and PA9 share the same total size (0x158 party) but have different field layouts in Block C (after offset 0xCD) and different species ID encoding.

---

## Conversion Pipeline

### Step 1: Legality Check (`canTransfer`)

Before any data conversion, the transfer is validated:

1. **Outbound check** — Can this Pokemon leave its source game?
   - LGPE starter Pikachu/Eevee (form 8) cannot leave
   - Ride-form Koraidon/Miraidon cannot leave SV

2. **Inbound check** — Can this Pokemon enter the destination game?
   - Species + form must exist in the destination game's PersonalInfo table
   - BDSP blocks Spinda and Nincada
   - FRLG only accepts species 1–386

Species presence is checked against per-game PersonalInfo tables (`personal_*.h`), which list every valid species+form combination.

### Step 2: Convert to Canonical (`toCanonical`)

Converts the source format to PA9 (the hub). The conversion depends on the source format:

| Source     | Function           | Key Operations |
|------------|--------------------|----------------|
| PK8/PB8   | `convertSameSize`  | Species ID national→internal9, gender bit shift, Block C field remap, height/weight offset swap |
| PA8        | `convertPA8toPA9`  | Full field-by-field remap (different block sizes), species national→internal9, gender bit shift, IsAlpha byte→flag, OT Memory layout fix |
| PB7        | `convertPB7toPA9`  | Complete field remap (entirely different layout), ability u8→u16, PID/nature offset changes, packed flag byte unpacking |
| PK3        | `convertPK3toPA9`  | Radical remap (Gen3 layout), species Gen3-internal→national→internal9, packed origins word, PP ups unpacking |
| SV/ZA      | (no-op)            | Already PA9 |

### Step 3: Convert from Canonical (`fromCanonical`)

Converts PA9 to the destination format. Reverse of step 2, plus **sanitization**:

| Destination | Function           | Key Operations |
|-------------|--------------------|----------------|
| PK8/PB8     | `convertSameSize`  | Species internal9→national, gender bit shift, Block C remap, height/weight swap, sanitize moves + items |
| PA8         | `convertPA9toPA8`  | Full field remap, species internal9→national, PLA move sanitization with PP correction, OT Memory layout fix |
| PB7         | `convertPA9toPB7`  | Complete remap, ability u16→u8, packed flag byte, LGPE move filtering |
| PK3         | `convertPA9toPK3`  | Radical remap, species internal9→national→Gen3-internal, packed origins, PP ups packing |
| SV/ZA       | (no-op)            | Already PA9 |

---

## Field Remapping Details

### PK8/PB8 ↔ PK9/PA9 (Same Size, Different Layout)

These formats share the same 0x158 size but Block C diverges after 0xCD:

| Field          | Gen8 (PK8/PB8) | Gen9 (PK9/PA9) |
|----------------|-----------------|-----------------|
| Version        | 0xDE            | 0xCE            |
| BattleVersion  | 0xDF            | 0xCF            |
| FormArgument   | 0xE4 (4 bytes)  | 0xD0 (4 bytes)  |
| AffixedRibbon  | 0xE8            | 0xD4            |
| Language       | 0xE2            | 0xD5            |
| HeightScalar   | 0x50            | 0x48            |
| WeightScalar   | 0x51            | 0x49            |
| Scale          | (none)          | 0x4A            |
| Gender bits    | bits 2-3 @ 0x22 | bits 1-2 @ 0x22 |
| Species ID     | national dex    | Gen9 internal   |

Gen8-only fields cleared on export: CanGigantamax (0x16 bit 4), DynamaxLevel (0x90).

### PA8 ↔ PA9 (Different Block Sizes)

PA8 uses 0x58-byte blocks vs PA9's 0x50-byte blocks. Every field after Block A header requires offset remapping:

| Field          | PA8             | PA9             |
|----------------|-----------------|-----------------|
| Moves          | 0x54            | 0x72            |
| Move PP        | 0x5C            | 0x7A            |
| Nickname       | 0x60            | 0x58            |
| PP Ups         | 0x86            | 0x7E            |
| Relearn Moves  | 0x8A            | 0x82            |
| IV32           | 0x94            | 0x8C            |
| HT Name        | 0xB8            | 0xA8            |
| OT Name        | 0x110           | 0xF8            |
| Level          | 0x168           | 0x148           |
| IsAlpha        | 0x16 bit 5      | 0x23            |
| Height/Weight  | 0x50/0x51       | 0x48/0x49       |

**OT Memory layout mismatch** (critical):
- PA9: `[Intensity 0x113][Memory 0x114][Feeling 0x115][TextVar 0x116-0x117]`
- PA8: `[Intensity 0x12B][Memory 0x12C][gap 0x12D][TextVar 0x12E-0x12F][Feeling 0x130]`

These must be mapped field-by-field, not with memcpy.

### PB7 ↔ PA9 (Gen7b — Let's Go)

Completely different layout. Key differences:
- Ability is u8 (PB7 0x14) vs u16 (PA9 0x14)
- PID at 0x18 (PB7) vs 0x1C (PA9)
- Nature at 0x1C (PB7) vs 0x20 (PA9)
- Gender + Form + Fateful packed into one byte (PB7 0x1D) vs separate fields (PA9)
- Moves at 0x5A (PB7) vs 0x72 (PA9)
- Nickname 24 bytes (PB7) vs 26 bytes (PA9)

### PK3 ↔ PA9 (Gen3 — FireRed/LeafGreen)

Radically different structure:
- PID at 0x00 doubles as EncryptionConstant
- Species uses Gen3-internal IDs (require `SpeciesConverter` lookup tables)
- Origins word at 0x46 packs: MetLevel (bits 0-6), Version (7-10), Ball (11-14), OTGender (15)
- PP Ups packed as 2-bit fields in a single byte at 0x28
- Names use Gen3 character encoding (not UTF-16LE)

---

## Move Sanitization

Every `fromCanonical` conversion applies move sanitization to prevent the destination game from rejecting the Pokemon (showing as an egg or crashing).

### Dummied Moves

Each game has moves that exist in data but are flagged as unusable. Pokemon with dummied moves are treated as invalid by the game engine. pkHouse uses per-game bitfield tables (ported from PKHeX `MoveInfo*.cs`) to filter these:

| Game    | Bitfield Size | Approx. Dummied Count |
|---------|---------------|----------------------|
| SwSh    | 93 bytes      | ~144 moves           |
| BDSP    | 104 bytes     | ~314 moves           |
| PLA     | 104 bytes     | ~673 moves (only ~90 valid) |
| SV/ZA   | 100 bytes     | ~251 moves           |
| LGPE    | whitelist     | only 142+165 allowed |

### Sanitization Steps

1. **Max move ID check** — Clear moves above the game's max (e.g., 826 for SwSh/BDSP, 354 for Gen3)
2. **Dummied move filter** — Check each move against the game's bitfield; zero out dummied moves
3. **PLA PP correction** — PLA has its own PP table; move PP values are replaced with PLA-specific values
4. **LGPE whitelist** — Moves ≤164 are all allowed; higher moves checked against explicit whitelist
5. **Compact** — Shift remaining non-zero moves to front, zero-fill the rest

### Item Sanitization

Held items above the game's max item ID are cleared:
- Gen9 (SV/ZA): max 2557
- SwSh: max 1607
- BDSP: max 1822
- PLA: max 1828
- LGPE: max 959
- Gen3: max 376

---

## Universal Bank Storage

The universal bank stores Pokemon in their **original decrypted format** — no conversion happens on deposit or withdrawal. Each slot uses `MAX_PARTY_SIZE` (0x178 = 376 bytes) with zero-padding for smaller formats.

File layout per slot:
```
[1 byte]   Origin game tag (encoded GameType)
[376 bytes] Decrypted Pokemon data (original format, zero-padded)
```

The PA9 hub is **only used as an intermediate during cross-game transfers**, never for storage. Conversion only happens when placing a Pokemon into a context with a different game than its origin:

```
Pick from universal bank (origin = SwSh, data = PK8)
  → Place into universal bank → no conversion, stored as PK8
  → Place into SwSh save/bank → no conversion, same game
  → Place into PLA save/bank  → convert(PK8, SwSh, LA): PK8 → PA9 → PA8
```

---

## Species ID Systems

Three different species numbering systems are in use:

| System          | Used By            | Example: Terapagos |
|-----------------|--------------------|--------------------|
| National Dex    | PK8, PB8, PA8, PB7 | 1024              |
| Gen9 Internal   | PK9, PA9           | (internal ID)      |
| Gen3 Internal   | PK3                | (Gen3 ID)          |

Conversion tables in `species_converter.h/cpp`:
- `getInternal9(national)` — national → Gen9 internal
- `getNational9(internal9)` — Gen9 internal → national
- `getInternal3(national)` — national → Gen3 internal
- `getNational3(internal3)` — Gen3 internal → national
