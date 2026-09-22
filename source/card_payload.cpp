#include "card_payload.h"
#include "species_types.h"
#include <cstring>

uint16_t CardPayload::crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (int bit = 0; bit < 8; bit++)
            crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                                 : static_cast<uint16_t>(crc << 1);
    }
    return crc;
}

std::vector<uint8_t> CardPayload::build(const Pokemon& pkm, GameType game) {
    const int bodyLen = pkPartySize(game);
    if (bodyLen <= 0 || bodyLen > PokeCrypto::MAX_PARTY_SIZE)
        return {};

    std::vector<uint8_t> out(HEADER_SIZE + bodyLen);
    std::memcpy(out.data(), MAGIC, 4);
    out[4] = VERSION;
    out[5] = static_cast<uint8_t>(game);
    out[6] = static_cast<uint8_t>(bodyLen & 0xFF);
    out[7] = static_cast<uint8_t>((bodyLen >> 8) & 0xFF);

    std::memcpy(out.data() + HEADER_SIZE, pkm.data.data(), bodyLen);

    uint16_t crc = crc16(out.data() + HEADER_SIZE, static_cast<size_t>(bodyLen));
    out[8] = static_cast<uint8_t>(crc & 0xFF);
    out[9] = static_cast<uint8_t>((crc >> 8) & 0xFF);
    return out;
}

// --- Validation --------------------------------------------------------------

namespace {

// Does this species+form actually exist in the target game? Only four of the
// personal tables carry a presence flag, so the others pass by default.
bool speciesPresent(GameType game, uint16_t natdex, uint8_t form) {
    if (isSV(game))
        return PersonalSV::IS_PRESENT[PersonalSV::getEntryIndex(natdex, form)] != 0;
    if (game == GameType::ZA)
        return PersonalZA::IS_PRESENT[PersonalZA::getEntryIndex(natdex, form)] != 0;
    if (isSwSh(game))
        return PersonalSWSH::IS_PRESENT[PersonalSWSH::getEntryIndex(natdex, form)] != 0;
    if (game == GameType::LA)
        return PersonalLA::IS_PRESENT[PersonalLA::getEntryIndex(natdex, form)] != 0;
    return true;
}

// Cheap range checks. None of these prove a blob is the format it claims, but
// together they catch data that has been reinterpreted through the wrong
// offsets, which is what a mislabelled card would look like.
bool plausible(const Pokemon& pkm) {
    if (pkm.nature() > 24) return false;
    if (pkm.ball() > 37) return false;

    const uint8_t level = pkm.level();
    if (level < 1 || level > 100) return false;

    const int evTotal = pkm.evHp() + pkm.evAtk() + pkm.evDef()
                      + pkm.evSpe() + pkm.evSpA() + pkm.evSpD();
    if (evTotal > 510) return false;

    // Language 6 was never used; 0 means the field was read at the wrong offset.
    const uint8_t lang = pkm.language();
    if (!isFRLG(pkm.gameType_) && (lang == 0 || lang == 6 || lang > 10))
        return false;

    // A met date is either unset or a real date.
    if (pkm.metYear() != 0) {
        if (pkm.metMonth() < 1 || pkm.metMonth() > 12) return false;
        if (pkm.metDay() < 1 || pkm.metDay() > 31) return false;
    }
    return true;
}

} // anonymous namespace

CardPayload::Parsed CardPayload::parse(const uint8_t* data, size_t len, GameType target) {
    Parsed out;

    if (len < static_cast<size_t>(HEADER_SIZE) || std::memcmp(data, MAGIC, 4) != 0) {
        out.result = Result::NotACard;
        return out;
    }
    if (data[4] != VERSION) {
        out.result = Result::UnsupportedVersion;
        return out;
    }

    const size_t bodyLen = static_cast<size_t>(data[6]) | (static_cast<size_t>(data[7]) << 8);
    if (bodyLen == 0 || len < static_cast<size_t>(HEADER_SIZE) + bodyLen) {
        out.result = Result::Truncated;
        return out;
    }

    const uint16_t wantCrc = static_cast<uint16_t>(data[8] | (data[9] << 8));
    if (crc16(data + HEADER_SIZE, bodyLen) != wantCrc) {
        out.result = Result::BadChecksum;
        return out;
    }

    // Everything past here is CRC-covered, so the game byte can be trusted.
    if (data[5] >= GAME_TYPE_COUNT) {
        out.result = Result::UnknownGame;
        return out;
    }
    out.game = static_cast<GameType>(data[5]);
    out.gameKnown = true;

    if (bodyLen != static_cast<size_t>(pkPartySize(out.game))) {
        out.result = Result::BadSize;
        return out;
    }

    // The hard gate. Family, not exact game: a Sword card belongs in a Shield
    // bank. Placing a blob of the wrong format would make SaveFile::setBoxSlot
    // reinterpret it through the target's offsets and write a corrupt Pokemon.
    if (std::strcmp(bankFolderNameOf(out.game), bankFolderNameOf(target)) != 0) {
        out.result = Result::WrongGame;
        return out;
    }

    out.pkm = Pokemon{};
    out.pkm.gameType_ = out.game;
    std::memcpy(out.pkm.data.data(), data + HEADER_SIZE, bodyLen);

    if (out.pkm.isEmpty() || out.pkm.species() == 0 || out.pkm.species() > 1025) {
        out.result = Result::Implausible;
        return out;
    }
    if (out.pkm.computeChecksum() != out.pkm.storedChecksum()) {
        out.result = Result::BadPokemonChecksum;
        return out;
    }
    if (!speciesPresent(out.game, out.pkm.species(), out.pkm.form())) {
        out.result = Result::NotPresent;
        return out;
    }
    if (!plausible(out.pkm)) {
        out.result = Result::Implausible;
        return out;
    }

    out.result = Result::Ok;
    return out;
}
