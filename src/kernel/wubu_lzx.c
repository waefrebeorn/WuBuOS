/*
 * wubu_lzx.c -- the IN-KERNEL LZX decompressor.
 *
 * Faithful port of the LZX algorithm from the ReactOS cabinet decoder
 * (reactos/dll/win32/cabinet/fdi.c -- itself derived from cabextract's
 * lzx.c by Stuart Caie, LGPL-2.1). Windows-isms replaced with kernel
 * native surface:
 *
 *   - mem_alloc / mem_free  (kernel heap, declared in libc.h)
 *   - uint32_t / uint8_t / uint16_t  (stdint)
 *   - 32-bit LE 16-bit-word bit reader (LZX_WINDOW_BITS=32)
 *   - make_decode_table() fast Huffman lookup (David Tritscher)
 *
 * The Intel E8 post-processing transform (relative->absolute CALL offsets
 * for x86) is preserved exactly: CAB LZX streams carry per-block offset
 * fix-ups, and skipping it would make halo.exe / the redist EXEs crash.
 *
 * Window size for LZX:21 is 1 << 21 = 2 MiB.
 *
 * CAB LZX framing (per ReactOS fdi.c): each CFDATA block is decompressed
 * independently. lzx_decompress() is called once per CFDATA block. The
 * decoder state (bitbuf, window, R0/R1/R2, header_read) persists across
 * calls, so the LZX bitstream flows continuously across CFDATA block
 * boundaries.
 */
#include "wubu_lzx.h"
#include "libc.h"
#include "memory.h"
#include <string.h>

#define LZX_NUM_CHARS          (256)
#define LZX_BLOCKTYPE_VERBATIM     (1)
#define LZX_BLOCKTYPE_ALIGNED      (2)
#define LZX_BLOCKTYPE_UNCOMPRESSED (3)
#define LZX_PRETREE_NUM_ELEMENTS   (20)
#define LZX_ALIGNED_NUM_ELEMENTS   (8)
#define LZX_NUM_PRIMARY_LENGTHS    (7)
#define LZX_NUM_SECONDARY_LENGTHS  (249)
#define LZX_MIN_MATCH             (2)

#define LZX_MAINTREE_MAXSYMBOLS (LZX_NUM_CHARS + (50u<<3))
#define LZX_MAINTREE_TABLEBITS  (12)
#define LZX_LENGTH_MAXSYMBOLS   (LZX_NUM_SECONDARY_LENGTHS+1)
#define LZX_LENGTH_TABLEBITS    (12)
#define LZX_ALIGNED_MAXSYMBOLS  (LZX_ALIGNED_NUM_ELEMENTS)
#define LZX_ALIGNED_TABLEBITS   (7)
#define LZX_PRETREE_MAXSYMBOLS  (LZX_PRETREE_NUM_ELEMENTS)
#define LZX_PRETREE_TABLEBITS   (6)
#define LZX_LENTABLE_SAFETY     (64)
#define LZX_WINDOW_BITS         (32)

/* position slot -> extra bits / base. 51 slots for a 2 MiB window. */
static const uint8_t lzx_extra_bits[51] = {
     0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
     7, 7, 8, 8, 9, 9, 10,10,11,11,12,12,13,13,14,14,
    15,15,16,16,17,17,17,17,17,17,17,17,17,17,17,17,
    17,17,17
};
static const uint32_t lzx_position_base[51] = {
          0, 1, 2,  3,   4,   6,   8,  12,  16,  24,  32,  48, 64, 96,128,192,
        256,384,512,768,1024,1536,2048,3072,4096,6144,8192,12288,16384,24576,32768,49152,
      65536,98304,131072,196608,262144,393216,524288,655360,786432,917504,1048576,1179648,
      1310720,1441792,1572864,1703936,1835008,1966080,2097152
};

