#include "pokemon.h"
#include "species_converter.h"

uint16_t Pokemon::species() const {
    if (isSwSh(gameType_) || isBDSP(gameType_) || gameType_ == GameType::LA)
        return speciesInternal(); // PK8/PB8/PA8 stores national dex ID directly
    return SpeciesConverter::getNational9(speciesInternal());
}

// Helper: read UTF-16LE string from given offset
static std::string readUtf16String(const uint8_t* base, int offset, int maxChars) {
    std::string result;
    for (int i = 0; i < maxChars; i++) {
        uint16_t ch;
        std::memcpy(&ch, base + offset + i * 2, 2);
        if (ch == 0)
            break;
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

std::string Pokemon::nickname() const {
    int ofs = (gameType_ == GameType::LA) ? 0x60 : 0x58;
    return readUtf16String(data.data(), ofs, 13);
}

std::string Pokemon::otName() const {
    int ofs = (gameType_ == GameType::LA) ? 0x110 : 0xF8;
    return readUtf16String(data.data(), ofs, 13);
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
    if (gameType_ == GameType::LA)
        PokeCrypto::decryptArray8A(encrypted, len, data.data());
    else
        PokeCrypto::decryptArray9(encrypted, len, data.data());
}

void Pokemon::refreshChecksum() {
    // Checksum = sum of all 16-bit words from byte 8 to SIZE_STORED
    int end = (gameType_ == GameType::LA) ? PokeCrypto::SIZE_8ASTORED : PokeCrypto::SIZE_9STORED;
    uint16_t chk = 0;
    for (int i = 8; i < end; i += 2)
        chk += readU16(i);
    writeU16(0x06, chk);
}

void Pokemon::getEncrypted(uint8_t* outBuf) {
    refreshChecksum();
    if (gameType_ == GameType::LA)
        PokeCrypto::encryptArray8A(data.data(), PokeCrypto::SIZE_8ASTORED, outBuf);
    else
        PokeCrypto::encryptArray9(data.data(), PokeCrypto::SIZE_9PARTY, outBuf);
}
