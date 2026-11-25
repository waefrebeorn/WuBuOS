/* wubu_x11_bridge.c -- see wubu_x11_bridge.h. */
#include "wubu_x11_bridge.h"

#include <string.h>

int wubu_x11_parse_setup(const uint8_t *buf, size_t n, WubuX11Setup *out) {
    if (!buf || !out) return -1;
    if (n < WUBU_X11_SETUP_FIXED) return -1;

    uint8_t order = buf[0];
    if (order != 'B' && order != 'l') return -1;
    out->lsb_first = (order == 'l');

    /* The version and both lengths are read in the CLIENT's byte order, which is
     * the whole reason this function exists. */
    #define RD16(off) ((uint16_t)(out->lsb_first ?         ((uint16_t)buf[off] | ((uint16_t)buf[(off)+1] << 8)) :         (((uint16_t)buf[off] << 8) | (uint16_t)buf[(off)+1])))
    out->major = RD16(2);
    out->minor = RD16(4);
    out->auth_name_len = RD16(6);
    out->auth_data_len = RD16(8);
    #undef RD16

    /* An X11 client that cannot name its auth protocol is not a client. */
    if (out->auth_name_len == 0 || out->auth_name_len >= WUBU_X11_AUTH_NAME_MAX)
        return -2;

    /* BOTH the name and the data are padded to four bytes. Forgetting the name's
     * padding under-counts by up to 3 bytes, which desynchronises every message
     * after the setup -- the test caught exactly this. */
    size_t name_off = WUBU_X11_SETUP_FIXED;
    size_t name_end = name_off + ((out->auth_name_len + 3u) & ~3u);
    size_t data_end = name_end + ((out->auth_data_len + 3u) & ~3u);
    if (data_end > n) return -3;

    memcpy(out->auth_name, buf + name_off, out->auth_name_len);
    out->auth_name[out->auth_name_len] = '\0';
    out->total_len = data_end;
    return 0;
}
