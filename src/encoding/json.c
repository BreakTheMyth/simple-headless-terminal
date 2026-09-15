#include <stdio.h>

#include "json.h"
#include "utf8.h"
#include "log/log.h"

typedef enum PARSE_STATE {
    NORMAL,
    ESCAPED,    
    HEX1,
    HEX2,
    HEX3,
    HEX4,
    LOW,
    LOW_ESCAPED,
    LOW_HEX1,
    LOW_HEX2,
    LOW_HEX3,
    LOW_HEX4,
} PARSE_STATE;

typedef struct json_parser {
    PARSE_STATE  state;
    uint32_t     tmp;
    char        *p_dst;
} json_parser;

typedef void (*handle_json_state)(json_parser *self, char c);

static void handle_normal(json_parser *self, char c) {

    if (c == '\\') {
        self->state = ESCAPED;
    } else {
        *(self->p_dst++) = c;
    }
}

static void handle_escaped(json_parser *self, char c) {

    switch (c) {
        case '\"':
        case '\\':
            *(self->p_dst++) = c;

            self->state = NORMAL;

            break;
        case 'b':
            *(self->p_dst++) = '\b';

            self->state = NORMAL;

            break;
        case 't':
            *(self->p_dst++) = '\t';

            self->state = NORMAL;

            break;
        case 'n':
            *(self->p_dst++) = '\n';

            self->state = NORMAL;

            break;
        case 'f':
            *(self->p_dst++) = '\f';

            self->state = NORMAL;

            break;
        case 'r':
            *(self->p_dst++) = '\r';

            self->state = NORMAL;

            break;
        case '/':
            *(self->p_dst++) = '/';

            self->state = NORMAL;

            break;
        case 'u':
            self->state = HEX1;

            break;
        default:
            LOG_ERROR("Failed to parse \"\\%c\".", c);

            self->state = NORMAL;
    }
}

static int check_hex(char c) {

    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    } else if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    } else {
        return -1;
    }
}

static void handle_hex1(json_parser *self, char c) {

    int hex = check_hex(c);

    if (hex < 0) {

        LOG_ERROR("Failed to parse \"\\u%c\"", c);

        self->tmp   = 0;
        self->state = NORMAL;

        return;
    }

    self->tmp   = (self->tmp << 4) | hex;
    self->state = HEX2;
}

static void handle_hex2(json_parser *self, char c) {

    int hex = check_hex(c);

    if (hex < 0) {

        LOG_ERROR("Failed to parse \"\\u%01x%c\"", self->tmp, c);

        self->tmp   = 0;
        self->state = NORMAL;

        return;
    }

    self->tmp   = (self->tmp << 4) | hex;
    self->state = HEX3;
}

static void handle_hex3(json_parser *self, char c) {

    int hex = check_hex(c);

    if (hex < 0) {

        LOG_ERROR("Failed to parse \"\\u%02x%c\"", self->tmp, c);

        self->tmp   = 0;
        self->state = NORMAL;

        return;
    }

    self->tmp   = (self->tmp << 4) | hex;
    self->state = HEX4;
}

static void handle_hex4(json_parser *self, char c) {

    int hex = check_hex(c);

    if (hex < 0) {

        LOG_ERROR("Failed to parse \"\\u%03x%c\"", self->tmp, c);

        self->tmp   = 0;
        self->state = NORMAL;

        return;
    }

    self->tmp = (self->tmp << 4) | hex;

    if (self->tmp >= 0xD800 && self->tmp <= 0xDBFF) {

        self->state = LOW;

        return;
    }

    int len = cell_to_utf8(self->tmp, self->p_dst);

    if (!len) {
        LOG_ERROR("Failed to parse \"\\u%04x\"", self->tmp);
    }

    self->p_dst += len;

    self->tmp   = 0;
    self->state = NORMAL;
}

static void handle_low(json_parser *self, char c) {

    if (c != '\\') {

        LOG_ERROR("Failed to parse \"\\u%04x%c\"", self->tmp, c);

        self->tmp   = 0;
        self->state = NORMAL;

        return;
    }

    self->state = LOW_ESCAPED;
}

static void handle_low_escaped(json_parser *self, char c) {

    if (c != 'u') {

        LOG_ERROR("Failed to parse \"\\u%04x\\%c\"", self->tmp, c);

        self->tmp   = 0;
        self->state = NORMAL;

        return;
    }

    self->state = LOW_HEX1;
}

static void handle_low_hex1(json_parser *self, char c) {

    int hex = check_hex(c);

    if (hex < 0) {

        LOG_ERROR("Failed to parse \"\\u%04x\\u%c\"", self->tmp, c);

        self->tmp   = 0;
        self->state = NORMAL;

        return;
    }

    self->tmp   = (self->tmp << 4) | hex;
    self->state = LOW_HEX2;
}