/* ---- huffman fast-decode table (David Tritscher / cabextract) ---- */
static int lzx_make_decode_table(uint32_t nsyms, uint32_t nbits,
                                 const uint8_t *length, uint16_t *table) {
    uint32_t sym, leaf, fill, pos = 0;
    uint32_t table_mask = 1u << nbits;
    uint32_t bit_mask = table_mask >> 1;
    uint32_t next_symbol = bit_mask;
    uint8_t bit_num = 1;

    while (bit_num <= nbits) {
        for (sym = 0; sym < nsyms; sym++) {
            if (length[sym] == bit_num) {
                leaf = pos;
                if ((pos += bit_mask) > table_mask) return 1;
                fill = bit_mask;
                while (fill--) table[leaf++] = (uint16_t)sym;
            }
        }
        bit_mask >>= 1; bit_num++;
    }
    if (pos != table_mask) {
        for (sym = pos; sym < table_mask; sym++) table[sym] = 0;
        pos <<= 16; table_mask <<= 16; bit_mask = 1u << 15;
        while (bit_num <= 16) {
            for (sym = 0; sym < nsyms; sym++) {
                if (length[sym] == bit_num) {
                    leaf = pos >> 16;
                    for (fill = 0; fill < (uint32_t)(bit_num - nbits); fill++) {
                        if (table[leaf] == 0) {
                            table[(next_symbol<<1)] = 0;
                            table[(next_symbol<<1)+1] = 0;
                            table[leaf] = next_symbol++;
                        }
                        leaf = table[leaf] << 1;
                        if ((pos >> (15 - fill)) & 1) leaf++;
                    }
                    table[leaf] = (uint16_t)sym;
                    if ((pos += bit_mask) > table_mask) return 1; /* table overflow */
                }
            }
            bit_mask >>= 1; bit_num++;
        }
    }
    if (pos == table_mask) return 0;
    /* Underfilled code (Kraft sum < 1.0): fill remaining secondary entries
     * with 0. All symbols with length <= nbits were already placed in the
     * primary table. If no symbols have length > nbits, the code is valid
     * but underfilled — succeed. */
    for (sym = pos >> 16; sym < table_mask >> 16; sym++) table[sym] = 0;
    for (sym = 0; sym < nsyms; sym++) if (length[sym] > nbits) return 1;
    return 0;
}

/* decoder state (kernel-heap allocated, opaque) */
struct lzx_state {
    uint8_t  *window;
    uint32_t  window_size;
    uint32_t  window_posn;
    uint32_t  R0, R1, R2;
    uint16_t  main_elements;
    int       header_read;
    uint16_t  block_type;
    uint32_t  block_length;
    uint32_t  block_remaining;
    uint32_t  frames_read;
    int32_t   intel_filesize;
    int32_t   intel_curpos;
    int       intel_started;
    uint32_t  actual_size;
    int       debug_fail;

    /* bit-reader state persisted across lzx_decompress calls so that LZX
     * blocks spanning multiple CFDATA blocks keep their bitstream
     * continuity. ReactOS keeps these local because its caller
     * (fdi_decomp) accumulates CFDATA blocks into one buffer before
     * calling the decompressor; we call per-CFDATA-block from the
     * kernel, so we must persist them ourselves. */
    const uint8_t *br_ip;
    uint32_t      br_bitbuf;
    int           br_bitsleft;

    uint16_t  PRETREE_table[(1u<<LZX_PRETREE_TABLEBITS) + (LZX_PRETREE_MAXSYMBOLS<<1)];
    uint8_t   PRETREE_len [LZX_PRETREE_MAXSYMBOLS + LZX_LENTABLE_SAFETY];
    uint16_t  MAINTREE_table[(1u<<LZX_MAINTREE_TABLEBITS) + (LZX_MAINTREE_MAXSYMBOLS<<1)];
    uint8_t   MAINTREE_len [LZX_MAINTREE_MAXSYMBOLS + LZX_LENTABLE_SAFETY];
    uint16_t  LENGTH_table[(1u<<LZX_LENGTH_TABLEBITS) + (LZX_LENGTH_MAXSYMBOLS<<1)];
    uint8_t   LENGTH_len [LZX_LENGTH_MAXSYMBOLS + LZX_LENTABLE_SAFETY];
    uint16_t  ALIGNED_table[(1u<<LZX_ALIGNED_TABLEBITS) + (LZX_ALIGNED_MAXSYMBOLS<<1)];
    uint8_t   ALIGNED_len [LZX_ALIGNED_NUM_ELEMENTS + LZX_LENTABLE_SAFETY];
};

