// handler_update.cpp - adapt a Pokemon's handling-trainer (HT) block when it
// is placed into a save file.
//
// Ported from PKHeX.Core: BelongsTo/TradeOT/TradeHT in PK8.cs, PB8.cs, PA8.cs,
// PK9.cs, PA9.cs, PB7.cs and the shared HT writer PKH.UpdateHandler
// (PKM/HOME/PKH.cs). Each save format's SetPKM calls UpdateHandler on every
// Pokemon written into the save; SaveFile::setBoxSlot does the same here.

#include "handler_update.h"
#include "pokemon.h"
#include "wondercard.h" // TrainerInfo
#include "personal_za.h"
#include "personal_sv.h"
#include "personal_swsh.h"
#include "personal_bdsp.h"
#include "personal_la.h"
#include <algorithm>
#include <cstring>
#include <ctime>

namespace {

constexpr int TRASH_BYTES    = 26; // trainer name region: 12 chars + terminator, UTF-16LE
constexpr int MAX_NAME_CHARS = 12;

std::u16string readName(const uint8_t* p) {
    std::u16string s;
    for (int i = 0; i < MAX_NAME_CHARS + 1; i++) {
        uint16_t ch;
        std::memcpy(&ch, p + i * 2, 2);
        if (ch == 0) break;
        s += static_cast<char16_t>(ch);
    }
    return s;
}

void writeName(uint8_t* p, const std::u16string& name) {
    std::memset(p, 0, TRASH_BYTES);
    int n = std::min<int>(static_cast<int>(name.size()), MAX_NAME_CHARS);
    for (int i = 0; i < n; i++) {
        uint16_t ch = name[i];
        std::memcpy(p + i * 2, &ch, 2);
    }
}

struct NowDate { uint8_t year; uint8_t month; uint8_t day; uint8_t hour; uint8_t min; uint8_t sec; };

NowDate nowDate() {
    std::time_t t = std::time(nullptr);
    std::tm* tm = std::localtime(&t);
    return { static_cast<uint8_t>(tm->tm_year - 100), // stored as year - 2000
             static_cast<uint8_t>(tm->tm_mon + 1),
             static_cast<uint8_t>(tm->tm_mday),
             static_cast<uint8_t>(tm->tm_hour),
             static_cast<uint8_t>(tm->tm_min),
             static_cast<uint8_t>(tm->tm_sec) };
}

uint8_t baseFriendship(const Pokemon& pk) {
    uint16_t nat = pk.species();
    uint8_t form = pk.form();
    GameType g = pk.gameType_;
    if (g == GameType::ZA) return PersonalZA::getBaseFriendship(nat, form);
    if (isSV(g))           return PersonalSV::getBaseFriendship(nat, form);
    if (isSwSh(g))         return PersonalSWSH::getBaseFriendship(nat, form);
    if (isBDSP(g))         return PersonalBDSP::getBaseFriendship(nat, form);
    if (g == GameType::LA) return PersonalLA::getBaseFriendship(nat, form);
    return 50;
}

uint32_t id32Of(const Pokemon& pk) {
    return (static_cast<uint32_t>(pk.sid()) << 16) | pk.tid();
}

bool otNameEquals(const Pokemon& pk, const TrainerInfo& tr) {
    return readName(pk.data.data() + pk.ofs().otName) == tr.otName;
}

// Per-format offsets for the modern HT block (PK8/PB8/PA8/PK9/PA9)
struct HandlerOfs {
    int htTrash;        // HT name, UTF-16LE, 26 bytes
    int htGender;
    int htLanguage;
    int currentHandler;
    int htFriendship;
    int htMemory;       // 5 bytes: intensity, memory, feeling, variable (u16)
    int otGenderByte;   // OT gender in bit 7
    int versionByte;
    bool checkLanguage; // BelongsTo also compares language (PK9/PA9 only)
};

const HandlerOfs& handlerOfsFor(GameType g) {
    static constexpr HandlerOfs PA9 = {0xA8, 0xC2, 0xC3, 0xC4, 0xC8, 0xC9, 0x125, 0xCE, true};
    static constexpr HandlerOfs PK8 = {0xA8, 0xC2, 0xC3, 0xC4, 0xC8, 0xC9, 0x125, 0xDE, false};
    static constexpr HandlerOfs PA8 = {0xB8, 0xD2, 0xD3, 0xD4, 0xD8, 0xD9, 0x13D, 0xEE, false};
    if (g == GameType::LA) return PA8;
    if (isSV(g) || g == GameType::ZA) return PA9;
    return PK8; // SwSh, BDSP
}

bool belongsTo(const Pokemon& pk, const TrainerInfo& tr, const HandlerOfs& o, bool skipVersion) {
    if (!skipVersion && tr.gameVersion != pk.data[o.versionByte]) return false;
    if (tr.id32 != id32Of(pk)) return false;
    if (tr.gender != (pk.data[o.otGenderByte] >> 7)) return false;
    if (o.checkLanguage && tr.language != pk.language()) return false;
    return otNameEquals(pk, tr);
}

// PKH.UpdateHandler: register the save's trainer as the handling trainer
void tradeHT(Pokemon& pk, const TrainerInfo& tr, const HandlerOfs& o) {
    pk.data[o.currentHandler] = 1;

    // Already the registered handler? Keep earned HT friendship/memories.
    if (pk.data[o.htGender] == tr.gender &&
        readName(pk.data.data() + o.htTrash) == tr.otName)
        return;

    std::memset(pk.data.data() + o.htMemory, 0, 5); // clear HT memories
    writeName(pk.data.data() + o.htTrash, tr.otName);
    pk.data[o.htLanguage] = tr.language;
    pk.data[o.htGender] = tr.gender;
    pk.data[o.htFriendship] = baseFriendship(pk);
    // Memories are deferred to the game; it sets them on its own.
}

// Eggs that left their OT get link-trade met data (G8PKM/PK9 layout:
// MetYear/Month/Day at 0x11C-0x11E, MetLocation at 0x122)
void setLinkTradeEgg(Pokemon& pk, uint16_t location) {
    NowDate d = nowDate();
    pk.data[0x11C] = d.year;
    pk.data[0x11D] = d.month;
    pk.data[0x11E] = d.day;
    pk.writeU16(0x122, location);
}

void updateModern(Pokemon& pk, const TrainerInfo& tr) {
    const HandlerOfs& o = handlerOfsFor(pk.gameType_);
    GameType g = pk.gameType_;

    if (pk.isEgg()) {
        if (isSwSh(g)) {
            // PK8: traded eggs only get link-trade met data, no HT changes
            constexpr uint16_t LINK_TRADE_6 = 30002;
            if (pk.readU16(0x122) != LINK_TRADE_6 && !belongsTo(pk, tr, o, false))
                setLinkTradeEgg(pk, LINK_TRADE_6);
            return;
        }
        if (isBDSP(g)) {
            // PB8: link-trade met data, then fall through — BD/SP also
            // updates the HT details & handler for eggs
            constexpr uint16_t LINK_TRADE_6_NPC = 30001;
            if (pk.readU16(0x122) != LINK_TRADE_6_NPC && !belongsTo(pk, tr, o, false))
                setLinkTradeEgg(pk, LINK_TRADE_6_NPC);
        } else if (isSV(g)) {
            // PK9: eggs match on trainer details ignoring version
            bool belongs = belongsTo(pk, tr, o, true);
            if (pk.readU16(0x120) == 60005 && belongs)
                return; // Jacq gift egg, don't change
            if (belongs) {
                // Back with its OT: clear traded-egg state. (PKHeX also
                // regenerates nickname/OT strings; unchanged here since the
                // trainer matches.)
                std::memset(pk.data.data() + o.htTrash, 0, TRASH_BYTES);
                pk.data[o.htGender] = 0;
                pk.data[o.htLanguage] = 0;
                pk.data[0x11C] = pk.data[0x11D] = pk.data[0x11E] = 0; // met date
                pk.writeU16(0x122, 0);                                // met location
                pk.data[o.currentHandler] = 0;
            } else {
                writeName(pk.data.data() + o.htTrash, tr.otName);
                pk.data[o.htGender] = tr.gender;
                pk.data[o.htLanguage] = tr.language;
                setLinkTradeEgg(pk, 30002);
                pk.data[o.currentHandler] = 1;
            }
            return;
        }
        // ZA/LA have no eggs; PKHeX applies the regular path
    }

    if (belongsTo(pk, tr, o, false))
        pk.data[o.currentHandler] = 0; // TradeOT
    else
        tradeHT(pk, tr, o);
}

// PB7 (LGPE) uses the Gen6-style layout and copies friendship instead of
// resetting it (friendship feeds the CP formula)
void updateLGPE(Pokemon& pk, const TrainerInfo& tr) {
    constexpr int HT_TRASH   = 0x78;
    constexpr int HT_GENDER  = 0x92;
    constexpr int HANDLER    = 0x93;
    constexpr int HT_FRIEND  = 0xA2;
    constexpr int OT_FRIEND  = 0xCA;
    constexpr int RECV_DATE  = 0xCB; // year/month/day
    constexpr int RECV_TIME  = 0xCE; // hour/min/sec
    constexpr int MET_DATE   = 0xD4; // year/month/day
    constexpr int OT_GENDER  = 0xDD; // bit 7
    constexpr int VERSION    = 0xDF;
    constexpr uint8_t VERSION_GO = 34;

    bool untraded = pk.data[HT_TRASH] == 0 && pk.data[HT_TRASH + 1] == 0;

    // BelongsTo (no language check; untraded GO Park captures stay with their OT)
    bool belongs = tr.gameVersion == pk.data[VERSION] ||
                   (pk.data[VERSION] == VERSION_GO && untraded);
    if (belongs && tr.id32 != id32Of(pk)) belongs = false;
    if (belongs && tr.gender != (pk.data[OT_GENDER] >> 7)) belongs = false;
    if (belongs && !otNameEquals(pk, tr)) belongs = false;

    NowDate d = nowDate();
    if (belongs) {
        // TradeOT: only touch the received date when the handler flips back
        if (pk.data[HANDLER] != 0) {
            pk.data[HANDLER] = 0;
            if (untraded)
                std::memcpy(pk.data.data() + RECV_DATE, pk.data.data() + MET_DATE, 3);
            else {
                pk.data[RECV_DATE]     = d.year;
                pk.data[RECV_DATE + 1] = d.month;
                pk.data[RECV_DATE + 2] = d.day;
            }
            pk.data[RECV_TIME]     = d.hour;
            pk.data[RECV_TIME + 1] = d.min;
            pk.data[RECV_TIME + 2] = d.sec;
        }
        return;
    }

    // TradeHT
    if (readName(pk.data.data() + HT_TRASH) != tr.otName) {
        // Copy current friendship instead of resetting (don't alter CP)
        uint8_t friendship = pk.data[HANDLER] == 0 ? pk.data[OT_FRIEND] : pk.data[HT_FRIEND];
        writeName(pk.data.data() + HT_TRASH, tr.otName);
        pk.data[HT_FRIEND]     = friendship;
        pk.data[RECV_DATE]     = d.year;
        pk.data[RECV_DATE + 1] = d.month;
        pk.data[RECV_DATE + 2] = d.day;
        pk.data[RECV_TIME]     = d.hour;
        pk.data[RECV_TIME + 1] = d.min;
        pk.data[RECV_TIME + 2] = d.sec;
    }
    pk.data[HANDLER]   = 1;
    pk.data[HT_GENDER] = tr.gender;
}

} // namespace

void updatePokemonHandler(Pokemon& pkm, const TrainerInfo& tr) {
    GameType g = pkm.gameType_;
    if (isFRLG(g))
        return; // Gen3 has no handling-trainer data
    if (isLGPE(g))
        updateLGPE(pkm, tr);
    else
        updateModern(pkm, tr);
}
