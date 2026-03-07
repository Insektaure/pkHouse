#pragma once
#include <cstdint>

// Returns the form name for a given species + form number, or nullptr if unknown.
const char* getFormName(uint16_t species, uint8_t form);