/* ---- 32-bit bit reader: LZX reads 16-bit LE words, MSB-first ---- */
typedef struct {
    const uint8_t *ip;
    const uint8_t *end;
    uint32_t bitbuf;
    int bitsleft;
} lzx_br;

#define LZX_INIT_BITSTREAM(br) do { (br)->bitsleft = 0; (br)->bitbuf = 0; } while (0)

/* ReactOS ENSURE_BITS reads LE16 words unconditionally (inbuf has +2 padding).
 * shift = (LZX_WINDOW_BITS - 16 - bitsleft) = 16 - bitsleft. When bitsleft
 * exceeds 16, the shift wraps mod 32 on x86, placing the word
 * at the correct overflow position. This is the same behavior ReactOS
 * relies on. */
static inline void lzx_ensure(lzx_br *br, int n) {
    while (br->bitsleft < n) {
        uint16_t w = (uint16_t)(br->ip[1] << 8 | br->ip[0]);
        unsigned sh = (unsigned)(LZX_WINDOW_BITS - 16u - br->bitsleft);
        br->bitbuf |= (uint32_t)w << sh;
        br->bitsleft += 16; br->ip += 2;
    }
}

static inline uint32_t lzx_read_bits(lzx_br *br, int n) {
    if (n == 0) return 0;
    lzx_ensure(br, n);
    uint32_t v = br->bitbuf >> (LZX_WINDOW_BITS - (uint32_t)n);
    br->bitbuf <<= n; br->bitsleft -= n;
    return v;
}

/* read code lengths for symbols [first,last) using the pretree */
static int lzx_read_lens(struct lzx_state *s, uint8_t *lens, uint32_t first,
                         uint32_t last, lzx_br *br) {
    uint32_t i, j, x, y; int z;
    uint16_t *hufftbl = s->PRETREE_table;

    for (x = 0; x < LZX_PRETREE_NUM_ELEMENTS; x++) {
        lzx_ensure(br, 4);
        y = br->bitbuf >> (LZX_WINDOW_BITS - 4u); br->bitbuf <<= 4; br->bitsleft -= 4;
        s->PRETREE_len[x] = (uint8_t)y;
    }
    /* Rebuild the pretree decode table after reading the 20 lengths. */
    if (lzx_make_decode_table(LZX_PRETREE_MAXSYMBOLS, LZX_PRETREE_TABLEBITS,
                              s->PRETREE_len, s->PRETREE_table)) return 1;
    for (x = first; x < last; ) {
        lzx_ensure(br, 16);
        i = hufftbl[br->bitbuf >> (LZX_WINDOW_BITS - LZX_PRETREE_TABLEBITS)];
        if (i >= LZX_PRETREE_MAXSYMBOLS) {
            uint32_t j2 = 1u << (LZX_WINDOW_BITS - LZX_PRETREE_TABLEBITS);
            do { j2 >>= 1; i <<= 1; i |= (br->bitbuf & j2) ? 1 : 0; if (!j2) return 1; }
            while ((i = hufftbl[i]) >= LZX_PRETREE_MAXSYMBOLS);
        }
        z = (int)i;
        j = s->PRETREE_len[z]; br->bitbuf <<= j; br->bitsleft -= j;
        if (z == 17) {
            lzx_ensure(br, 4); y = br->bitbuf >> (LZX_WINDOW_BITS-4u); br->bitbuf <<= 4; br->bitsleft -= 4;
            y += 4; while (y--) lens[x++] = 0;
        } else if (z == 18) {
            lzx_ensure(br, 5); y = br->bitbuf >> (LZX_WINDOW_BITS-5u); br->bitbuf <<= 5; br->bitsleft -= 5;
            y += 20; while (y--) lens[x++] = 0;
        } else if (z == 19) {
            lzx_ensure(br, 1); y = br->bitbuf >> (LZX_WINDOW_BITS-1u); br->bitbuf <<= 1; br->bitsleft -= 1;
            y += 4; lzx_ensure(br, 16);
            i = hufftbl[br->bitbuf >> (LZX_WINDOW_BITS - LZX_PRETREE_TABLEBITS)];
            if (i >= LZX_PRETREE_MAXSYMBOLS) {
                uint32_t j3 = 1u << (LZX_WINDOW_BITS - LZX_PRETREE_TABLEBITS);
                do { j3 >>= 1; i <<= 1; i |= (br->bitbuf & j3) ? 1 : 0; if (!j3) return 1; }
                while ((i = hufftbl[i]) >= LZX_PRETREE_MAXSYMBOLS);
            }
            z = (int)i; j = s->PRETREE_len[z]; br->bitbuf <<= j; br->bitsleft -= j;
            z = (int)lens[x] - z; if (z < 0) z += 17;
            while (y--) lens[x++] = (uint8_t)z;
        } else {
            z = (int)lens[x] - z; if (z < 0) z += 17;
            lens[x++] = (uint8_t)z;
        }
    }
    return 0;
}

