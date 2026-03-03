#pragma once
#include <cstdint>
#include <cstddef>

/// Distribution date entry for WC9 wondercards.
/// Extracted from PKHeX.Core EncounterServerDate.cs
struct WC9DateEntry {
    uint16_t cardID;       // CardID for WC9Gifts, Checksum for WC9GiftsChk
    uint16_t startYear;
    uint8_t  startMonth;
    uint8_t  startDay;
    uint16_t endYear;      // 0 = no end date (open-ended)
    uint8_t  endMonth;
    uint8_t  endDay;
    uint8_t  daysAfterStart; // GenerateDaysAfterStart
};

/// WC9Gifts - keyed by CardID (decimal)
static constexpr WC9DateEntry WC9Gifts[] = {
    //  CardID  StartY  SM  SD   EndY  EM  ED  Days  Description
    {    1, 2022, 11, 17,    0,  0,  0, 0}, // PokéCenter Birthday Flabébé
    {    6, 2022, 12, 16, 2023,  2,  1, 0}, // Jump Festa Gyarados
    {  501, 2023,  2, 16, 2023,  2, 21, 0}, // Jiseok's Garganacl
    { 1513, 2023,  2, 27, 2024,  3,  1, 0}, // Hisuian Zoroark DLC Purchase Gift
    {  502, 2023,  3, 31, 2023,  7,  1, 0}, // TCG Flying Lechonk
    {  503, 2023,  4, 13, 2023,  4, 18, 0}, // Gavin's Palafin
    {   25, 2023,  4, 21, 2023,  8,  1, 0}, // Pokémon Center Pikachu (Mini & Jumbo)
    { 1003, 2023,  5, 29, 2023,  8,  1, 0}, // Arceus Jewel of Life Bronzong
    { 1002, 2023,  5, 31, 2023,  8,  1, 0}, // Arceus Jewel of Life Pichu
    {   28, 2023,  6,  9, 2023,  6, 12, 0}, // Soramitsu's Bronzong
    { 1005, 2023,  6, 16, 2023,  6, 20, 0}, // Jeongwonsuk's Gastrodon
    {  504, 2023,  6, 30, 2023,  7,  4, 0}, // Paul's Shiny Arcanine
    { 1522, 2023,  7, 21, 2023,  9,  1, 0}, // Dark Tera Type Charizard
    {   24, 2023,  7, 26, 2023,  8, 19, 0}, // Nontaro's Shiny Grimmsnarl
    {  505, 2023,  8,  7, 2023,  9,  1, 0}, // WCS 2023 Stretchy Form Tatsugiri
    { 1521, 2023,  8,  8, 2023,  9, 19, 0}, // My Very Own Mew
    {  506, 2023,  8, 10, 2023,  8, 15, 0}, // Eduardo Gastrodon
    { 1524, 2023,  9,  6, 2024,  9,  1, 0}, // Glaseado Cetitan
    {  507, 2023, 10, 13, 2024,  1,  1, 0}, // Trixie Mimikyu
    {   31, 2023, 11,  1, 2025,  2,  1, 0}, // PokéCenter Birthday Charcadet and Pawmi
    { 1006, 2023, 11,  2, 2024,  1,  1, 0}, // Korea Bundle Fidough
    {  508, 2023, 11, 17, 2023, 11, 21, 0}, // Alex's Dragapult
    { 1526, 2023, 11, 22, 2024, 11,  1, 0}, // Team Star Revavroom
    { 1529, 2023, 12,  7, 2023, 12, 22, 0}, // New Moon Darkrai
    { 1530, 2023, 12,  7, 2024,  1,  4, 0}, // Shiny Buddy Lucario
    { 1527, 2023, 12, 13, 2024, 12,  1, 0}, // Paldea Gimmighoul
    {   36, 2023, 12, 14, 2024,  2, 14, 0}, // CoroCoro Roaring Moon and Iron Valiant
    { 1007, 2023, 12, 29, 2024,  2, 11, 0}, // Winter Festa Baxcalibur
    {   38, 2024,  1, 14, 2024,  3, 14, 0}, // CoroCoro Scream Tail, Brute Bonnet, etc.
    {   48, 2024,  2, 22, 2024,  4,  1, 0}, // Project Snorlax Campaign Gift
    { 1534, 2024,  3, 12, 2025,  3,  1, 0}, // YOASOBI Pawmot
    { 1535, 2024,  3, 14, 2024, 10,  1, 0}, // Liko's Sprigatito
    {  509, 2024,  4,  4, 2024,  4,  9, 0}, // Marco's Iron Hands
    { 1008, 2024,  5,  4, 2024,  5,  8, 0}, // Sin Yeomyeong's Flutter Mane
    {   52, 2024,  5, 11, 2024,  7,  1, 0}, // Sophia's Gyarados
    { 1536, 2024,  5, 18, 2024, 12,  1, 0}, // Dot's Quaxly
    {   49, 2024,  5, 31, 2024,  6,  3, 0}, // Naaku's Talonflame
    {  510, 2024,  6,  7, 2024,  6, 11, 0}, // Nils's Porygon2
    {   50, 2024,  7, 13, 2024, 10,  1, 0}, // Japan Pokéss Summer Festival Eevee
    { 1537, 2024,  7, 24, 2025,  2,  1, 0}, // Roy's Fuecoco
    {  511, 2024,  8, 15, 2024,  8, 31, 0}, // WCS 2024 Steenee
    {  512, 2024,  8, 16, 2024,  8, 20, 0}, // Tomoya's Sylveon
    {   62, 2024, 10, 31, 2026,  2,  1, 0}, // PokéCenter Birthday Tandemaus
    {  513, 2024, 11, 15, 2024, 11, 23, 0}, // Patrick's Pelipper
    {   54, 2024, 11, 21, 2025,  6,  1, 0}, // Operation Get Mythical JPN Keldeo
    {   55, 2024, 11, 21, 2025,  6,  1, 0}, // Operation Get Mythical JPN Zarude
    {   56, 2024, 11, 21, 2025,  6,  1, 0}, // Operation Get Mythical JPN Deoxys
    { 1011, 2024, 11, 21, 2025,  6,  1, 0}, // Operation Get Mythical KOR Keldeo
    { 1012, 2024, 11, 21, 2025,  6,  1, 0}, // Operation Get Mythical KOR Zarude
    { 1013, 2024, 11, 21, 2025,  6,  1, 0}, // Operation Get Mythical KOR Deoxys
    { 1010, 2025,  1, 21, 2025,  4,  1, 0}, // KOR Aaron's Lucario (Movie Gift)
    {  514, 2025,  2,  5, 2025,  7,  1, 2}, // Pokémon Day 2025 Flying Tera Eevee
    {  519, 2025,  2, 20, 2025,  3,  1, 0}, // Marco's Jumpluff
    {   66, 2025,  4, 18, 2025,  8,  1, 0}, // Wei Chyr's Rillaboom
    { 1019, 2025,  4, 24, 2025,  7,  1, 0}, // Pokémon Town KOR Ditto Project
    { 1020, 2025,  6,  6, 2025,  6, 10, 0}, // PTC 2025 KOR Porygon2
    {  523, 2025,  6, 13, 2025,  6, 21, 0}, // NAIC 2025 Wolfe's Incineroar
    {   67, 2025,  6, 20, 2025,  6, 23, 0}, // PJCS 2025 Hyuma Hara's Flutter Mane
    {   68, 2025,  6, 20, 2025, 10,  1, 0}, // PJCS 2025 Ray Yamanaka's Amoonguss
    { 1542, 2025,  8,  7, 2025, 10,  1, 0}, // Shiny Wo-Chien
    { 1544, 2025,  8, 21, 2025, 10,  1, 0}, // Shiny Chien-Pao
    { 1546, 2025,  9,  4, 2025, 10,  1, 0}, // Shiny Ting-Lu
    { 1548, 2025,  9, 18, 2025, 10,  1, 0}, // Shiny Chi-Yu
    {  524, 2025,  8, 14, 2025,  8, 31, 0}, // WCS 2025 Toedscool
    {  525, 2025,  8, 15, 2025,  8, 23, 0}, // WCS 2025 Luca Ceribelli's Farigiraf
    { 1540, 2025,  9, 25, 2025, 10, 25, 0}, // Shiny Miraidon/Koraidon Gift
    {   70, 2025, 10, 31, 2027,  2,  1, 0}, // PokéCenter Fidough Birthday Gift
    {  526, 2025, 11, 21, 2025, 12,  1, 0}, // LAIC 2026 Federico Camporesi's Whimsicott

    // HOME 3.0.0+ gifts (open-ended)
    { 9021, 2023,  5, 30,    0,  0,  0, 0}, // Hidden Ability Sprigatito
    { 9022, 2023,  5, 30,    0,  0,  0, 0}, // Hidden Ability Fuecoco
    { 9023, 2023,  5, 30,    0,  0,  0, 0}, // Hidden Ability Quaxly
    { 9024, 2024, 10, 16,    0,  0,  0, 0}, // Shiny Meloetta
    { 9025, 2024, 11,  1,    0,  0,  0, 0}, // PokéCenter Birthday Tandemaus
    { 9030, 2025, 10, 31,    0,  0,  0, 0}, // PokéCenter Fidough Birthday Gift
};

static constexpr size_t WC9Gifts_Count = sizeof(WC9Gifts) / sizeof(WC9Gifts[0]);

/// WC9GiftsChk - keyed by Checksum (hex), not CardID
static constexpr WC9DateEntry WC9GiftsChk[] = {
    // Checksum  StartY  SM  SD   EndY  EM  ED  Days  Description
    {0xE5EB, 2022, 11, 17, 2023,  2,  3, 0}, // Fly Pikachu - rev 1 (male, 128 height/weight)
    {0x908B, 2023,  2,  2, 2023,  3,  1, 0}, // Fly Pikachu - rev 2 (both 0 height/weight)
};

static constexpr size_t WC9GiftsChk_Count = sizeof(WC9GiftsChk) / sizeof(WC9GiftsChk[0]);
