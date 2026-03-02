#pragma once
#include <cstdint>

// Get base PP for a move by generation context.
// gen: 8 = PK8/PB8, 81 = PA8, 9 = PK9, 91 = PA9
uint8_t getMovePP(int gen, uint16_t moveId);