/* decode huffman symbol via fast table + (possibly) bit-by-bit fallback */
static inline int lzx_huff_decode(struct lzx_state *s, uint16_t *table,
                                  uint8_t *lens, uint32_t maxsyms, uint32_t tablebits,
                                  lzx_br *br) {
    uint32_t i, j;
    lzx_ensure(br, 16);
    i = table[br->bitbuf >> (LZX_WINDOW_BITS - tablebits)];
    if (i >= maxsyms) {
        j = 1u << (LZX_WINDOW_BITS - tablebits);
        do { j >>= 1; i <<= 1; i |= (br->bitbuf & j) ? 1 : 0; if (!j) return -1; }
        while ((i = table[i]) >= maxsyms);
    }
    j = lens[i]; br->bitbuf <<= j; br->bitsleft -= j;
    return (int)i;
}

int lzx_decompress(struct lzx_state *s,
                   const uint8_t *in, uint32_t inlen,
                   uint8_t *out, uint32_t outlen) {
    const uint8_t *endinp = in + inlen;
    uint8_t *window = s->window;
    uint32_t window_posn = s->window_posn;
    uint32_t window_size = s->window_size;
    uint32_t R0 = s->R0, R1 = s->R1, R2 = s->R2;
    uint32_t match_offset, i, j, k;
    lzx_br br; br.end = endinp;
    /* ReactOS fdi_decomp calling convention: called per CFDATA block with
     * inpos = CAB(inbuf) (start of the CURRENT block's data) and
     * INIT_BITSTREAM (bitbuf/bitsleft zeroed) at every call. Only the LZX
     * block state (block_type/remaining, tables, window, R0/R1/R2) persists
     * across calls; the bit reader ALWAYS restarts at `in` with empty bitbuf. */
    br.ip = in;
    br.bitbuf = 0;
    br.bitsleft = 0;
    int togo = (int)outlen, this_run, main_element, aligned_bits;
    int match_length, length_footer, extra, verbatim_bits;
    int copy_length;
    uint8_t *rundest, *runsrc;

    /* read the one-shot intel E8 header */
    if (!s->header_read) {
        k = lzx_read_bits(&br, 1);
        i = 0; j = 0;
        if (k) { i = lzx_read_bits(&br, 16); j = lzx_read_bits(&br, 16); }
        s->intel_filesize = (int32_t)((i << 16) | j);
        s->header_read = 1;
    }

    while (togo > 0) {
        if (s->block_remaining == 0) {
            /* ReactOS: after an UNCOMPRESSED block, realign bitstream to a
             * 16-bit word boundary and reset the bit reader (INIT_BITSTREAM).
             * The next block header is then read from a fresh bit state. */
            if (s->block_type == LZX_BLOCKTYPE_UNCOMPRESSED) {
                if (s->block_length & 1) br.ip++; /* realign bitstream to word */
                br.bitbuf = 0; br.bitsleft = 0;
            }
            /* ReactOS reads the header with READ_BITS(3); READ_BITS(16);
             * READ_BITS(8) — each ensures only its own nbits. A leading
             * lzx_ensure(24) here would over-read a word when bitsleft is
             * 16..23, overflowing the 32-bit bitbuf and corrupting the
             * header (this caused len=66432 vs true 66529 for the UNCMP
             * block). lzx_read_bits() ensures internally, so no pre-fill. */
            s->block_type = (uint16_t)(lzx_read_bits(&br, 3) & 3);
            i = lzx_read_bits(&br, 16);
            j = lzx_read_bits(&br, 8);
            s->block_remaining = s->block_length = (i << 8) | j;
            if (s->block_type == 0 || s->block_type > 3) {
                s->debug_fail = 6;
                s->intel_curpos = (int32_t)(s->block_type << 24 | (i << 8) | j);
                return LZX_ILLEGALDATA;
            }

            switch (s->block_type) {
                case LZX_BLOCKTYPE_ALIGNED:
                    for (i = 0; i < 8; i++) {
                        lzx_ensure(&br, 3);
                        s->ALIGNED_len[i] = (uint8_t)lzx_read_bits(&br, 3);
                    }
                    if (lzx_make_decode_table(LZX_ALIGNED_MAXSYMBOLS, LZX_ALIGNED_TABLEBITS,
                                              s->ALIGNED_len, s->ALIGNED_table)) {
                        s->debug_fail = 9; return LZX_ILLEGALDATA;
                    }
                    /* fall through */
                case LZX_BLOCKTYPE_VERBATIM:
                    if (lzx_read_lens(s, s->MAINTREE_len, 0, 256, &br)) { s->debug_fail=1; return LZX_ILLEGALDATA; }
                    if (lzx_read_lens(s, s->MAINTREE_len, 256, s->main_elements, &br)) { s->debug_fail=2; return LZX_ILLEGALDATA; }
                    if (lzx_make_decode_table(s->main_elements, LZX_MAINTREE_TABLEBITS,
                                              s->MAINTREE_len, s->MAINTREE_table)) { s->debug_fail=3; return LZX_ILLEGALDATA; }
                    if (s->MAINTREE_len[0xE8] != 0) s->intel_started = 1;
                    if (lzx_read_lens(s, s->LENGTH_len, 0, LZX_NUM_SECONDARY_LENGTHS, &br)) { s->debug_fail=4; return LZX_ILLEGALDATA; }
                    if (lzx_make_decode_table(LZX_LENGTH_MAXSYMBOLS, LZX_LENGTH_TABLEBITS,
                                              s->LENGTH_len, s->LENGTH_table)) { s->debug_fail=5; return LZX_ILLEGALDATA; }
                    break;
                case LZX_BLOCKTYPE_UNCOMPRESSED:
                    s->intel_started = 1;
                    lzx_ensure(&br, 16);           /* get up to 16 pad bits into the buffer */
                    if (br.bitsleft > 16) br.ip -= 2; /* and align the bitstream! */
                    if ((uint32_t)(br.ip - in) + 12 > inlen) {
                        return LZX_ILLEGALDATA;
                    }
                    R0 = br.ip[0]|(br.ip[1]<<8)|(br.ip[2]<<16)|(br.ip[3]<<24); br.ip += 4;
                    R1 = br.ip[0]|(br.ip[1]<<8)|(br.ip[2]<<16)|(br.ip[3]<<24); br.ip += 4;
                    R2 = br.ip[0]|(br.ip[1]<<8)|(br.ip[2]<<16)|(br.ip[3]<<24); br.ip += 4;
                    break;
                default:
                    s->debug_fail = 6; return LZX_ILLEGALDATA;
            }
        }
        if (br.ip > endinp + 2) { s->debug_fail = 7; return LZX_ILLEGALDATA; }

        while ((this_run = (int)s->block_remaining) > 0 && togo > 0) {
            if (this_run > togo) this_run = togo;
            togo -= this_run; s->block_remaining -= (uint32_t)this_run;
            window_posn &= window_size - 1;
            if ((window_posn + (uint32_t)this_run) > window_size) return LZX_DATAFORMAT;

            if (s->block_type == LZX_BLOCKTYPE_VERBATIM) {
                while (this_run > 0) {
                    main_element = lzx_huff_decode(s, s->MAINTREE_table, s->MAINTREE_len,
                                                   s->main_elements, LZX_MAINTREE_TABLEBITS, &br);
                    if (main_element < 0) { s->debug_fail = 8; return LZX_ILLEGALDATA; }
                    if (main_element < LZX_NUM_CHARS) {
                        window[window_posn++] = (uint8_t)main_element; this_run--;
                    } else {
                        main_element -= LZX_NUM_CHARS;
                        match_length = main_element & LZX_NUM_PRIMARY_LENGTHS;
                        if (match_length == LZX_NUM_PRIMARY_LENGTHS) {
                            length_footer = lzx_huff_decode(s, s->LENGTH_table, s->LENGTH_len,
                                                            LZX_LENGTH_MAXSYMBOLS, LZX_LENGTH_TABLEBITS, &br);
                            if (length_footer < 0) return LZX_ILLEGALDATA;
                            match_length += length_footer;
                        }
                        match_length += LZX_MIN_MATCH;
                        match_offset = (uint32_t)(main_element >> 3);
                        if (match_offset > 2) {
                            if (match_offset != 3) {
                                extra = (int)lzx_extra_bits[match_offset];
                                verbatim_bits = (int)lzx_read_bits(&br, extra);
                                match_offset = lzx_position_base[match_offset] - 2 + (uint32_t)verbatim_bits;
                            } else match_offset = 1;
                            R2 = R1; R1 = R0; R0 = match_offset;
                        } else if (match_offset == 0) match_offset = R0;
                        else if (match_offset == 1) { match_offset = R1; R1 = R0; R0 = match_offset; }
                        else { match_offset = R2; R2 = R0; R0 = match_offset; }
                        rundest = window + window_posn; this_run -= match_length;
                        if (window_posn >= match_offset) runsrc = rundest - match_offset;
                        else {
                            runsrc = rundest + (window_size - match_offset);
                            copy_length = (int)(match_offset - window_posn);
                            if (copy_length < match_length) {
                                match_length -= copy_length; window_posn += (uint32_t)copy_length;
                                while (copy_length-- > 0) *rundest++ = *runsrc++;
                                runsrc = window;
                            }
                        }
                        window_posn += (uint32_t)match_length;
                        while (match_length-- > 0) *rundest++ = *runsrc++;
                    }
                }
            } else if (s->block_type == LZX_BLOCKTYPE_ALIGNED) {
                while (this_run > 0) {
                    main_element = lzx_huff_decode(s, s->MAINTREE_table, s->MAINTREE_len,
                                                   s->main_elements, LZX_MAINTREE_TABLEBITS, &br);
                    if (main_element < 0) { s->debug_fail = 8; return LZX_ILLEGALDATA; }
                    if (main_element < LZX_NUM_CHARS) {
                        window[window_posn++] = (uint8_t)main_element; this_run--;
                    } else {
                        main_element -= LZX_NUM_CHARS;
                        match_length = main_element & LZX_NUM_PRIMARY_LENGTHS;
                        if (match_length == LZX_NUM_PRIMARY_LENGTHS) {
                            length_footer = lzx_huff_decode(s, s->LENGTH_table, s->LENGTH_len,
                                                            LZX_LENGTH_MAXSYMBOLS, LZX_LENGTH_TABLEBITS, &br);
                            if (length_footer < 0) return LZX_ILLEGALDATA;
                            match_length += length_footer;
                        }
                        match_length += LZX_MIN_MATCH;
                        match_offset = (uint32_t)(main_element >> 3);
                        if (match_offset > 2) {
                            extra = (int)lzx_extra_bits[match_offset];
                            match_offset = lzx_position_base[match_offset] - 2;
                            if (extra > 3) {
                                extra -= 3;
                                verbatim_bits = (int)lzx_read_bits(&br, extra);
                                match_offset += (uint32_t)verbatim_bits << 3;
                                aligned_bits = lzx_huff_decode(s, s->ALIGNED_table, s->ALIGNED_len,
                                                               LZX_ALIGNED_MAXSYMBOLS, LZX_ALIGNED_TABLEBITS, &br);
                                if (aligned_bits < 0) return LZX_ILLEGALDATA;
                                match_offset += (uint32_t)aligned_bits;
                            } else if (extra == 3) {
                                aligned_bits = lzx_huff_decode(s, s->ALIGNED_table, s->ALIGNED_len,
                                                               LZX_ALIGNED_MAXSYMBOLS, LZX_ALIGNED_TABLEBITS, &br);
                                if (aligned_bits < 0) return LZX_ILLEGALDATA;
                                match_offset += (uint32_t)aligned_bits;
                            } else if (extra == 1 || extra == 2) {
                                verbatim_bits = (int)lzx_read_bits(&br, extra);
                                match_offset += (uint32_t)verbatim_bits;
                            } else { match_offset = 1; }
                            R2 = R1; R1 = R0; R0 = match_offset;
                        } else if (match_offset == 0) match_offset = R0;
                        else if (match_offset == 1) { match_offset = R1; R1 = R0; R0 = match_offset; }
                        else { match_offset = R2; R2 = R0; R0 = match_offset; }
                        rundest = window + window_posn; this_run -= match_length;
                        if (window_posn >= match_offset) runsrc = rundest - match_offset;
                        else {
                            runsrc = rundest + (window_size - match_offset);
                            copy_length = (int)(match_offset - window_posn);
                            if (copy_length < match_length) {
                                match_length -= copy_length; window_posn += (uint32_t)copy_length;
                                while (copy_length-- > 0) *rundest++ = *runsrc++;
                                runsrc = window;
                            }
                        }
                        window_posn += (uint32_t)match_length;
                        while (match_length-- > 0) *rundest++ = *runsrc++;
                    }
                }
            } else if (s->block_type == LZX_BLOCKTYPE_UNCOMPRESSED) {
                if ((uint32_t)(br.ip - in) + (uint32_t)this_run > inlen) {
                    return LZX_ILLEGALDATA;
                }
                memcpy(window + window_posn, br.ip, (size_t)this_run);
                br.ip += this_run; window_posn += (uint32_t)this_run;
            }
        }
    }

    if (togo != 0) return LZX_ILLEGALDATA;
    /* copy the tail of the window to out */
    uint32_t copy_from = window_posn ? window_posn : window_size;
    copy_from -= (uint32_t)outlen;
    memcpy(out, window + copy_from, (size_t)outlen);

    s->window_posn = window_posn; s->R0 = R0; s->R1 = R1; s->R2 = R2;
    /* NOTE: br (bit reader) state is NOT persisted across per-CFDA-block
     * calls. ReactOS INIT_BITSTREAM restarts it at inpos=in with zeroed
     * bitbuf every call. Only block tables, window, LRU registers, and
     * intel_curpos persist. (br_ip/br_bitbuf/br_bitsleft fields are kept
     * in the struct for ABI compat but left stale; never read at entry.)
     */

    /* Intel E8 post-processing: relative -> absolute CALL offsets */
    if ((s->frames_read++ < 32768) && s->intel_filesize != 0) {
        if (outlen <= 6 || !s->intel_started) {
            s->intel_curpos += outlen;
        } else {
            uint8_t *data = out; uint8_t *dataend = data + outlen - 10;
            int32_t curpos = s->intel_curpos; int32_t filesize = s->intel_filesize;
            int32_t abs_off, rel_off;
            s->intel_curpos = curpos + outlen;
            while (data < dataend) {
                if (*data++ != 0xE8) { curpos++; continue; }
                abs_off = data[0]|(data[1]<<8)|(data[2]<<16)|(data[3]<<24);
                if ((abs_off >= -curpos) && (abs_off < filesize)) {
                    rel_off = (abs_off >= 0) ? abs_off - curpos : abs_off + filesize;
                    data[0] = (uint8_t)rel_off; data[1] = (uint8_t)(rel_off>>8);
                    data[2] = (uint8_t)(rel_off>>16); data[3] = (uint8_t)(rel_off>>24);
                }
                data += 4; curpos += 5;
            }
        }
    }
    return LZX_OK;
}

