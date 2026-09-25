#pragma once
#include <string>
#include <cstddef>

inline std::string formatSize(size_t bytes) {
    if (bytes >= 1024 * 1024)
        return std::to_string(bytes / (1024 * 1024)) + " MB";
    return std::to_string(bytes / 1024) + " KB";
}

// Upper case for the small caps labels, UTF-8 aware: ASCII, the accented
// Latin letters (é -> É, œ -> Œ) and Cyrillic (а -> А), which is every script
// the shared font draws in a language that has cases. Anything else - CJK,
// digits, punctuation - is copied as it is.
inline std::string toUpperUtf8(const std::string& in) {
    auto upper = [](char32_t c) -> char32_t {
        if (c >= U'a' && c <= U'z') return c - 0x20;
        if (c >= 0x00E0 && c <= 0x00FE && c != 0x00F7) return c - 0x20;   // à..þ, not ÷
        if (c == 0x00FF) return 0x0178;                                    // ÿ
        // Latin Extended-A: pairs, upper case first...
        if ((c >= 0x0100 && c <= 0x0137) || (c >= 0x014A && c <= 0x0177))
            return (c & 1) ? c - 1 : c;
        // ...except where the pairs start on an odd code point.
        if ((c >= 0x0139 && c <= 0x0148) || (c >= 0x0179 && c <= 0x017E))
            return (c & 1) ? c : c - 1;
        if (c >= 0x0430 && c <= 0x044F) return c - 0x20;                   // а..я
        if (c >= 0x0450 && c <= 0x045F) return c - 0x50;                   // ѐ..џ
        return c;
    };

    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ) {
        const unsigned char b0 = static_cast<unsigned char>(in[i]);
        // Only one- and two-byte sequences have case in the ranges above; the
        // rest are copied through untouched.
        if (b0 < 0x80) {
            out += static_cast<char>(upper(b0));
            i += 1;
        } else if ((b0 & 0xE0) == 0xC0 && i + 1 < in.size()) {
            const char32_t c = ((b0 & 0x1F) << 6) | (static_cast<unsigned char>(in[i + 1]) & 0x3F);
            const char32_t u = upper(c);
            out += static_cast<char>(0xC0 | (u >> 6));
            out += static_cast<char>(0x80 | (u & 0x3F));
            i += 2;
        } else {
            const size_t len = (b0 & 0xF0) == 0xE0 ? 3 : (b0 & 0xF8) == 0xF0 ? 4 : 1;
            out += in.substr(i, len);
            i += len;
        }
    }
    return out;
}
