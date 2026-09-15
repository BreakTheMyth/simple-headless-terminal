#include "utf8.h"

int cell_to_utf8(const uint32_t cell, char utf8[5]) {

    int utf8_len = 0;

    utf8[utf8_len] = 0;

    if ((cell > 0x10FFFF) ||
        (cell >= 0xD800 && cell <= 0xDFFF)) return 0;

    if (cell <= 0x7F) {
        utf8[utf8_len++] = (0x00 | ((cell >>  0) & 0x7F));
    } else if (cell <= 0x7FF) {
        utf8[utf8_len++] = (0xC0 | ((cell >>  6) & 0x1F));
        utf8[utf8_len++] = (0x80 | ((cell >>  0) & 0x3F));
    } else if (cell <= 0xFFFF) {
        utf8[utf8_len++] = (0xE0 | ((cell >> 12) & 0x0F));
        utf8[utf8_len++] = (0x80 | ((cell >>  6) & 0x3F));
        utf8[utf8_len++] = (0x80 | ((cell >>  0) & 0x3F));
    } else {
        utf8[utf8_len++] = (0xF0 | ((cell >> 18) & 0x07));
        utf8[utf8_len++] = (0x80 | ((cell >> 12) & 0x3F));
        utf8[utf8_len++] = (0x80 | ((cell >>  6) & 0x3F));
        utf8[utf8_len++] = (0x80 | ((cell >>  0) & 0x3F));
    }

    utf8[utf8_len] = 0;

    return utf8_len;
}