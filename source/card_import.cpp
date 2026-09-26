#include "card_import.h"
#include "quirc.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <dirent.h>

namespace {

bool hasPngExtension(const std::string& name) {
    if (name.size() < 5) return false;
    std::string ext = name.substr(name.size() - 4);
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext == ".png";
}

// Newer exports lead with "NNNN-FF - ": the national dex number and the form,
// fixed width. Reads it off the front of `base` when it is exactly that.
bool takeSpeciesPrefix(std::string& base, uint16_t& species, uint8_t& form) {
    // "0037-01 - "
    if (base.size() < 10) return false;
    for (int i : {0, 1, 2, 3, 5, 6})
        if (!std::isdigit(static_cast<unsigned char>(base[i]))) return false;
    if (base[4] != '-' || base.compare(7, 3, " - ") != 0) return false;
    const int dex = std::stoi(base.substr(0, 4));
    const int f   = std::stoi(base.substr(5, 2));
    if (dex > 1025) return false;
    species = static_cast<uint16_t>(dex);
    form = static_cast<uint8_t>(f);
    base.erase(0, 10);
    return true;
}

// Exported cards are named "[NNNN-FF - ]<Species> - <Tag> - [flags] - <EC>.png".
// Drop the extension, the number prefix and the checksum suffix to get
// something readable for the list; anything that does not match that shape is
// listed under its own name.
std::string labelFor(const std::string& filename, CardFile& cf) {
    std::string base = filename.substr(0, filename.size() - 4);
    cf.hasSpecies = takeSpeciesPrefix(base, cf.species, cf.form);
    const std::string sep = " - ";
    size_t cut = base.rfind(sep);
    if (cut != std::string::npos && base.size() - cut - sep.size() == 8) {
        bool allHex = true;
        for (size_t i = cut + sep.size(); i < base.size(); i++)
            if (!std::isxdigit(static_cast<unsigned char>(base[i]))) { allHex = false; break; }
        if (allHex) base.erase(cut);
    }
    return base;
}

} // anonymous namespace

std::vector<CardFile> scanCards(const std::string& basePath, GameType game) {
    std::vector<CardFile> results;

    const std::string dir = basePath + "cards/" + bankFolderNameOf(game) + "/";
    DIR* d = opendir(dir.c_str());
    if (!d) return results;

    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == ".." || !hasPngExtension(name))
            continue;
        CardFile cf;
        cf.filename = name;
        cf.path = dir + name;
        cf.label = labelFor(name, cf);
        results.push_back(std::move(cf));
    }
    closedir(d);

    std::sort(results.begin(), results.end(),
              [](const CardFile& a, const CardFile& b) { return a.filename < b.filename; });
    return results;
}

namespace {

// Runs quirc over one rectangle of an RGB24 surface.
CardPayload::Parsed decodeRegion(SDL_Surface* rgb, const SDL_Rect& region, GameType target) {
    CardPayload::Parsed out;  // defaults to NotACard
    if (region.w <= 0 || region.h <= 0)
        return out;

    struct quirc* q = quirc_new();
    if (!q) return out;
    if (quirc_resize(q, region.w, region.h) < 0) {
        quirc_destroy(q);
        return out;
    }

    // quirc works on 8-bit greyscale; convert with Rec. 601 luma weights.
    int gw = 0, gh = 0;
    uint8_t* gray = quirc_begin(q, &gw, &gh);
    for (int y = 0; y < gh; y++) {
        const uint8_t* src = static_cast<const uint8_t*>(rgb->pixels)
                           + static_cast<size_t>(region.y + y) * rgb->pitch
                           + static_cast<size_t>(region.x) * 3;
        uint8_t* dst = gray + static_cast<size_t>(y) * gw;
        for (int x = 0; x < gw; x++, src += 3)
            dst[x] = static_cast<uint8_t>((src[0] * 77 + src[1] * 151 + src[2] * 28) >> 8);
    }
    quirc_end(q);

    // A shared image may carry more than one code; keep the first that decodes
    // cleanly, and otherwise report the most specific failure we saw.
    const int count = quirc_count(q);
    for (int i = 0; i < count; i++) {
        struct quirc_code code;
        struct quirc_data data;
        quirc_extract(q, i, &code);
        if (quirc_decode(&code, &data) != QUIRC_SUCCESS)
            continue;
        if (data.payload_len <= 0)
            continue;

        CardPayload::Parsed p = CardPayload::parse(data.payload,
                                                   static_cast<size_t>(data.payload_len), target);
        if (p.result == CardPayload::Result::Ok) {
            quirc_destroy(q);
            return p;
        }
        if (out.result == CardPayload::Result::NotACard)
            out = p;
    }

    quirc_destroy(q);
    return out;
}

} // anonymous namespace

CardPayload::Parsed decodeCard(const std::string& path, GameType target, bool fastPathOnly) {
    CardPayload::Parsed out;

    SDL_Surface* img = IMG_Load(path.c_str());
    if (!img) return out;

    SDL_Surface* rgb = SDL_ConvertSurfaceFormat(img, SDL_PIXELFORMAT_RGB24, 0);
    SDL_FreeSurface(img);
    if (!rgb) return out;

    // Fast path: if the image has a card's proportions, the QR is in a known
    // place. Scanning a 340x340 crop instead of the full picture is what makes
    // decoding cheap enough to do while the cursor moves.
    const double sx = static_cast<double>(rgb->w) / CardLayout::WIDTH;
    const double sy = static_cast<double>(rgb->h) / CardLayout::HEIGHT;
    bool cardShaped = rgb->w > 0 && rgb->h > 0 && std::fabs(sx - sy) < 0.02 * sx;

    if (cardShaped) {
        constexpr int PAD = 10;  // a little slack for rounding and rescaling
        SDL_Rect r;
        r.x = static_cast<int>((CardLayout::QR_X - PAD) * sx);
        r.y = static_cast<int>((CardLayout::QR_Y - PAD) * sy);
        r.w = static_cast<int>((CardLayout::QR_SIZE + 2 * PAD) * sx);
        r.h = static_cast<int>((CardLayout::QR_SIZE + 2 * PAD) * sy);
        r.x = std::max(0, r.x);
        r.y = std::max(0, r.y);
        r.w = std::min(r.w, rgb->w - r.x);
        r.h = std::min(r.h, rgb->h - r.y);

        out = decodeRegion(rgb, r, target);
        if (out.result != CardPayload::Result::NotACard) {
            SDL_FreeSurface(rgb);
            return out;
        }
    }

    if (fastPathOnly) {
        SDL_FreeSurface(rgb);
        return out;
    }

    SDL_Rect full = {0, 0, rgb->w, rgb->h};
    out = decodeRegion(rgb, full, target);
    SDL_FreeSurface(rgb);
    return out;
}
