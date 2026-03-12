# Cross-Gen Conversion Test Plan

## Species Blocking (isPresentInGame)

- [ ] Scorbunny (SV) -> PLA: should be **blocked**
- [ ] Pikachu (SV) -> PLA: should be **allowed**
- [ ] Charizard (SwSh) -> BDSP: should be **allowed**
- [ ] Spinda (SwSh) -> BDSP: should be **blocked** (hardcoded rule)
- [ ] Nincada (SV) -> BDSP: should be **blocked** (non-BDSP origin)
- [ ] Mew (LGPE) -> PLA: should be **blocked** (LGPE can only go to SwSh)
- [ ] Mew (LGPE) -> SwSh: should be **allowed**
- [ ] Partner Pikachu form 8 (LGPE) -> SwSh: should be **blocked**
- [ ] Mewtwo (SwSh) -> LGPE: should be **blocked** (no return to LGPE)
- [ ] Arceus (PLA) -> SV: should be **allowed**
- [ ] Bidoof (PLA) -> SV: should be **allowed**
- [ ] Garchomp (SV) -> FRLG: should be **blocked** (species > 386)
- [ ] Pikachu (SV) -> FRLG: should be **allowed**

## Gigantamax Blocks

- [ ] G-Max Pikachu (SwSh) -> BDSP: should be **blocked**
- [ ] G-Max Eevee (SwSh) -> BDSP: should be **blocked**
- [ ] G-Max Pikachu (SwSh) -> PLA: should be **blocked**
- [ ] Regular Pikachu (SwSh) -> BDSP: should be **allowed**

## Hyper Training Block

- [ ] Hyper Trained Lv50 mon (SV) -> BDSP: should be **blocked**
- [ ] Hyper Trained Lv100 mon (SV) -> BDSP: should be **allowed**

## Conversion Paths (field preservation)

For each path, verify: species name, nickname, OT name, TID/SID, IVs, EVs, nature, ability, held item, moves, ball, level, gender, shiny status.

### SV/ZA <-> PK8 family (SwSh/BDSP)

- [ ] SV -> SwSh: place Pokemon, pick it back, verify fields intact
- [ ] SwSh -> SV: same check
- [ ] SV -> BDSP: same check
- [ ] BDSP -> SV: same check
- [ ] Round-trip SV -> SwSh -> SV: verify no data loss

### SV/ZA <-> PLA

- [ ] SV -> PLA: verify fields (height/weight/scale remap, nickname, OT, moves)
- [ ] PLA -> SV: same check
- [ ] Round-trip SV -> PLA -> SV: verify no data loss
- [ ] PLA alpha mon -> SV: verify IsAlpha preserved
- [ ] SV mon -> PLA -> SV: verify IsAlpha is cleared (not an alpha)

### SV/ZA <-> LGPE

- [ ] LGPE -> SwSh: verify fields (ability u8->u16, PID offset, gender/form packed byte)
- [ ] SwSh -> LGPE: **should be blocked** (no return path)
- [ ] LGPE -> SV: **should be blocked** (LGPE can only go to SwSh)

### SV/ZA <-> FRLG

- [ ] FRLG -> SV: verify nickname (Gen3 charset -> UTF-16), OT name, species, IVs, nature
- [ ] SV -> FRLG: verify nickname (UTF-16 -> Gen3 charset), moves sanitized, item sanitized
- [ ] Round-trip FRLG -> SV -> FRLG: verify no data loss on shared fields

## Ball Handling

- [ ] SwSh mon -> PLA: ball should become Strange Ball
- [ ] SV mon -> PLA: ball should become Strange Ball
- [ ] PLA mon -> BDSP: ball should become Strange Ball
- [ ] PLA mon -> SV: ball should be **preserved** (no Strange Ball)

## Move/Item Sanitization

- [ ] Mon with SV-exclusive move -> SwSh: move should be **cleared**
- [ ] Mon with item above SwSh max -> SwSh: item should be **cleared**
- [ ] Mon with PLA move -> SV: move should be **cleared** if dummied

## Universal Bank

- [ ] Place SV mon into universal bank: stored as-is (no conversion)
- [ ] Place SwSh mon into universal bank: stored as-is
- [ ] Pick from universal bank, place into game bank: conversion should trigger
- [ ] Origin tag displayed correctly in universal bank slots
- [ ] Mixed origins in same universal box: each shows correct tag

## Edge Cases

- [ ] Place into same game type: no conversion, data unchanged
- [ ] Place into paired game (Sw<->Sh, S<->V, BD<->SP, FR<->LG, GP<->GE): no conversion
- [ ] Empty slot: should not trigger conversion
- [ ] Multi-select place with mixed convertible/blocked: should stop on first blocked
- [ ] Swap: held mon converts to destination, target converts to source context
