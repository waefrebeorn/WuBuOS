/*
 * wubu_zlib.h -- the IN-KERNEL zlib / DEFLATE surface.
 *
 * Your directive: "the kernel owns it; there is no third party." The
 * wubu_inflate_* symbols below are the kernel's OWN DEFLATE inflater,
 * ported from the project's wubuzip inflater (slermes/lib/libwubuoffice)
 * and bound to the kernel heap (mem_alloc/mem_free) so the freestanding
 * kernel never calls any host libc.
 *
 * Two entry points are exported:
 *
 *   wubu_inflate_raw  -- raw DEFLATE (RFC 1951) stream, no zlib header.
 *   wubu_inflate      -- full zlib stream (2-byte header + raw DEFLATE +
 *                         adler32 trailer), the shape inside ZIP members,
 *                         gzip bodies, and UDIF zlb chunks.
 *
 * Both use a streaming-output model: the caller supplies the destination
 * buffer + capacity and a pointer that receives the consumed-source and
 * produced-dest byte counts. This keeps the kernel freestanding (no
 * heap growable buffers inside the decoder) while still letting the
 * ZIP/CAB/UDIF readers own the allocation.
 */
#ifndef WUBU_ZLIB_H
#define WUBU_ZLIB_H

#include <stdint.h>
#include <stddef.h>

#ifdef WUBU_KERNEL
#  include "memory.h"      /* mem_alloc / mem_free */
#else
#  include <stdlib.h>
#  include <string.h>
   /* hosted tests reuse the same surface so the decoder is exercised
    * against real zlib as an oracle before it touches the kernel. */
#endif

/*
 * Raw DEFLATE (no zlib 2-byte header). Decodes src[0..slen) into dst
 * (capacity dcap). On success returns 0, sets *out_len to bytes produced
 * and *src_consumed to bytes eaten. The stream is terminated by the
 * end-of-block marker of the final block or by exhausting slen.
 *
 * Returns -1 on a malformed stream, -2 if the output would overflow dcap.
 */
int wubu_inflate_raw(const uint8_t *src, uint32_t slen,
                     uint8_t *dst, uint32_t dcap,
                     uint32_t *out_len, uint32_t *src_consumed);

/*
 * Full zlib stream (header + raw DEFLATE + adler32). Used by ZIP members
 * (method 8) and UDIF zlb chunks. Same contract as wubu_inflate_raw.
 */
int wubu_inflate(const uint8_t *src, uint32_t slen,
                 uint8_t *dst, uint32_t dcap,
                 uint32_t *out_len, uint32_t *src_consumed);

/* Adler-32 (zlib format). Used to verify inflate output integrity. */
uint32_t wubu_adler32(const uint8_t *data, uint32_t len);

/*
 * Gzip stream (RFC 1952): header | DEFLATE body | CRC32 + ISIZE trailer.
 * Parses the gzip header, skips to the DEFLATE body, runs wubu_inflate_raw,
 * then reads the 8-byte trailer. Same output contract as the others.
 */
int wubu_gunzip(const uint8_t *src, uint32_t slen,
                uint8_t *dst, uint32_t dcap,
                uint32_t *out_len, uint32_t *src_consumed);

#endif /* WUBU_ZLIB_H */
