/* wubu_inflate.c -- see wubu_inflate.h. One pass, no allocation, no globals. */
#include "wubu_inflate.h"
#include <string.h>
#ifdef WUBU_INFLATE_DEBUG
#include <stdio.h>
#define DBG(fmt, ...) fprintf(stderr, "[inflate] " fmt "\n", __VA_ARGS__)
#else
#define DBG(fmt, ...) ((void)0)
#endif

#define MAXBITS 15
#define MAXLCODES 286
#define MAXDCODES 30
#define FIXLCODES 288

typedef struct {
    const unsigned char *p;
    size_t len, pos;
    unsigned buf;
    int cnt;
} Bits;

typedef struct {
    short count[MAXBITS + 1];
    short symbol[FIXLCODES];
} Huff;

static int need(Bits *b, int n) {
    while (b->cnt < n) {
        if (b->pos >= b->len) return -1;
        b->buf |= (unsigned)b->p[b->pos++] << b->cnt;
        b->cnt += 8;
    }
    return 0;
}

static int bits(Bits *b, int n, unsigned *out) {
    if (need(b, n)) return -1;
    *out = b->buf & ((1u << n) - 1u);
    b->buf >>= n;
    b->cnt -= n;
    return 0;
}

static int decode(Bits *b, const Huff *h) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len <= MAXBITS; len++) {
        unsigned bit;
        if (bits(b, 1, &bit)) return -1;
        code |= (int)bit;
        int count = h->count[len];
        if (code - count < first) return h->symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return b->pos >= b->len && b->cnt < 1 ? -1 : -3;   /* -1 truncated, -3 no such code */
}

/* Canonical Huffman construction (RFC 1951 3.2.2). Returns:
 * 0 ok, 1 incomplete (allowed for distance codes only), -1 over-subscribed. */
static int build(Huff *h, const short *length, int n) {
    memset(h->count, 0, sizeof h->count);
    for (int s = 0; s < n; s++) h->count[length[s]]++;
    if (h->count[0] == n) return 0;
    int left = 1;
    for (int len = 1; len <= MAXBITS; len++) {
        left <<= 1;
        left -= h->count[len];
        if (left < 0) return -1;
    }
    short offs[MAXBITS + 1];
    offs[1] = 0;
    for (int len = 1; len < MAXBITS; len++) offs[len + 1] = (short)(offs[len] + h->count[len]);
    for (int s = 0; s < n; s++)
        if (length[s]) h->symbol[offs[length[s]]++] = (short)s;
    return left;
}

static const short lenbase[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
static const short lenext[29]  = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
static const short distbase[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,32768};
static const short distext[30]  = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};

static int codes(Bits *b, const Huff *lc, const Huff *dc,
                 unsigned char *dst, size_t cap, size_t *out) {
    for (;;) {
        int sym = decode(b, lc);
        if (sym < 0) { DBG("litlen decode failed at pos=%zu sym=%d", b->pos, sym); return sym == -1 ? WUBU_INFLATE_TRUNCATED : WUBU_INFLATE_BAD_CODE; }
        if (sym < 256) {
            if (*out >= cap) return WUBU_INFLATE_OVERFLOW;
            dst[(*out)++] = (unsigned char)sym;
            continue;
        }
        if (sym == 256) return 0;
        sym -= 257;
        if (sym >= 29) return WUBU_INFLATE_BAD_CODE;
        unsigned extra;
        if (lenext[sym] && bits(b, lenext[sym], &extra)) { DBG("len extra bits failed sym=%d out=%zu pos=%zu", sym, *out, b->pos); return WUBU_INFLATE_TRUNCATED; }
        size_t len = (size_t)lenbase[sym] + (lenext[sym] ? extra : 0);
        int dsym = decode(b, dc);
        if (dsym < 0) { DBG("dist decode failed at pos=%zu sym=%d out=%zu", b->pos, dsym, *out); return dsym == -1 ? WUBU_INFLATE_TRUNCATED : WUBU_INFLATE_BAD_CODE; }
        if (dsym >= 30) return WUBU_INFLATE_BAD_CODE;
        if (distext[dsym] && bits(b, distext[dsym], &extra)) { DBG("dist extra bits failed dsym=%d out=%zu pos=%zu", dsym, *out, b->pos); return WUBU_INFLATE_TRUNCATED; }
        size_t dist = (size_t)distbase[dsym] + (distext[dsym] ? extra : 0);
        if (dist > *out) { DBG("dist %zu beyond out %zu at pos=%zu", dist, *out, b->pos); return WUBU_INFLATE_BAD_DIST; }
        if (*out + len > cap) return WUBU_INFLATE_OVERFLOW;
        for (size_t i = 0; i < len; i++, (*out)++) dst[*out] = dst[*out - dist];
    }
}

