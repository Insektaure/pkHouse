#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Species ID conversion and name lookup.
// Ported from PKHeX.Core/PKM/Util/Conversion/SpeciesConverter.cs
namespace SpeciesConverter {

    // Convert Gen9 internal species ID to national dex ID.
    uint16_t getNational9(uint16_t raw);

    // Convert national dex ID to Gen9 internal species ID.
    uint16_t getInternal9(uint16_t species);

} // namespace SpeciesConverter

// Species name lookup (loaded from romfs text file).
namespace SpeciesName {

    // Load species names from file (one name per line, index 0 = "Egg").
    void load(const std::string& path);

    // Get species name by national dex ID.
    const std::string& get(uint16_t nationalId);

} // namespace SpeciesName

// Move name lookup (loaded from romfs text file).
namespace MoveName {
    void load(const std::string& path);
    const std::string& get(uint16_t moveId);
} // namespace MoveName

// Nature name lookup (loaded from romfs text file).
namespace NatureName {
    void load(const std::string& path);
    const std::string& get(uint8_t natureId);
} // namespace NatureName

// Ability name lookup (loaded from romfs text file).
namespace AbilityName {
    void load(const std::string& path);
    const std::string& get(uint16_t abilityId);
} // namespace AbilityName
