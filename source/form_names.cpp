#include "form_names.h"

static const uint16_t ALOLAN[] = {19,20,26,27,28,37,38,50,51,52,53,74,75,76,88,89,103,105};
static const uint16_t GALARIAN[] = {77,78,79,80,83,110,122,144,145,146,199,222,263,264,554,555,562,618};
static const uint16_t HISUIAN[] = {58,59,100,101,157,211,215,503,549,570,571,628,705,706,713,724};
static const uint16_t PALDEAN[] = {128,194};

static bool inList(const uint16_t* arr, int len, uint16_t s) {
    for (int i = 0; i < len; i++) if (arr[i] == s) return true;
    return false;
}

const char* getFormName(uint16_t species, uint8_t form) {
    if (form == 0) return nullptr;

    // Regional forms
    if (form == 1) {
        if (inList(ALOLAN, sizeof(ALOLAN)/sizeof(*ALOLAN), species))     return "Alolan";
        if (inList(GALARIAN, sizeof(GALARIAN)/sizeof(*GALARIAN), species)) return "Galarian";
        if (inList(HISUIAN, sizeof(HISUIAN)/sizeof(*HISUIAN), species))   return "Hisuian";
        if (inList(PALDEAN, sizeof(PALDEAN)/sizeof(*PALDEAN), species))   return "Paldean";
    }
    if (form == 2 && species == 52) return "Galarian"; // Meowth

    // Species-specific forms
    switch (species) {
    // Pikachu (cap forms)
    case 25: { const char* f[] = {nullptr,"Original","Hoenn","Sinnoh","Unova","Kalos","Alola","Partner","Starter","World"}; if (form < 10) return f[form]; break; }
    // Tauros (Paldean)
    case 128: { const char* f[] = {nullptr,"Paldean","Paldean-Blaze","Paldean-Aqua"}; if (form < 4) return f[form]; break; }
    // Pichu
    case 172: if (form == 1) return "Spiky"; break;
    // Unown (A=0 .. Z=25, !=26, ?=27)
    case 201: { static char letter[2] = {}; if (form <= 25) { letter[0] = 'A' + form; return letter; } if (form == 26) return "Excl"; if (form == 27) return "Qmark"; break; }
    // Castform
    case 351: { const char* f[] = {nullptr,"Sunny","Rainy","Snowy"}; if (form < 4) return f[form]; break; }
    // Deoxys
    case 386: { const char* f[] = {nullptr,"Attack","Defense","Speed"}; if (form < 4) return f[form]; break; }
    // Burmy / Wormadam
    case 412: case 413: { const char* f[] = {nullptr,"Sandy","Trash"}; if (form < 3) return f[form]; break; }
    // Cherrim
    case 421: if (form == 1) return "Sunshine"; break;
    // Shellos / Gastrodon
    case 422: case 423: if (form == 1) return "East"; break;
    // Rotom
    case 479: { const char* f[] = {nullptr,"Heat","Wash","Frost","Fan","Mow"}; if (form < 6) return f[form]; break; }
    // Dialga / Palkia
    case 483: case 484: if (form == 1) return "Origin"; break;
    // Giratina
    case 487: if (form == 1) return "Origin"; break;
    // Shaymin
    case 492: if (form == 1) return "Sky"; break;
    // Arceus / Silvally (type forms)
    case 493: case 773: { const char* f[] = {nullptr,"Fighting","Flying","Poison","Ground","Rock","Bug","Ghost","Steel","Fire","Water","Electric","Grass","Ice","Psychic","Dragon","Dark","Fairy"}; if (form < 18) return f[form]; break; }
    // Basculin
    case 550: { const char* f[] = {nullptr,"Blue-Striped","White-Striped"}; if (form < 3) return f[form]; break; }
    // Darmanitan
    case 555: { const char* f[] = {nullptr,"Zen","Galarian","Galarian-Zen"}; if (form < 4) return f[form]; break; }
    // Deerling / Sawsbuck
    case 585: case 586: { const char* f[] = {nullptr,"Summer","Autumn","Winter"}; if (form < 4) return f[form]; break; }
    // Tornadus / Thundurus / Landorus / Enamorus
    case 641: case 642: case 645: case 905: if (form == 1) return "Therian"; break;
    // Kyurem
    case 646: { const char* f[] = {nullptr,"White","Black"}; if (form < 3) return f[form]; break; }
    // Genesect
    case 649: { const char* f[] = {nullptr,"Douse","Shock","Burn","Chill"}; if (form < 5) return f[form]; break; }
    // Vivillon
    case 666: { const char* f[] = {nullptr,"Polar","Tundra","Continental","Garden","Elegant","Meadow","Modern","Marine","Archipelago","High-Plains","Sandstorm","River","Monsoon","Savanna","Sun","Ocean","Jungle","Fancy","Pokeball"}; if (form < 20) return f[form]; break; }
    // Flabebe / Floette / Florges
    case 669: case 670: case 671: { const char* f[] = {nullptr,"Yellow","Orange","Blue","White","Eternal"}; if (form < 6) return f[form]; break; }
    // Furfrou
    case 676: { const char* f[] = {nullptr,"Heart","Star","Diamond","Debutante","Matron","Dandy","La-Reine","Kabuki","Pharaoh"}; if (form < 10) return f[form]; break; }
    // Meowstic / Indeedee / Basculegion / Oinkologne
    case 678: case 876: case 902: case 916: if (form == 1) return "Female"; break;
    // Aegislash
    case 681: if (form == 1) return "Blade"; break;
    // Pumpkaboo / Gourgeist
    case 710: case 711: { const char* f[] = {nullptr,"Average","Large","Super"}; if (form < 4) return f[form]; break; }
    // Zygarde
    case 718: { const char* f[] = {nullptr,"10","10-PC","50-PC","Complete"}; if (form < 5) return f[form]; break; }
    // Hoopa
    case 720: if (form == 1) return "Unbound"; break;
    // Oricorio
    case 741: { const char* f[] = {nullptr,"Pom-Pom","Pau","Sensu"}; if (form < 4) return f[form]; break; }
    // Rockruff
    case 744: if (form == 1) return "Dusk"; break;
    // Lycanroc
    case 745: { const char* f[] = {nullptr,"Midnight","Dusk"}; if (form < 3) return f[form]; break; }
    // Wishiwashi
    case 746: if (form == 1) return "School"; break;
    // Minior
    case 774: { const char* f[] = {nullptr,"Orange-Meteor","Yellow-Meteor","Green-Meteor","Blue-Meteor","Indigo-Meteor","Violet-Meteor","Red-Core","Orange-Core","Yellow-Core","Green-Core","Blue-Core","Indigo-Core","Violet-Core"}; if (form < 14) return f[form]; break; }
    // Necrozma
    case 800: { const char* f[] = {nullptr,"Dusk-Mane","Dawn-Wings","Ultra"}; if (form < 4) return f[form]; break; }
    // Cramorant
    case 845: { const char* f[] = {nullptr,"Gulping","Gorging"}; if (form < 3) return f[form]; break; }
    // Toxtricity
    case 849: if (form == 1) return "Low-Key"; break;
    // Sinistea
    case 854: if (form == 1) return "Antique"; break;
    // Polteageist
    case 855: if (form == 1) return "Antique"; break;
    // Alcremie
    case 869: { const char* f[] = {nullptr,"Ruby-Cream","Matcha-Cream","Mint-Cream","Lemon-Cream","Salted-Cream","Ruby-Swirl","Caramel-Swirl","Rainbow-Swirl"}; if (form < 9) return f[form]; break; }
    // Eiscue
    case 875: if (form == 1) return "Noice"; break;
    // Morpeko
    case 877: if (form == 1) return "Hangry"; break;
    // Zacian
    case 888: if (form == 1) return "Crowned"; break;
    // Zamazenta
    case 889: if (form == 1) return "Crowned"; break;
    // Eternatus
    case 890: if (form == 1) return "Eternamax"; break;
    // Urshifu
    case 892: if (form == 1) return "Rapid-Strike"; break;
    // Zarude
    case 893: if (form == 1) return "Dada"; break;
    // Calyrex
    case 898: { const char* f[] = {nullptr,"Ice-Rider","Shadow-Rider"}; if (form < 3) return f[form]; break; }
    // Maushold
    case 925: if (form == 1) return "Family-of-Four"; break;
    // Squawkabilly
    case 931: { const char* f[] = {nullptr,"Blue","Yellow","White"}; if (form < 4) return f[form]; break; }
    // Palafin
    case 964: if (form == 1) return "Hero"; break;
    // Tatsugiri
    case 978: { const char* f[] = {nullptr,"Droopy","Stretchy"}; if (form < 3) return f[form]; break; }
    // Dudunsparce
    case 982: if (form == 1) return "Three-Segment"; break;
    // Gimmighoul
    case 999: if (form == 1) return "Roaming"; break;
    // Koraidon
    case 1007: { const char* f[] = {nullptr,"Limited","Sprinting","Swimming","Gliding"}; if (form < 5) return f[form]; break; }
    // Miraidon
    case 1008: { const char* f[] = {nullptr,"Low-Power","Drive","Aquatic","Glide"}; if (form < 5) return f[form]; break; }
    // Poltchageist
    case 1012: if (form == 1) return "Artisan"; break;
    // Sinistcha
    case 1013: if (form == 1) return "Masterpiece"; break;
    // Ogerpon
    case 1017: { const char* f[] = {nullptr,"Wellspring","Hearthflame","Cornerstone"}; if (form < 4) return f[form]; break; }
    // Terapagos
    case 1024: { const char* f[] = {nullptr,"Terastal","Stellar"}; if (form < 3) return f[form]; break; }
    }

    return nullptr;
}
