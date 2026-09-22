#include "card_payload.h"
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
