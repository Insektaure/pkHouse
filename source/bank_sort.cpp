#include "bank_sort.h"
#include "living_dex_data.h"
#include "species_converter.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace {

// A Pokemon out of the bank, with the slot it came from.
struct Item {
    Pokemon pkm;
    int     from;
};

int ivTotal(const Pokemon& p) {
    return p.ivHp() + p.ivAtk() + p.ivDef() + p.ivSpA() + p.ivSpD() + p.ivSpe();
}

const LivingDexData::Section* sectionsFor(GameType g, int& count) {
    using namespace LivingDexData;
    auto give = [&](const Section* s, int n) { count = n; return s; };
    if (isSV(g))           return give(SV, 3);
    if (isSwSh(g))         return give(SWSH, 3);
    if (g == GameType::ZA) return give(ZA, 2);
    if (g == GameType::LA) return give(LA, 1);
    if (isBDSP(g))         return give(BDSP, 2);
    if (isLGPE(g))         return give(LGPE, 1);
    if (isFRLG(g))         return give(FRLG, 1);
    count = 0;
    return nullptr;
}

// Writes `target` (a slot -> index into `items`, or -1 for empty) over the
// bank's slots [first, first + target.size()).
void writeBack(Bank& bank, int first, const std::vector<int>& target, const std::vector<Item>& items) {
    const int per = bank.slotsPerBox();
    for (size_t k = 0; k < target.size(); k++) {
        const int slot = first + static_cast<int>(k);
        if (target[k] >= 0) bank.setSlot(slot / per, slot % per, items[target[k]].pkm);
        else                bank.clearSlot(slot / per, slot % per);
    }
}

// Each species' place in the game's Pokedex order, and how many each section
// adds - the order both the Living Dex and the game-Pokedex sort use: a
// species' first entry (the next are its Complete Living Dex variants). The
// generator already keeps a species to one section; counting here as well
// keeps the layout within its vectors whatever the data.
void gameOrder(const LivingDexData::Section* sections, int sectionCount,
               std::unordered_map<uint16_t, int>& order, std::vector<int>& sectionSize) {
    order.clear();
    sectionSize.assign(sectionCount, 0);
    for (int s = 0; s < sectionCount; s++)
        for (int k = 0; k < sections[s].count; k++)
            if (order.emplace(sections[s].entries[k].species, static_cast<int>(order.size())).second)
                sectionSize[s]++;
}

// A Complete Living Dex slot: species, form, gender (or ANY_GENDER).
uint32_t slotKey(uint16_t species, uint8_t form, uint8_t gender) {
    return (static_cast<uint32_t>(species) << 16) | (static_cast<uint32_t>(form) << 8) | gender;
}

// What a Living Dex gives a slot to: each species; or each entry - species,
// then its variants (genders, regional forms) - with the male / female split
// only where they look different, or wherever a species can be both.
enum class Slots { Species, GenderDifferences, AllGenders };