static void handle_low_hex2(json_parser *self, char c) {

    int hex = check_hex(c);

    if (hex < 0) {

        LOG_ERROR("Failed to parse \"\\u%04x\\u%01x%c\"", 
            self->tmp >> 4, self->tmp & 0xF, c);

        self->tmp   = 0;
        self->state = NORMAL;

        return;
    }

    self->tmp   = (self->tmp << 4) | hex;
    self->state = LOW_HEX3;
}

static void handle_low_hex3(json_parser *self, char c) {

    int hex = check_hex(c);

    if (hex < 0) {

        LOG_ERROR("Failed to parse \"\\u%04x\\u%02x%c\"", 
            self->tmp >> 8, self->tmp & 0xFF, c);

        self->tmp   = 0;
        self->state = NORMAL;

        return;
    }

    self->tmp   = (self->tmp << 4) | hex;
    self->state = LOW_HEX4;
}

static void handle_low_hex4(json_parser *self, char c) {

    int hex = check_hex(c);

    if (hex < 0) {

        LOG_ERROR("Failed to parse \"\\u%04x\\u%03x%c\"", 
            self->tmp >> 12, self->tmp & 0xFFF, c);

        self->tmp   = 0;
        self->state = NORMAL;

        return;
    }

    self->tmp = (self->tmp << 4) | hex;

    uint32_t high = self->tmp >> 16;
    uint32_t low  = self->tmp & 0xFFFF;

    if (low < 0xDC00 || low > 0xDFFF) {
        LOG_ERROR("Failed to parse \"\\u%04x\\u%04x\"", 
            high, low);

        self->tmp   = 0;
        self->state = NORMAL;

        return;
    }

    uint32_t cell = 0x10000 + 
        ((high - 0xD800) << 10) +
        ((low  - 0xDC00) << 0);

    int len = cell_to_utf8(cell, self->p_dst);

    if (!len) {
        LOG_ERROR("Failed to parse \"\\u%04x\\u%04x\"", 
            self->tmp >> 16, self->tmp & 0xFFFF);
    }

    self->p_dst += len;

    self->tmp   = 0;
    self->state = NORMAL;
}

static const handle_json_state json_handlers[] = {
    handle_normal,
    handle_escaped, 
    handle_hex1,
    handle_hex2,
    handle_hex3,
    handle_hex4,
    handle_low,
    handle_low_escaped,
    handle_low_hex1,
    handle_low_hex2,
    handle_low_hex3,
    handle_low_hex4,
};

#define HANDLE(p, c) json_handlers[(p).state](&(p), (c))

int json_to_utf8(
    const char *src, 
    int         src_len, 
    char       *dst, 
    int         dst_size, 
    int        *consumed) {

    if (dst_size <= src_len) return -1;

    json_parser parser = { NORMAL, 0, dst };

    *consumed = 0;

    for (int i = 0; i < src_len; i++) {

        char c = src[i];

        HANDLE(parser, c);

        if (parser.state == NORMAL) *consumed = i + 1;
    }

    *parser.p_dst = 0;

    return (int)(parser.p_dst - dst);
}

int utf8_to_json(
    const char *src, 
    int         src_len, 
    char       *dst,
    int         dst_size, 
    int        *consumed) {

    if (dst_size <= src_len * 6) return -1;

    char *p_dst = dst;

    int dst_len = src_len;

    *consumed = 0;

    for (int i = 0; i < src_len; i++) {

        char c = src[i];

        if (src[i] & 0x80) {
            *p_dst++ = c;

            continue;
        }

        switch (c) {
            case '\"':
                *p_dst++ = '\\';
                *p_dst++ = '\"';

                dst_len++;

                break;
            case '\\':
                *p_dst++ = '\\';
                *p_dst++ = '\\';

                dst_len++;

                break;
            case '\b':
                *p_dst++ = '\\';
                *p_dst++ = 'b';

                dst_len++;

                break;
            case '\t':
                *p_dst++ = '\\';
                *p_dst++ = 't';

                dst_len++;

                break;
            case '\n':
                *p_dst++ = '\\';
                *p_dst++ = 'n';

                dst_len++;

                break;
            case '\f':
                *p_dst++ = '\\';
                *p_dst++ = 'f';

                dst_len++;

                break;
            case '\r':
                *p_dst++ = '\\';
                *p_dst++ = 'r';

                dst_len++;

                break;
            default:
                if (src[i] < 0x20) {
                    p_dst += sprintf(p_dst, "\\u%04x", c);

                    dst_len += 5;

                    break;
                }

                *p_dst++ = c;

        }
    }

    *consumed = src_len;

    *p_dst = 0;

    return dst_len;
}