/*
 * wubu_lzx.h -- the IN-KERNEL LZX decompressor surface.
 *
 * LZX is the Microsoft LZ77 + 2x-Huffman algorithm used inside MS-CAB
 * cabinets with the "LZX" compression type.  Halo PC's Inno Setup
 * installer embeds exactly one such cabinet (CAB1.CAB) in its .rsrc
 * section, compressed with LZX:21 (21-bit = 2 MiB sliding window).
 *
 * The last session "cheated" by shelling out to host 7z to decode this;
 * per your directive the kernel owns the binary, so this module is a
 * faithful, self-contained port of the LZX algorithm (from the ReactOS
 * cabinet decoder, itself derived from cabextract by Stuart Caie).
 * No host libz, no system CAB tool -- only the kernel's own heap.
 */
#ifndef WUBU_LZX_H
#define WUBU_LZX_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* return codes */
#define LZX_OK            0
#define LZX_DATAFORMAT    1   /* corrupt / unsupported block */
#define LZX_ILLEGALDATA   2   /* illegal symbol / table overrun */
#define LZX_NOMEM         3   /* heap failure */

/* opaque decoder state, kernel-heap allocated */
struct lzx_state;

/* Create a decoder.  `window_bytes` is the absolute window size in bytes;
 * pass (1u << 21) for LZX:21 (Halo PC). */
struct lzx_state *lzx_init(uint32_t window_bytes);
void               lzx_free(struct lzx_state *s);
/* Reset per-file state (new CFDATA stream / new file). Returns 0 on ok. */
int                lzx_reset(struct lzx_state *s);

const char *lzx_strerror(int rc);

/* Debug: returns which internal error site was last hit (0 = ok, nonzero = fail point). */
int lzx_debug_fail(struct lzx_state *s);

/* Debug: dump internal LZX state as a single hex value */
uint32_t lzx_debug_state(struct lzx_state *s);

/* Decompress one CAB CFDATA "frame".
 *   s      : decoder state (call lzx_reset between frames)
 *   in     : the LZX-compressed bytes (after the 8-byte CFDATA header)
 *   inlen  : byte length of the compressed input
 *   out    : caller-supplied output buffer
 *   outlen : bytes to produce (the block_remaining / uncompressed size)
 * Returns LZX_OK on success; the E8 Intel transform is applied in-place
 * to `out` when applicable (x86 executables). */
int lzx_decompress(struct lzx_state *s,
                   const uint8_t *in, uint32_t inlen,
                   uint8_t *out, uint32_t outlen);

#ifdef __cplusplus
}
#endif
#endif /* WUBU_LZX_H */
