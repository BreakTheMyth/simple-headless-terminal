#pragma once

#include <stdint.h>

int cell_to_utf8(const uint32_t cell, char utf8[5]);