struct lzx_state *lzx_init(uint32_t window_bytes) {
    struct lzx_state *s = mem_alloc(sizeof(*s));
    if (!s) return NULL;
    memset(s, 0, sizeof(*s));
    uint32_t posn_slots = 0; int i = 0;
    while (i < (int)window_bytes) { i += 1 << lzx_extra_bits[posn_slots++]; }
    s->window = mem_alloc(window_bytes);
    if (!s->window) { mem_free(s); return NULL; }
    s->actual_size = s->window_size = window_bytes;
    s->R0 = s->R1 = s->R2 = 1;
    s->main_elements = (uint16_t)(LZX_NUM_CHARS + (posn_slots << 3));
    s->header_read = 0; s->frames_read = 0; s->block_remaining = 0;
    s->intel_curpos = 0; s->intel_started = 0; s->window_posn = 0;
    s->br_ip = NULL; s->br_bitbuf = 0; s->br_bitsleft = 0;
    for (i = 0; i < LZX_MAINTREE_MAXSYMBOLS + LZX_LENTABLE_SAFETY; i++) s->MAINTREE_len[i] = 0;
    for (i = 0; i < LZX_LENGTH_MAXSYMBOLS + LZX_LENTABLE_SAFETY; i++) s->LENGTH_len[i] = 0;
    return s;
}

