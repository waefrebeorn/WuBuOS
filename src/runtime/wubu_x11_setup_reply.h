/* wubu_x11_setup_reply.h -- the server's answer to a connection setup.
 *
 * The client sends its setup block (wubu_x11_parse_setup); the server answers with
 * either a Failed block or a Success block describing the screens it offers. This
 * is the Success block. Get a field wrong and a real client refuses to connect
 * with nothing but "cannot open display" -- which is why the encoder is tested
 * byte by byte against the protocol layout rather than by trying a client and
 * hoping.
 *
 * The server offers exactly ONE screen and ONE visual, and says so: a broadcast
 * surface (1024x768, 24-bit TrueColor) that is presented into a DosGuiWindow. A
 * guest does not get to choose a display mode; it gets a window.
 */
#ifndef WUBU_X11_SETUP_REPLY_H
#define WUBU_X11_SETUP_REPLY_H

#include <stddef.h>
#include <stdint.h>

typedef struct WubuX11ScreenCfg {
    uint16_t width, height;      /* pixels */
    uint16_t width_mm, height_mm;
    uint8_t  depth;              /* 24: TrueColor, 8 bits per channel */
    uint32_t root_window;        /* resource id of the root */
    uint32_t root_visual;
    uint32_t default_colormap;
} WubuX11ScreenCfg;

#define WUBU_X11_REPLY_MAX 512

/* Build a Success setup reply for `cfg`. Writes at most `cap` bytes and returns
 * the byte count, or a negative value when the buffer is too small. `lsb_first`
 * must match the client's byte order -- every field is written in it. */
int wubu_x11_build_setup_reply(uint8_t *out, size_t cap,
                               const WubuX11ScreenCfg *cfg, int lsb_first);

#endif
