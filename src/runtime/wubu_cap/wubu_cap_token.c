/*
 * wubu_cap_token.c -- WuBuOS capability core token slow-path helpers.
 * Ported from GrahaOS kernel/cap/token.c.
 */
#include "wubu_cap_internal.h"

#include <stddef.h>

bool wubu_cap_validate_audience(const wubu_cap_object_t *obj, int32_t pid) {
    if (!obj) return false;
    if (obj->flags & WUBU_CAP_FLAG_PUBLIC) return true;
    for (uint8_t i = 0; i < obj->audience_count && i < WUBU_CAP_AUDIENCE_MAX; i++)
        if (obj->audience_set[i] == pid) return true;
    return false;
}

static int put_str(char *buf, int pos, int buflen, const char *s) {
    while (*s && pos < buflen - 1) buf[pos++] = *s++;
    return pos;
}
static int put_dec(char *buf, int pos, int buflen, uint32_t v) {
    char tmp[11]; int tlen = 0;
    if (v == 0) tmp[tlen++] = '0';
    else { while (v) { tmp[tlen++] = (char)('0' + (v % 10u)); v /= 10u; } }
    while (tlen > 0 && pos < buflen - 1) buf[pos++] = tmp[--tlen];
    return pos;
}
static int put_hex2(char *buf, int pos, int buflen, uint8_t v) {
    static const char hex[] = "0123456789abcdef";
    if (pos < buflen - 1) buf[pos++] = hex[(v >> 4) & 0xF];
    if (pos < buflen - 1) buf[pos++] = hex[v & 0xF];
    return pos;
}

int wubu_cap_token_describe(wubu_cap_token_t tok, char *buf, int buflen) {
    if (!buf || buflen <= 0) return 0;
    int pos = 0;
    pos = put_str(buf, pos, buflen, "tok={gen=");
    pos = put_dec(buf, pos, buflen, wubu_cap_token_gen(tok));
    pos = put_str(buf, pos, buflen, ",idx=");
    pos = put_dec(buf, pos, buflen, wubu_cap_token_idx(tok));
    pos = put_str(buf, pos, buflen, ",flags=0x");
    pos = put_hex2(buf, pos, buflen, wubu_cap_token_flags(tok));
    if (pos < buflen - 1) buf[pos++] = '}';
    buf[pos] = '\0';
    return pos;
}