void lzx_free(struct lzx_state *s) {
    if (s) { if (s->window) mem_free(s->window); mem_free(s); }
}

int lzx_reset(struct lzx_state *s) {
    int i;
    s->R0 = s->R1 = s->R2 = 1;
    s->header_read = 0; s->frames_read = 0; s->block_remaining = 0;
    s->intel_curpos = 0; s->intel_started = 0; s->window_posn = 0;
    s->debug_fail = 0; s->br_ip = NULL; s->br_bitbuf = 0; s->br_bitsleft = 0;
    for (i = 0; i < LZX_MAINTREE_MAXSYMBOLS + LZX_LENTABLE_SAFETY; i++) s->MAINTREE_len[i] = 0;
    for (i = 0; i < LZX_LENGTH_MAXSYMBOLS + LZX_LENTABLE_SAFETY; i++) s->LENGTH_len[i] = 0;
    return 0;
}

int lzx_debug_fail(struct lzx_state *s) {
    return s ? s->debug_fail : -1;
}

uint32_t lzx_debug_state(struct lzx_state *s) {
    if (!s) return 0;
    return ((uint32_t)s->block_type << 28) | ((uint32_t)(s->br_bitsleft & 0xF) << 24) | (s->block_remaining & 0x1FFFF);
}

const char *lzx_strerror(int rc) {
    switch (rc) {
        case LZX_OK:          return "ok";
        case LZX_DATAFORMAT:  return "data format error";
        case LZX_ILLEGALDATA: return "illegal data";
        case LZX_NOMEM:       return "no memory";
        default:              return "unknown";
    }
}
