/* wubu_inflate.h -- DEFLATE decoding (RFC 1951) and the zlib wrapper (RFC 1950).
 *
 * Own implementation (docs/GOAL-SLERM.json, SLERM-2001). This is the mechanism
 * under ZIP, PNG, DOCX/XLSX/ODT, PDF streams and HTTP content-encoding, so a
 * fail-open decoder is a data-integrity bug, not a cosmetic one: every refusal
 * below is a named failure, never a silent short read.
 */
#ifndef WUBU_INFLATE_H
#define WUBU_INFLATE_H

#include <stddef.h>

enum {
    WUBU_INFLATE_OK         =  0,
    WUBU_INFLATE_ARGS       = -1,  /* null pointers / zero capacity */
    WUBU_INFLATE_TRUNCATED  = -2,  /* input ended mid-symbol */
    WUBU_INFLATE_BAD_BLOCK  = -3,  /* block type 3, or stored LEN/NLEN mismatch */
    WUBU_INFLATE_BAD_CODE   = -4,  /* invalid Huffman code or over-subscribed table */
    WUBU_INFLATE_BAD_DIST   = -5,  /* distance beyond the bytes produced so far */
    WUBU_INFLATE_OVERFLOW   = -6,  /* output larger than dstcap */
    WUBU_INFLATE_DICT       = -7,  /* zlib preset dictionary: unsupported */
    WUBU_INFLATE_CHECKSUM   = -8,  /* zlib Adler-32 mismatch */
    WUBU_INFLATE_BAD_HEADER = -9   /* not a zlib stream we accept */
};

/* Inflate `src` into `dst`. Accepts a zlib wrapper (2-byte header + Adler-32) or
 * a raw DEFLATE stream. Returns the number of bytes produced, or -1 with *err set.
 * A zlib stream is verified, not trusted: a wrong checksum is CHECKSUM, not OK. */
long wubu_inflate(const unsigned char *src, size_t srclen,
                  unsigned char *dst, size_t dstcap, int *err);

/* Adler-32 (RFC 1950) over `n` bytes, exposed because the zlib path checks it. */
unsigned long wubu_adler32(const unsigned char *p, size_t n);

const char *wubu_inflate_reason(int err);

#endif