BankSort::Result livingDex(Bank& bank, GameType game, std::vector<Item>& items, Slots mode) {
    const bool complete = mode != Slots::Species;
    BankSort::Result r;
    r.pokemon = static_cast<int>(items.size());
    int sectionCount = 0;
    const LivingDexData::Section* sections = sectionsFor(game, sectionCount);
    if (!sections) return r;
    const int per = bank.slotsPerBox(), total = bank.totalSlots();

    std::unordered_map<uint16_t, int> order;
    std::vector<int> sectionSize;
    gameOrder(sections, sectionCount, order, sectionSize);

    // The layout indexes: the species' (order), or the entries' (slotIndex);
    // `slotSize` is how many each section adds.
    int slots = static_cast<int>(order.size());
    std::vector<int> slotSize = sectionSize;
    std::unordered_map<uint32_t, int> slotIndex;
    if (complete) {
        slots = 0;
        for (int s = 0; s < sectionCount; s++) {
            slotSize[s] = 0;
            for (int k = 0; k < sections[s].count; k++) {
                const LivingDexData::Entry& e = sections[s].entries[k];
                // Genders that look alike: one slot for both (the female
                // entry lands on the male's key), unless all are split.
                const uint8_t g = (mode == Slots::AllGenders || e.visible) ? e.gender : LivingDexData::ANY_GENDER;
                if (slotIndex.emplace(slotKey(e.species, e.form, g), slots).second) {
                    slots++;
                    slotSize[s]++;
                }
            }
        }
    }
    r.species = slots;

    // The layout index a Pokemon belongs to, or -1. Complete: its form and
    // gender's slot, or its form's when the gender makes none; a form with no
    // slot of its own (a cosmetic one: Vivillon, Alcremie...) takes the
    // species' first form's.
    auto slotFor = [&](const Pokemon& p) -> int {
        if (!complete) {
            auto it = order.find(p.species());
            return it != order.end() ? it->second : -1;
        }
        const uint16_t sp = p.species();
        const uint8_t gender = p.gender();
        auto lookup = [&](uint8_t form, uint8_t g) {
            auto it = slotIndex.find(slotKey(sp, form, g));
            return it != slotIndex.end() ? it->second : -1;
        };
        auto formSlot = [&](uint8_t form) {
            const int k = gender <= 1 ? lookup(form, gender) : -1;
            return k >= 0 ? k : lookup(form, LivingDexData::ANY_GENDER);
        };
        const uint8_t form = p.form();
        int k = formSlot(form);
        if (k < 0 && form != 0 && lookup(form, 0) < 0 && lookup(form, 1) < 0)
            k = formSlot(0);
        return k;
    };

    // The slot of each layout index: each Pokedex starting a new box
    // (aligned), or one after the other (packed). `end` is where the rest go:
    // on a new box too when aligned.
    auto layout = [&](bool aligned, std::vector<int>& slotOf, int& end) {
        slotOf.assign(slots, 0);
        int pos = 0, idx = 0;
        for (int s = 0; s < sectionCount; s++) {
            if (aligned && pos % per) pos += per - pos % per;
            for (int k = 0; k < slotSize[s]; k++) slotOf[idx++] = pos++;
        }
        if (aligned && pos % per) pos += per - pos % per;
        end = pos;
    };

    // One Pokemon per layout index takes its slot - never an egg. One already
    // in its slot of the aligned layout keeps it; otherwise the first found in
    // bank order.
    std::vector<int> alignedSlot;
    int alignedEnd = 0;
    layout(true, alignedSlot, alignedEnd);
    std::vector<int> holder(slots, -1);   // layout index -> item
    std::vector<char> isHolder(items.size(), 0);
    for (int pass = 0; pass < 2; pass++) {
        for (size_t i = 0; i < items.size(); i++) {
            const Pokemon& p = items[i].pkm;
            if (p.isEgg() || isHolder[i]) continue;
            const int k = slotFor(p);
            if (k < 0 || holder[k] >= 0) continue;
            if (pass == 0 && items[i].from != alignedSlot[k]) continue;
            holder[k] = static_cast<int>(i);
            isHolder[i] = 1;
        }
    }
    r.placed = static_cast<int>(std::count_if(holder.begin(), holder.end(), [](int h) { return h >= 0; }));

    // Everything else - duplicates, other forms, species outside this
    // Pokedex, eggs - after the layout: in Pokedex order, then national order,
    // eggs last.
    std::vector<int> extras;
    for (size_t i = 0; i < items.size(); i++)
        if (!isHolder[i]) extras.push_back(static_cast<int>(i));
    auto rank = [&](const Pokemon& p) {
        auto it = order.find(p.species());
        return it != order.end() ? it->second : static_cast<int>(order.size()) + p.species();
    };
    std::stable_sort(extras.begin(), extras.end(), [&](int a, int b) {
        const Pokemon& pa = items[a].pkm;
        const Pokemon& pb = items[b].pkm;
        if (pa.isEgg() != pb.isEgg()) return !pa.isEgg();
        const int ra = rank(pa), rb = rank(pb);
        if (ra != rb) return ra < rb;
        return pa.form() < pb.form();
    });

    // Aligned if it fits, packed if only that fits, otherwise refused with
    // what it would take - before anything has moved.
    std::vector<int> slotOf = alignedSlot;
    int end = alignedEnd;
    if (end + static_cast<int>(extras.size()) > total) {
        layout(false, slotOf, end);
        if (end + static_cast<int>(extras.size()) > total) {
            r.needed = end + static_cast<int>(extras.size());
            r.capacity = total;
            return r;
        }
    }

    std::vector<int> target(total, -1);
    for (size_t k = 0; k < holder.size(); k++)
        if (holder[k] >= 0) target[slotOf[k]] = holder[k];
    for (size_t k = 0; k < extras.size(); k++)
        target[end + static_cast<int>(k)] = extras[k];
    writeBack(bank, 0, target, items);
    r.done = true;
    return r;
}

} // anonymous namespace