static int fixed_tables(Huff *lc, Huff *dc) {
    short l[FIXLCODES];
    int s = 0;
    for (; s < 144; s++) l[s] = 8;
    for (; s < 256; s++) l[s] = 9;
    for (; s < 280; s++) l[s] = 7;
    for (; s < FIXLCODES; s++) l[s] = 8;
    if (build(lc, l, FIXLCODES) < 0) return -1;
    short d[MAXDCODES];
    for (s = 0; s < MAXDCODES; s++) d[s] = 5;
    return build(dc, d, MAXDCODES) < 0 ? -1 : 0;
}

static int dynamic_tables(Bits *b, Huff *lc, Huff *dc) {
    static const short order[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
    unsigned hlit, hdist, hclen, v;
    if (bits(b, 5, &hlit) || bits(b, 5, &hdist) || bits(b, 4, &hclen)) return WUBU_INFLATE_TRUNCATED;
    int nlen = (int)hlit + 257, ndist = (int)hdist + 1, ncode = (int)hclen + 4;
    if (nlen > MAXLCODES || ndist > MAXDCODES) return WUBU_INFLATE_BAD_CODE;
    short lengths[MAXLCODES + MAXDCODES];
    memset(lengths, 0, sizeof lengths);
    for (int i = 0; i < ncode; i++) {
        if (bits(b, 3, &v)) return WUBU_INFLATE_TRUNCATED;
        lengths[order[i]] = (short)v;
    }
    Huff cl;
    int rc = build(&cl, lengths, 19);
    DBG("hdr hlit=%u hdist=%u hclen=%u code_len_incomplete=%d", hlit, hdist, hclen, rc);
    if (rc < 0) return WUBU_INFLATE_BAD_CODE;
    memset(lengths, 0, sizeof lengths);
    int idx = 0;
    while (idx < nlen + ndist) {
        int sym = decode(b, &cl);
        if (sym < 0) { DBG("code-length decode failed idx=%d pos=%zu", idx, b->pos); return sym == -1 ? WUBU_INFLATE_TRUNCATED : WUBU_INFLATE_BAD_CODE; }
        if (sym < 16) { lengths[idx++] = (short)sym; continue; }
        int rep, val = 0;
        if (sym == 16) {
            if (idx == 0) return WUBU_INFLATE_BAD_CODE;
            val = lengths[idx - 1];
            if (bits(b, 2, &v)) return WUBU_INFLATE_TRUNCATED;
            rep = 3 + (int)v;
        } else if (sym == 17) {
            if (bits(b, 3, &v)) return WUBU_INFLATE_TRUNCATED;
            rep = 3 + (int)v;
        } else {
            if (bits(b, 7, &v)) return WUBU_INFLATE_TRUNCATED;
            rep = 11 + (int)v;
        }
        if (idx + rep > nlen + ndist) return WUBU_INFLATE_BAD_CODE;
        while (rep--) lengths[idx++] = (short)val;
    }
    DBG("lens done idx=%d nlen=%d ndist=%d eob=%d first=%d", idx, nlen, ndist, lengths[256], lengths[0]);
    if (lengths[256] == 0) return WUBU_INFLATE_BAD_CODE;
    if (build(lc, lengths, nlen) < 0) return WUBU_INFLATE_BAD_CODE;
    if (build(dc, lengths + nlen, ndist) < 0) return WUBU_INFLATE_BAD_CODE;
    return 0;
}

static int raw_inflate(const unsigned char *src, size_t srclen,
                       unsigned char *dst, size_t cap, size_t *out, size_t *consumed) {
    Bits b = { src, srclen, 0, 0, 0 };
    int last = 0;
    while (!last) {
        unsigned v;
        if (bits(&b, 1, &v)) return WUBU_INFLATE_TRUNCATED;
        last = (int)v;
        if (bits(&b, 2, &v)) { DBG("block type read failed pos=%zu len=%zu out=%zu", b.pos, b.len, *out); return WUBU_INFLATE_TRUNCATED; }
        DBG("block last=%d type=%u pos=%zu out=%zu", last, v, b.pos, *out);
        if (v == 0) {
            b.buf = 0; b.cnt = 0;                       /* stored: discard to byte boundary */
            if (b.pos + 4 > b.len) return WUBU_INFLATE_TRUNCATED;
            unsigned len = (unsigned)b.p[b.pos] | ((unsigned)b.p[b.pos + 1] << 8);
            unsigned nlen = (unsigned)b.p[b.pos + 2] | ((unsigned)b.p[b.pos + 3] << 8);
            b.pos += 4;
            if ((len ^ 0xFFFFu) != nlen) return WUBU_INFLATE_BAD_BLOCK;
            if (b.pos + len > b.len) return WUBU_INFLATE_TRUNCATED;
            if (*out + len > cap) return WUBU_INFLATE_OVERFLOW;
            memcpy(dst + *out, b.p + b.pos, len);
            b.pos += len; *out += len;
            continue;
        }
        Huff lc, dc;
        if (v == 1) {
            if (fixed_tables(&lc, &dc)) return WUBU_INFLATE_BAD_CODE;
        } else if (v == 2) {
            int rc = dynamic_tables(&b, &lc, &dc);
            if (rc) return rc;
        } else {
            return WUBU_INFLATE_BAD_BLOCK;
        }
        int rc = codes(&b, &lc, &dc, dst, cap, out);
        if (rc) return rc;
    }
    if (consumed) *consumed = b.pos - (size_t)(b.cnt / 8);   /* whole bytes the stream really needed */
    return 0;
}

unsigned long wubu_adler32(const unsigned char *p, size_t n) {
    unsigned long a = 1, b = 0;
    for (size_t i = 0; i < n; i++) {
        a = (a + p[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

static long do_inflate(const unsigned char *src, size_t srclen, int zlib,
                       unsigned char *dst, size_t dstcap, int *err) {
    size_t start = zlib ? 2 : 0, out = 0, consumed = 0;
    int rc = raw_inflate(src + start, srclen - start, dst, dstcap, &out, &consumed);
    if (rc) { *err = rc; return -1; }
    if (!zlib) return (long)out;
    if (srclen - start < 4) { *err = WUBU_INFLATE_TRUNCATED; return -1; }
    /* The trailer is the last four bytes. If the decoder needed bytes from that
     * region, the stream was cut short and the "trailer" is really payload. */
    if (consumed > srclen - start - 4) { *err = WUBU_INFLATE_TRUNCATED; return -1; }
    const unsigned char *t = src + start + (srclen - start - 4);
    unsigned long want = ((unsigned long)t[0] << 24) | ((unsigned long)t[1] << 16) |
                         ((unsigned long)t[2] << 8) | (unsigned long)t[3];
    if (wubu_adler32(dst, out) != want) { *err = WUBU_INFLATE_CHECKSUM; return -1; }
    return (long)out;
}

long wubu_inflate(const unsigned char *src, size_t srclen,
                  unsigned char *dst, size_t dstcap, int *err) {
    if (err) *err = WUBU_INFLATE_OK;
    if (!src || !dst || dstcap == 0 || srclen == 0) {
        if (err) *err = WUBU_INFLATE_ARGS;
        return -1;
    }
    int zlib = 0;
    if (srclen >= 2 && (src[0] & 0x0F) == 8 && (((src[0] << 8) | src[1]) % 31) == 0) {
        if (src[0] >> 4 > 7) { if (err) *err = WUBU_INFLATE_BAD_HEADER; return -1; }
        if (src[1] & 0x20)   { if (err) *err = WUBU_INFLATE_DICT;       return -1; }
        zlib = 1;
    }
    if (!zlib) return do_inflate(src, srclen, 0, dst, dstcap, err);

    /* ZIP entries hold RAW deflate, and roughly one raw stream in thirty-one opens
     * with two bytes that also satisfy the zlib header check. So a zlib attempt
     * that fails at ANY stage is retried as raw before the file is refused: a
     * false-positive header must not make a real archive unreadable. */
    int e1 = WUBU_INFLATE_OK;
    long n = do_inflate(src, srclen, 1, dst, dstcap, &e1);
    if (n >= 0) return n;
    int e2 = WUBU_INFLATE_OK;
    long n2 = do_inflate(src, srclen, 0, dst, dstcap, &e2);
    if (n2 >= 0) return n2;
    if (err) *err = e1;      /* the zlib reading is the more specific diagnosis */
    return -1;
}

const char *wubu_inflate_reason(int err) {
    switch (err) {
        case WUBU_INFLATE_OK:         return "ok";
        case WUBU_INFLATE_ARGS:       return "bad arguments";
        case WUBU_INFLATE_TRUNCATED:  return "that file is truncated";
        case WUBU_INFLATE_BAD_BLOCK:  return "damaged compressed block";
        case WUBU_INFLATE_BAD_CODE:   return "invalid Huffman code";
        case WUBU_INFLATE_BAD_DIST:   return "back-reference outside the data";
        case WUBU_INFLATE_OVERFLOW:   return "decompressed data is larger than expected";
        case WUBU_INFLATE_DICT:       return "this archive needs a preset dictionary";
        case WUBU_INFLATE_CHECKSUM:   return "checksum does not match the data";
        case WUBU_INFLATE_BAD_HEADER: return "not a valid compressed stream";
        default:                      return "invalid";
    }
}
