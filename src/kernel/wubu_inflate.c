/*
 * wubu_inflate.c -- the IN-KERNEL DEFLATE (RFC 1951) + zlib inflater.
 *
 * Ported, byte-for-byte in algorithm, from the project's wubuzip inflater
 * (slermes/lib/libwubuoffice/src/wubuzip/inflate.c + block.c + huffman.c
 * + fixed.c + bit.c). The original used a heap-growable output buffer and
 * host libc malloc; the kernel version is freestanding: the decoder writes
 * straight into the caller-supplied destination buffer (no allocation inside
 * the hot path except the transient Huffman tables, which use mem_alloc).
 *
 * Algorithm preserved exactly:
 *   - LSB-first bit reader (feeds zero bits at EOF, corrupt-stream guard).
 *   - Canonical Huffman construction (Adler/puff.c style).
 *   - Fixed and dynamic block handling, length/distance base+extra tables.
 *   - end-of-block symbol 256 terminates the stream.
 *
 * The zlib wrapper (2-byte header + adler32) is handled by wubu_inflate().
 */
#include "wubu_zlib.h"
#include "libc.h"
#include <string.h>

/* ── length/distance tables (RFC 1951 3.2.5) ─────────────────────── */
static const uint16_t k_len_base[29] = {
    3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258
};
static const uint8_t  k_len_extra[29] = {
    0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0
};
static const uint16_t k_dist_base[30] = {
    1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,
    1025,1537,2049,3073,4097,6145,8193,12289,16385,24577
};
static const uint8_t  k_dist_extra[30] = {
    0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13
};
#define kNumLenLowBits   3
#define kNumLenLowSyms  (1u<<kNumLenLowBits)
#define kNumLenMidBits   5
#define kNumLenMidSyms  (1u<<kNumLenMidBits)

/* ── bit reader (LSB-first) ──────────────────────────────────────── */
typedef struct {
    const uint8_t *data;
    uint32_t len;       /* bytes available */
    uint32_t pos;       /* byte cursor */
    uint32_t bitbuf;    /* LSB-first accumulator */
    int bitcnt;         /* bits currently in bitbuf */
} wubu_br;

static inline void br_init(wubu_br *b, const uint8_t *d, uint32_t n) {
    b->data = d; b->len = n; b->pos = 0; b->bitbuf = 0; b->bitcnt = 0;
}

/* Read n (1..25) bits LSB-first. EOF feeds zero bits (caller bounds). */
static inline uint32_t br_get(wubu_br *b, int n) {
    while (b->bitcnt < n) {
        if (b->pos >= b->len) {
            b->bitbuf |= 0u;            /* zero-fill at EOF */
            b->bitcnt += 8;
        } else {
            b->bitbuf |= (uint32_t)b->data[b->pos++] << b->bitcnt;
            b->bitcnt += 8;
        }
    }
    uint32_t v = b->bitbuf & ((UINT32_C(1) << n) - 1u);
    b->bitbuf >>= n;
    b->bitcnt -= n;
    return v;
}

static inline void br_align(wubu_br *b) {
    b->bitbuf = 0; b->bitcnt = 0;
}

/* ── Huffman (canonical, Adler/puff.c) ───────────────────────────── */
typedef struct {
    uint16_t cnt[16];   /* codes of each length 1..15 */
    uint16_t sym[288];  /* up to 288 literals/lengths */
} wubu_huff;

static int huff_build(wubu_huff *h, const uint8_t *lengths, int n) {
    int cnt[16], offs[16], left;
    memset(cnt, 0, sizeof cnt);
    for (int i = 0; i < n; i++) cnt[lengths[i]]++;
    cnt[0] = 0;
    left = 1;
    for (int len = 1; len <= 15; len++) {
        left <<= 1;
        left -= cnt[len];
        if (left < 0) return -1;            /* over-subscribed */
    }
    if (left != 0 && (n == 0 || left != 1)) return -1;  /* incomplete */
    offs[1] = 0;
    for (int len = 1; len < 15; len++) offs[len + 1] = offs[len] + cnt[len];
    for (int i = 0; i < n; i++) {
        int len = lengths[i];
        if (len) h->sym[offs[len]++] = (uint16_t)i;
    }
    for (int i = 1; i < 16; i++) h->cnt[i] = (uint16_t)cnt[i];
    h->cnt[0] = 0;
    return 0;
}

