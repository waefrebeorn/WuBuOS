/*
 * wubu_gop_modes.h -- decode a firmware Graphics Output Protocol mode list.
 *
 * WHY THIS EXISTS
 *   On the bootable image there is no sysfs, no X server and no Linux DRM card, so the
 *   only display information available comes from the FIRMWARE: the UEFI Graphics
 *   Output Protocol, whose QueryMode returns an EFI_GRAPHICS_OUTPUT_MODE_INFORMATION
 *   per supported resolution. Nothing in this tree decoded those records -- the
 *   structures were defined in firmware/efi.h but never consumed.
 *
 * WHAT THIS IS
 *   Pure decoding of mode-information records into plain integers.  No firmware calls,
 *   no pointers dereferenced, no allocation beyond the caller's array -- so the exact
 *   same code runs on the metal (records read from the boot services table), under a
 *   host test harness, or inside the UEFI firmware itself.
 *
 * REUSE, NOT REDEFINITION
 *   The wire structures come from src/firmware/efi.h, which mirrors the published UEFI
 *   specification (verified against uefi.org 2.10/2.11 section 12.9).  Redefining them
 *   here would guarantee a silent mismatch the day either side changes.
 *
 * FAILURE POLICY
 *   Every refusal returns false and leaves the output untouched.  A mode claiming a
 *   non-zero Version, a zero resolution, a stride narrower than its width, or a pixel
 *   format at or past PixelFormatMax is rejected rather than guessed at -- building a
 *   desktop around a misread mode is how you get a machine that boots to garbage.
 */
#ifndef WUBU_GOP_MODES_H
#define WUBU_GOP_MODES_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "efi.h"   /* EFI_GRAPHICS_PIXEL_FORMAT and the wire structures */

/* One decoded GOP mode.  Mirrors the useful subset of
 * EFI_GRAPHICS_OUTPUT_MODE_INFORMATION; the masks are meaningful only when
 * format == PixelBitMask. */
typedef struct {
    int      h, v;              /* HorizontalResolution / VerticalResolution */
    int      stride;            /* PixelsPerScanLine: REAL row span, may exceed h */
    int      format;            /* EFI_GRAPHICS_PIXEL_FORMAT value */
    bool     has_framebuffer;   /* false for PixelBltOnly -- no linear buffer */
    uint32_t r_mask, g_mask, b_mask, reserved_mask;
    bool     valid;
} wubu_gop_mode_t;

/* Decode ONE mode-information record.
 *
 * `record_size` must be at least sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION); firmware
 * may append extra bytes for newer Versions, which is why the length is a parameter
 * rather than assumed.
 *
 * Returns true on success.  Refusals (short record, Version != 0, empty resolution,
 * stride < width, format >= PixelFormatMax) return false.
 */
bool wubu_gop_decode_mode(const uint8_t *buf, size_t len,
                          size_t record_size, wubu_gop_mode_t *out);

/* Walk an ARRAY of consecutive records covering exactly `total_len` bytes.
 * Returns the number of successfully decoded modes written to `out` (never more than
 * cap).  A malformed record ENDS the walk: the firmware's own array is sequential and a
 * broken entry means everything after it is suspect.
 */
int wubu_gop_walk_modes(const uint8_t *buf, size_t total_len,
                        size_t record_size, wubu_gop_mode_t *out, int cap);

#endif /* WUBU_GOP_MODES_H */
