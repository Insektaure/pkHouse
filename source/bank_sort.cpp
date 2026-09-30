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

BankSort::Result livingDex(Bank& bank, GameType game, std::vector<Item>& items) {
    BankSort::Result r;
    r.pokemon = static_cast<int>(items.size());
    int sectionCount = 0;
    const LivingDexData::Section* sections = sectionsFor(game, sectionCount);
    if (!sections) return r;
    const int per = bank.slotsPerBox(), total = bank.totalSlots();

    // Each species' place in the layout order, and how many each section
    // adds. The generator already keeps a species to one section; counting
    // here as well keeps the layout within its vectors whatever the data.
    std::unordered_map<uint16_t, int> order;
    std::vector<int> sectionSize(sectionCount, 0);
    for (int s = 0; s < sectionCount; s++)
        for (int k = 0; k < sections[s].count; k++)
            if (order.emplace(sections[s].species[k], static_cast<int>(order.size())).second)
                sectionSize[s]++;
    r.species = static_cast<int>(order.size());

    // The slot of each layout index: each Pokedex starting a new box
    // (aligned), or one after the other (packed). `end` is where the rest go:
    // on a new box too when aligned.
    auto layout = [&](bool aligned, std::vector<int>& slotOf, int& end) {
        slotOf.assign(order.size(), 0);
        int pos = 0, idx = 0;
        for (int s = 0; s < sectionCount; s++) {
            if (aligned && pos % per) pos += per - pos % per;
            for (int k = 0; k < sectionSize[s]; k++) slotOf[idx++] = pos++;
        }
        if (aligned && pos % per) pos += per - pos % per;
        end = pos;
    };

    // One Pokemon per species takes its slot - never an egg. One already in
    // its slot of the aligned layout keeps it; otherwise the first found in
    // bank order.
    std::vector<int> alignedSlot;
    int alignedEnd = 0;
    layout(true, alignedSlot, alignedEnd);
    std::vector<int> holder(order.size(), -1);   // layout index -> item
    std::vector<char> isHolder(items.size(), 0);
    for (int pass = 0; pass < 2; pass++) {
        for (size_t i = 0; i < items.size(); i++) {
            const Pokemon& p = items[i].pkm;
            if (p.isEgg() || isHolder[i]) continue;
            auto it = order.find(p.species());
            if (it == order.end() || holder[it->second] >= 0) continue;
            if (pass == 0 && items[i].from != alignedSlot[it->second]) continue;
            holder[it->second] = static_cast<int>(i);
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
    const bool whole = box < 0 || order == Order::LivingDex;
    const int first = whole ? 0 : box * per;
    const int end = whole ? total : std::min(total, first + per);

    std::vector<Item> items;
    for (int s = first; s < end; s++)
        if (!bank.isSlotEmpty(s / per, s % per))
            items.push_back({bank.getSlot(s / per, s % per), s});

    if (order == Order::LivingDex)
        return livingDex(bank, game, items);

    Result r;
    r.pokemon = static_cast<int>(items.size());
    if (items.empty()) return r;

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