static int huff_decode(wubu_br *b, const wubu_huff *h) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len <= 15; len++) {
        code |= (int)br_get(b, 1);
        int count = h->cnt[len];
        if (code - first < count) return h->sym[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

/* ── streaming output sink ───────────────────────────────────────── */
typedef struct {
    uint8_t *dst;
    uint32_t cap;
    uint32_t len;
} wubu_sink;

static inline int sink_put(wubu_sink *s, uint8_t c) {
    if (s->len >= s->cap) return -2;      /* overflow */
    s->dst[s->len++] = c;
    return 0;
}

/* ── fixed Huffman (RFC 1951 3.2.6) ──────────────────────────────── */
static void fixed_litlen(uint8_t out[288]) {
    int i = 0;
    for (; i < 144; i++) out[i] = 8;
    for (; i < 256; i++) out[i] = 9;
    for (; i < 280; i++) out[i] = 7;
    for (; i < 288; i++) out[i] = 8;
}
static void fixed_dist(uint8_t out[32]) {
    for (int i = 0; i < 32; i++) out[i] = 5;
}

/* ── decode one dynamic/fixed block ──────────────────────────────── */
static int block_decode(wubu_br *b, const wubu_huff *lit,
                        const wubu_huff *dist, wubu_sink *out) {
    for (;;) {
        int sym = huff_decode(b, lit);
        if (sym < 0) return -1;
        if (sym == 256) return 0;         /* end of block */
        if (sym < 256) {
            if (sink_put(out, (uint8_t)sym)) return -2;
            continue;
        }
        sym -= 257;
        if (sym >= 29) return -1;
        int length = k_len_base[sym] + (int)br_get(b, k_len_extra[sym]);
        int ds = huff_decode(b, dist);
        if (ds < 0 || ds >= 30) return -1;
        int distance = k_dist_base[ds] + (int)br_get(b, k_dist_extra[ds]);
        if ((uint32_t)distance > out->len) return -1;  /* back-window underflow */
        for (int i = 0; i < length; i++) {
            if (sink_put(out, out->dst[out->len - (uint32_t)distance])) return -2;
        }
    }
}

/* decode a dynamic-Huffman block */
static int block_dynamic(wubu_br *b, wubu_huff *lit, wubu_huff *dist, wubu_sink *out) {
    int hlit  = (int)br_get(b, 5) + 257;
    int hdist = (int)br_get(b, 5) + 1;
    int hclen = (int)br_get(b, 4) + 4;
    static const uint8_t ORD[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
    uint8_t cl_len[19];
    memset(cl_len, 0, sizeof cl_len);
    for (int i = 0; i < hclen; i++) cl_len[ORD[i]] = (uint8_t)br_get(b, 3);
    wubu_huff clh;
    if (huff_build(&clh, cl_len, 19) != 0) return -1;

    uint8_t lengths[320];   /* 288 + 32 */
    int n = hlit + hdist;
    int i = 0;
    while (i < n) {
        int s = huff_decode(b, &clh);
        if (s < 0) return -1;
        if (s < 16) {
            lengths[i++] = (uint8_t)s;
        } else if (s == 16) {
            if (i == 0) return -1;
            int rep = (int)br_get(b, 2) + 3;
            uint8_t v = lengths[i - 1];
            while (rep-- > 0 && i < n) lengths[i++] = v;
        } else if (s == 17) {
            int rep = (int)br_get(b, 3) + 3;
            while (rep-- > 0 && i < n) lengths[i++] = 0;
        } else if (s == 18) {
            int rep = (int)br_get(b, 7) + 11;
            while (rep-- > 0 && i < n) lengths[i++] = 0;
        } else {
            return -1;
        }
    }
    if (huff_build(lit, lengths, hlit) != 0) return -1;
    if (huff_build(dist, lengths + hlit, hdist) != 0) return -1;
    return block_decode(b, lit, dist, out);
}

/* ── the raw DEFLATE driver ──────────────────────────────────────── */
static int inflate_raw(const uint8_t *src, uint32_t slen,
                       uint8_t *dst, uint32_t dcap,
                       uint32_t *out_len, uint32_t *src_consumed) {
    wubu_br b; br_init(&b, src, slen);
    wubu_sink out = { .dst = dst, .cap = dcap, .len = 0 };
    uint32_t guard = 0;
    wubu_huff lit, dist;

    for (;;) {
        if (++guard > 1000000) return -1;   /* corrupt-stream guard */
        int bfinal = (int)br_get(&b, 1);
        int btype  = (int)br_get(&b, 2);
        int rc;
        if (btype == 0) {
            /* stored / uncompressed block */
            br_align(&b);
            if (b.pos + 4 > b.len) return -1;
            uint32_t len  = (uint32_t)src[b.pos]
                          | ((uint32_t)src[b.pos + 1] << 8);
            b.pos += 4;                      /* skip LEN + NLEN */
            for (uint32_t i = 0; i < len; i++) {
                if (b.pos >= b.len) return -1;
                if (sink_put(&out, src[b.pos++])) return -2;
            }
        } else if (btype == 1) {
            uint8_t ll[288], dd[32];
            fixed_litlen(ll);
            fixed_dist(dd);
            if (huff_build(&lit, ll, 288) != 0 ||
                huff_build(&dist, dd, 32) != 0) return -1;
            rc = block_decode(&b, &lit, &dist, &out);
            if (rc) return rc;
        } else if (btype == 2) {
            rc = block_dynamic(&b, &lit, &dist, &out);
            if (rc) return rc;
        } else {
            return -1;                      /* btype == 3: reserved/error */
        }
        if (bfinal) break;
    }
    *out_len = out.len;
    *src_consumed = b.pos;
    return 0;
}

int wubu_inflate_raw(const uint8_t *src, uint32_t slen,
                     uint8_t *dst, uint32_t dcap,
                     uint32_t *out_len, uint32_t *src_consumed) {
    return inflate_raw(src, slen, dst, dcap, out_len, src_consumed);
}

/*
 * Full zlib stream: 2-byte header (CMF+FLG) | raw DEFLATE | adler32.
 * We validate the header, run raw inflate, then (optionally) skip the
 * trailing adler32. The CM field must be 8 (deflate) and the CINFO field
 * must be <= 7 (32K window).
 */
int wubu_inflate(const uint8_t *src, uint32_t slen,
                 uint8_t *dst, uint32_t dcap,
                 uint32_t *out_len, uint32_t *src_consumed) {
    if (slen < 2) return -1;
    uint8_t cm = src[0] & 0x0f;
    uint8_t cinfo = (src[0] >> 4) & 0x0f;
    if (cm != 8) return -1;            /* not deflate */
    if (cinfo > 7) return -1;          /* window too large */
    /* FLG must be such that (CMF*256 + FLG) % 31 == 0 */
    if (((src[0] * 256 + src[1]) % 31) != 0) return -1;

    uint32_t consumed = 0;
    int rc = inflate_raw(src + 2, slen - 2, dst, dcap, out_len, &consumed);
    if (rc != 0) return rc;
    /* skip the 4-byte adler32 trailer if it fits */
    uint32_t total = 2 + consumed;
    if (total + 4 <= slen) total += 4;   /* adler32 present */
    if (src_consumed) *src_consumed = total;
    return 0;
}

/* adler32 (zlib format trailer check). Exposed so the ZIP/CAB callers can
 * verify integrity; standalone here, no host libz. */
uint32_t wubu_adler32(const uint8_t *data, uint32_t len) {
    uint32_t a = 1, b = 0;
    const uint32_t MOD = 65521;
    for (uint32_t i = 0; i < len; i++) {
        a = (a + data[i]) % MOD;
        b = (b + a) % MOD;
    }
    return (b << 16) | a;
}
