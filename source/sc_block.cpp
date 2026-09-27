#include "sc_block.h"
#include "binary_io.h"

// Every length comes from the file, so each one is checked against what is
// left of the buffer before it is used: a damaged save (a backup with a bad
// byte, a corrupt file) then fails to load instead of reading past the end or
// asking for gigabytes. The decryption stream is consumed in the same order
// as before, so a valid save parses exactly as it always did.
bool SCBlock::readFromOffset(const uint8_t* buf, size_t bufLen, size_t& offset, SCBlock& block) {
    block = SCBlock{};
    auto room = [&](uint64_t n) { return offset <= bufLen && n <= bufLen - offset; };

    // Key and type
    if (!room(5)) return false;
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
            return true;

        case SCTypeCode::Object: {
            // Read encrypted length
            if (!room(4)) return false;
            int32_t numBytes = static_cast<int32_t>(readU32LE(buf + offset) ^ static_cast<uint32_t>(xk.next32()));
            offset += 4;
            if (numBytes < 0 || !room(static_cast<uint64_t>(numBytes))) return false;
            block.data.resize(numBytes);
            for (int32_t i = 0; i < numBytes; i++)
                block.data[i] = buf[offset + i] ^ xk.next();
            offset += numBytes;
            return true;
        }

        case SCTypeCode::Array: {
            // Read encrypted entry count, then the encrypted sub-type
            if (!room(5)) return false;
            int32_t numEntries = static_cast<int32_t>(readU32LE(buf + offset) ^ static_cast<uint32_t>(xk.next32()));
            offset += 4;
            block.subType = static_cast<SCTypeCode>(buf[offset++] ^ xk.next());
            // In 64 bits: the product cannot overflow. Negative is refused,
            // as the old resize(negative) could not survive it; a negative
            // count of a zero-sized type is 0 bytes, as it always was.
            const int64_t numBytes = static_cast<int64_t>(numEntries) * getTypeSize(block.subType);
            if (numBytes < 0 || !room(static_cast<uint64_t>(numBytes))) return false;
            block.data.resize(static_cast<size_t>(numBytes));
            for (int64_t i = 0; i < numBytes; i++)
                block.data[i] = buf[offset + i] ^ xk.next();
            offset += static_cast<size_t>(numBytes);
            return true;
        }

        default: {
            // Single primitive value
            int numBytes = getTypeSize(block.type);
            if (!room(static_cast<uint64_t>(numBytes))) return false;
            block.data.resize(numBytes);
            for (int i = 0; i < numBytes; i++)
                block.data[i] = buf[offset + i] ^ xk.next();
            offset += numBytes;
            return true;
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
