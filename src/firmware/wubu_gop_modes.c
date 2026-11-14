/*
 * wubu_gop_modes.c -- see wubu_gop_modes.h.
 *
 * Layout of EFI_GRAPHICS_OUTPUT_MODE_INFORMATION (UEFI spec 12.9.2, and the copy in
 * src/firmware/efi.h), all little-endian, no padding surprises on the platforms we
 * target:
 *
 *   offset 0   UINT32 Version                    (must be 0)
 *   offset 4   UINT32 HorizontalResolution
 *   offset 8   UINT32 VerticalResolution
 *   offset 12  UINT32 PixelFormat                (enum value)
 *   offset 16  UINT32 RedMask      \  EFI_PIXEL_BITMASK, valid only when
 *   offset 20  UINT32 GreenMask     > PixelFormat == PixelBitMask
 *   offset 24  UINT32 BlueMask      |
 *   offset 28  UINT32 ReservedMask /
 *   offset 32  UINT32 PixelsPerScanLine
 *
 * Total 36 bytes.  The stride is a separate field from the width on purpose: firmware
 * pads scan lines for alignment, and treating the two as equal makes every framebuffer
 * blit walk off its row.
 */
#include "wubu_gop_modes.h"

#include <string.h>

#include "../firmware/efi.h"              /* the wire structures, reused */

/* Compile-time proof that the struct we are about to decode is exactly what the
 * specification says it is.  If someone extends efi.h without telling us, this fails
 * at build time instead of silently misreading firmware data. */
_Static_assert(sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION) == 36,
               "GOP mode-information layout must match the UEFI specification");

static uint32_t rd_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

bool wubu_gop_decode_mode(const uint8_t *buf, size_t len,
                          size_t record_size, wubu_gop_mode_t *out)
{
    if (!buf || !out) return false;

    memset(out, 0, sizeof(*out));            /* fail closed: untouched on refusal */

    /* A short buffer cannot hold the structure; a short STRIDE is equally unusable. */
    if (len < sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION)) return false;
    if (record_size < sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION)) return false;

    uint32_t version = rd_u32(buf + 0);
    /* Only Version 0 is defined; a future firmware may append fields, so refuse rather
     * than decode a layout we do not know. */
    if (version != 0) return false;

    int h = (int)rd_u32(buf + 4);
    int v = (int)rd_u32(buf + 8);
    int format = (int)rd_u32(buf + 12);
    int stride = (int)rd_u32(buf + 32);

    if (h <= 0 || v <= 0) return false;
    if (stride < h) return false;            /* narrower than visible width: corrupt */
    if (format < 0 || format >= (int)PixelFormatMax) return false;

    out->h = h;
    out->v = v;
    out->stride = stride;
    out->format = format;
    out->has_framebuffer = (format != PixelBltOnly);

    /* The bit masks are meaningful ONLY for PixelBitMask.  Copying them otherwise would
     * give a caller colour planes that describe nothing. */
    if (format == PixelBitMask) {
        out->r_mask       = rd_u32(buf + 16);
        out->g_mask       = rd_u32(buf + 20);
        out->b_mask       = rd_u32(buf + 24);
        out->reserved_mask = rd_u32(buf + 28);
    }

    out->valid = true;
    return true;
}

int wubu_gop_walk_modes(const uint8_t *buf, size_t total_len,
                        size_t record_size, wubu_gop_mode_t *out, int cap)
{
    if (!buf || !out || cap <= 0) return 0;
    if (record_size < sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION)) return 0;

    int n = 0;
    size_t off = 0;

    while (off + record_size <= total_len && n < cap) {
        if (!wubu_gop_decode_mode(buf + off, total_len - off,
                                  record_size, &out[n])) {
            break;                     /* a broken entry means the rest is suspect */
        }
        n++;
        off += record_size;
    }
    return n;
}