namespace BankSort {

Result sort(Bank& bank, GameType game, Order order, int box) {
    const int per = bank.slotsPerBox(), total = bank.totalSlots();
    const bool whole = box < 0 || isLivingDex(order);
    const int first = whole ? 0 : box * per;
    const int end = whole ? total : std::min(total, first + per);

    std::vector<Item> items;
    for (int s = first; s < end; s++)
        if (!bank.isSlotEmpty(s / per, s % per))
            items.push_back({bank.getSlot(s / per, s % per), s});

    if (isLivingDex(order))
        return livingDex(bank, game, items,
                         order == Order::LivingDex         ? Slots::Species
                         : order == Order::CompleteLivingDex ? Slots::GenderDifferences
                                                             : Slots::AllGenders);

    Result r;
    r.pokemon = static_cast<int>(items.size());
    if (items.empty()) return r;

    // The game's Pokedex rank of each Pokemon, for GameDex: species outside
    // the game's Pokedex after it, in national order.
    std::vector<int> rank;
    if (order == Order::GameDex) {
        int sectionCount = 0;
        const LivingDexData::Section* sections = sectionsFor(game, sectionCount);
        std::unordered_map<uint16_t, int> place;
        std::vector<int> sectionSize;
        if (sections) gameOrder(sections, sectionCount, place, sectionSize);
        const int outside = static_cast<int>(place.size());
        rank.reserve(items.size());
        for (const Item& it : items) {
            auto f = place.find(it.pkm.species());
            rank.push_back(f != place.end() ? f->second : outside + it.pkm.species());
        }
    }

    // Species names looked up once, not on every comparison.
    std::vector<std::string> names;
    if (order == Order::Name) {
        names.reserve(items.size());
        for (const Item& it : items) names.push_back(SpeciesName::get(it.pkm.species()));
    }
    std::vector<int> idx(items.size());
    for (size_t i = 0; i < idx.size(); i++) idx[i] = static_cast<int>(i);
    if (order != Order::Compact) {
        std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) {
            const Pokemon& pa = items[a].pkm;
            const Pokemon& pb = items[b].pkm;
            if (pa.isEgg() != pb.isEgg()) return !pa.isEgg();
            switch (order) {
                case Order::GameDex:
                    if (rank[a] != rank[b]) return rank[a] < rank[b];
                    break;
                case Order::Name:
                    if (names[a] != names[b]) return names[a] < names[b];
                    break;
                case Order::ShinyFirst:
                    if (pa.isShiny() != pb.isShiny()) return pa.isShiny();
                    break;
                case Order::Level:
                    if (pa.level() != pb.level()) return pa.level() > pb.level();
                    break;
                case Order::Ivs: {
                    const int ia = ivTotal(pa), ib = ivTotal(pb);
                    if (ia != ib) return ia > ib;
                    break;
                }
                default: break;
            }
            if (pa.species() != pb.species()) return pa.species() < pb.species();
            return pa.form() < pb.form();
        });
    }

    std::vector<int> target(end - first, -1);
    for (size_t k = 0; k < idx.size(); k++) target[k] = idx[k];
    writeBack(bank, first, target, items);
    r.done = true;
    return r;
}

} // namespace BankSort
