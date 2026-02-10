#include "sc_block.h"
#include <cstring>

static inline uint32_t readU32LE(const uint8_t* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

static inline void writeU32LE(uint8_t* p, uint32_t v) {
    std::memcpy(p, &v, 4);
}

SCBlock SCBlock::readFromOffset(const uint8_t* buf, size_t bufLen, size_t& offset) {
    SCBlock block{};

    // Read key
    block.key = readU32LE(buf + offset);
    offset += 4;

    // Init XorShift with key
    SCXorShift32 xk(block.key);

    // Read and decrypt type
    block.type = static_cast<SCTypeCode>(buf[offset++] ^ xk.next());

    switch (block.type) {
        case SCTypeCode::Bool1:
        case SCTypeCode::Bool2:
        case SCTypeCode::Bool3:
            // No data payload
            return block;

        case SCTypeCode::Object: {
            // Read encrypted length
            int32_t numBytes = static_cast<int32_t>(readU32LE(buf + offset) ^ static_cast<uint32_t>(xk.next32()));
            offset += 4;
            block.data.resize(numBytes);
            for (int32_t i = 0; i < numBytes; i++)
                block.data[i] = buf[offset + i] ^ xk.next();
            offset += numBytes;
            return block;
        }

        case SCTypeCode::Array: {
            // Read encrypted entry count
            int32_t numEntries = static_cast<int32_t>(readU32LE(buf + offset) ^ static_cast<uint32_t>(xk.next32()));
            offset += 4;
            // Read encrypted sub-type
            block.subType = static_cast<SCTypeCode>(buf[offset++] ^ xk.next());
            int elemSize = getTypeSize(block.subType);
            int32_t numBytes = numEntries * elemSize;
            block.data.resize(numBytes);
            for (int32_t i = 0; i < numBytes; i++)
                block.data[i] = buf[offset + i] ^ xk.next();
            offset += numBytes;
            return block;
        }

        default: {
            // Single primitive value
            int numBytes = getTypeSize(block.type);
            block.data.resize(numBytes);
            for (int i = 0; i < numBytes; i++)
                block.data[i] = buf[offset + i] ^ xk.next();
            offset += numBytes;
            return block;
        }
    }
}

size_t SCBlock::encodedSize() const {
    size_t size = 4 + 1; // key + type byte
    switch (type) {
        case SCTypeCode::Bool1:
        case SCTypeCode::Bool2:
        case SCTypeCode::Bool3:
            break;
        case SCTypeCode::Object:
            size += 4 + data.size(); // length + data
            break;
        case SCTypeCode::Array:
            size += 4 + 1 + data.size(); // count + subtype + data
            break;
        default:
            size += data.size();
            break;
    }
    return size;
}

size_t SCBlock::writeBlock(uint8_t* out) const {
    size_t pos = 0;

    // Write key
    writeU32LE(out + pos, key);
    pos += 4;

    // Encrypt with XorShift
    SCXorShift32 xk(key);

    // Write encrypted type
    out[pos++] = static_cast<uint8_t>(type) ^ xk.next();

    if (type == SCTypeCode::Object) {
        // Write encrypted length
        uint32_t len = static_cast<uint32_t>(data.size());
        writeU32LE(out + pos, len ^ static_cast<uint32_t>(xk.next32()));
        pos += 4;
    } else if (type == SCTypeCode::Array) {
        // Write encrypted entry count
        int elemSize = getTypeSize(subType);
        uint32_t entries = elemSize > 0 ? static_cast<uint32_t>(data.size() / elemSize) : 0;
        writeU32LE(out + pos, entries ^ static_cast<uint32_t>(xk.next32()));
        pos += 4;
        // Write encrypted sub-type
        out[pos++] = static_cast<uint8_t>(subType) ^ xk.next();
    }

    // Write encrypted data bytes
    for (size_t i = 0; i < data.size(); i++)
        out[pos++] = data[i] ^ xk.next();

    return pos;
}
