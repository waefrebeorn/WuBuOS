/* wubu_x11_bridge.h -- the WuBuOS side of an X11 connection.
 *
 * A guest may bring its programs; it may not bring its display. The bridge is how
 * an X11 program becomes an ORDINARY WuBuOS window: the server reads the client's
 * connection-setup block here, and the framebuffer it produces is presented into a
 * DosGuiWindow, exactly the way the DOS box already presents its scanout.
 *
 * This header owns only the parse, because the parse is where the byte order of
 * the ENTIRE connection is decided: every later length, coordinate and pixel in
 * the session is read with that one bit. Getting it wrong makes the server look
 * like a protocol bug hundreds of messages later.
 */
#ifndef WUBU_X11_BRIDGE_H
#define WUBU_X11_BRIDGE_H

#include <stddef.h>
#include <stdint.h>

#define WUBU_X11_SETUP_FIXED 12      /* the fixed part of the setup block */
#define WUBU_X11_AUTH_NAME_MAX 128

typedef struct WubuX11Setup {
    int      lsb_first;              /* 1 when the client is little-endian */
    uint16_t major, minor;           /* 11.0 in practice */
    uint16_t auth_name_len;
    uint16_t auth_data_len;
    char     auth_name[WUBU_X11_AUTH_NAME_MAX];  /* NUL-terminated copy */
    size_t   total_len;              /* bytes this setup block occupies, padded */
} WubuX11Setup;

/* Returns 0 on success, negative on a malformed or truncated block. On success
 * `need` (may be NULL) receives the total padded length to consume. */
int wubu_x11_parse_setup(const uint8_t *buf, size_t n, WubuX11Setup *out);

#endif
