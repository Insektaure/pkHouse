#pragma once

struct Pokemon;
struct TrainerInfo;

// Adapts a Pokemon's handling-trainer (HT) block to the trainer of the save
// file it is being placed into, mirroring PKHeX's SaveFile.SetPKM ->
// pkm.UpdateHandler(sav) behavior:
//  - If the Pokemon belongs to the save's trainer (version/ID32/gender/OT name
//    match), it is marked as handled by its OT (CurrentHandler = 0).
//  - Otherwise the save's trainer is written as the handling trainer
//    (CurrentHandler = 1, HT name/gender/language, friendship reset to the
//    species' base value, HT memories cleared).
// No-op for FRLG (Gen3 has no handling-trainer data).
void updatePokemonHandler(Pokemon& pkm, const TrainerInfo& tr);
