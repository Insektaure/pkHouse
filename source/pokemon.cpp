#include "pokemon.h"
#include "species_converter.h"

uint16_t Pokemon::species() const {
    return SpeciesConverter::getNational9(speciesInternal());
}

std::string Pokemon::nickname() const {
    std::string result;
    // Read up to 13 UTF-16LE code units from offset 0x58
    for (int i = 0; i < 13; i++) {
        uint16_t ch = readU16(0x58 + i * 2);
        if (ch == 0)
            break;
        // Basic UTF-16 to UTF-8 conversion
        if (ch < 0x80) {
            result += static_cast<char>(ch);
        } else if (ch < 0x800) {
            result += static_cast<char>(0xC0 | (ch >> 6));
            result += static_cast<char>(0x80 | (ch & 0x3F));
        } else {
            result += static_cast<char>(0xE0 | (ch >> 12));
            result += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (ch & 0x3F));
        }
    }
    return result;
}

std::string Pokemon::displayName() const {
    if (isEmpty())
        return "";
    if (isEgg())
        return SpeciesName::get(0); // "Egg"
    if (isNicknamed())
        return nickname();
    return SpeciesName::get(species());
}

void Pokemon::loadFromEncrypted(const uint8_t* encrypted, size_t len) {
    PokeCrypto::decryptArray9(encrypted, len, data.data());
}

void Pokemon::getEncrypted(uint8_t* outBuf) const {
    PokeCrypto::encryptArray9(data.data(), PokeCrypto::SIZE_9PARTY, outBuf);
}
