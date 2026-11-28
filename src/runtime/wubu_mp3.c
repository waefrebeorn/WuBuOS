/* wubu_mp3.c -- see wubu_mp3.h. MPEG-1/2/2.5 Layer III frame layer. */
#include "wubu_mp3.h"
#include <string.h>

/* Bitrate tables, kbps, indexed by the 4-bit field. Index 0 is "free format" and 15 is reserved;
 * both are refused rather than guessed at, because a wrong frame length desynchronises the walk. */
static const int kbps_v1_l3[15] = { 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 };
static const int kbps_v2_l3[15] = { 0,  8, 16, 24, 32, 40, 48, 56,  64,  80,  96, 112, 128, 144, 160 };

static const int sr_v1[3] = { 44100, 48000, 32000 };
static const int sr_v2[3] = { 22050, 24000, 16000 };
static const int sr_v25[3] = { 11025, 12000, 8000 };

const char *wubu_mp3_reason(int err) {
    switch (err) {
        case WUBU_MP3_OK:          return "ok";
        case WUBU_MP3_ARGS:        return "bad arguments";
        case WUBU_MP3_FORMAT:      return "not an MP3 stream (no frame sync found)";
        case WUBU_MP3_TRUNCATED:   return "the file ends inside a frame";
        case WUBU_MP3_UNSUPPORTED: return "not Layer III, or a reserved header field";
        default:                   return "invalid";
    }
}

/* ID3v2 is length-prefixed with four 7-bit bytes (a "syncsafe" integer). */
static size_t id3v2_size(const uint8_t *d, size_t len) {
    if (len < 10 || memcmp(d, "ID3", 3) != 0) return 0;
    size_t z = ((size_t)(d[6] & 0x7F) << 21) | ((size_t)(d[7] & 0x7F) << 14) |
               ((size_t)(d[8] & 0x7F) << 7)  |  (size_t)(d[9] & 0x7F);
    size_t total = 10 + z;
    if (d[5] & 0x10) total += 10;               /* footer present */
    return total <= len ? total : len;          /* a lying size must not walk off the buffer */
}

typedef struct {
    int version, sample_rate, channels, spf, bitrate_kbps, length;
} Header;

static int parse_header(const uint8_t *d, size_t len, size_t at, Header *h) {
    if (at + 4 > len) return WUBU_MP3_TRUNCATED;
    if (d[at] != 0xFF || (d[at + 1] & 0xE0) != 0xE0) return WUBU_MP3_FORMAT;
    int ver_bits = (d[at + 1] >> 3) & 3;          /* 00 = 2.5, 10 = 2, 11 = 1 */
    int layer    = (d[at + 1] >> 1) & 3;          /* 01 = Layer III */
    int br_idx   = (d[at + 2] >> 4) & 15;
    int sr_idx   = (d[at + 2] >> 2) & 3;
    int pad      = (d[at + 2] >> 1) & 1;
    int mode     = (d[at + 3] >> 6) & 3;

    if (ver_bits == 1 || layer != 1 || sr_idx == 3) return WUBU_MP3_UNSUPPORTED;
    if (br_idx == 0 || br_idx == 15) return WUBU_MP3_UNSUPPORTED;   /* free format / bad */

    h->version = (ver_bits == 3) ? 1 : (ver_bits == 2) ? 2 : 25;
    h->sample_rate = (ver_bits == 3) ? sr_v1[sr_idx] : (ver_bits == 2) ? sr_v2[sr_idx] : sr_v25[sr_idx];
    h->bitrate_kbps = (ver_bits == 3) ? kbps_v1_l3[br_idx] : kbps_v2_l3[br_idx];
    h->channels = (mode == 3) ? 1 : 2;
    h->spf = (ver_bits == 3) ? 1152 : 576;
    /* Layer III frame length: 144 * bitrate / rate for MPEG-1, 72 * for MPEG-2/2.5 (the frame is
     * half as long because there are 576 samples, not 1152). */
    int k = (ver_bits == 3) ? 144 : 72;
    h->length = k * h->bitrate_kbps * 1000 / h->sample_rate + pad;
    if (h->length < 4) return WUBU_MP3_UNSUPPORTED;
    return WUBU_MP3_OK;
}

/* A Xing/Info tag (VBR) or VBRI tag can only live in the FIRST frame, so look there and nowhere
 * else -- a literal search of the whole file would match entropy data. */
static int first_frame_is_vbr(const uint8_t *d, size_t len, size_t at, int frame_len) {
    size_t end = at + (size_t)frame_len;
    if (end > len) end = len;
    for (size_t i = at + 4; i + 4 <= end; i++)
        if (memcmp(d + i, "Xing", 4) == 0 || memcmp(d + i, "Info", 4) == 0) return 1;
    return 0;
}

int wubu_mp3_info(const void *data, size_t len, WubuMp3 *info, int *err) {
    if (err) *err = WUBU_MP3_OK;
    if (!data || !info || len < 4) { if (err) *err = WUBU_MP3_ARGS; return WUBU_MP3_ARGS; }
    const uint8_t *d = (const uint8_t *)data;
    memset(info, 0, sizeof *info);

    size_t skip = id3v2_size(d, len);
    info->has_id3v2 = skip > 0;

    /* Sync: walk forward a bounded distance, because ID3 padding and junk tags are normal. A
     * header that parses is not proof -- the next header must agree -- so both are checked. */
    size_t start = 0;
    int found = 0;
    for (size_t i = skip; i + 4 <= len && i < skip + 65536; i++) {
        Header a, b;
        if (parse_header(d, len, i, &a) != WUBU_MP3_OK) continue;
        if (i + (size_t)a.length + 4 <= len && parse_header(d, len, i + (size_t)a.length, &b) == WUBU_MP3_OK) {
            start = i; found = 1; break;
        }
        if (i + (size_t)a.length >= len) { start = i; found = 1; break; }   /* a one-frame file */
    }
    if (!found) { if (err) *err = WUBU_MP3_FORMAT; return WUBU_MP3_FORMAT; }

    info->first_frame_offset = (int)start;
    Header h;
    if (parse_header(d, len, start, &h) != WUBU_MP3_OK) { if (err) *err = WUBU_MP3_FORMAT; return WUBU_MP3_FORMAT; }
    info->version = h.version;
    info->layer = 3;
    info->sample_rate = h.sample_rate;
    info->channels = h.channels;
    info->samples_per_frame = h.spf;
    info->first_bitrate_kbps = h.bitrate_kbps;
    info->has_xing = first_frame_is_vbr(d, len, start, h.length);
    info->vbr = info->has_xing;

    /* Walk. Frame length comes from each frame's own header, which is what makes VBR work: the
     * step is per-frame, never an assumed constant. */
    size_t at = start;
    long total_kbps = 0;
    int frames = 0;
    while (at + 4 <= len) {
        Header f;
        if (parse_header(d, len, at, &f) != WUBU_MP3_OK) break;       /* trailing junk/ID3v1 */
        if (f.sample_rate != info->sample_rate || f.channels != info->channels) break;  /* a different stream */
        if (f.bitrate_kbps != info->first_bitrate_kbps) info->vbr = 1;
        if (at + (size_t)f.length > len) {                              /* a truncated last frame */
            if (err) *err = WUBU_MP3_TRUNCATED;
            break;
        }
        total_kbps += f.bitrate_kbps;
        frames++;
        at += (size_t)f.length;
    }
    if (len >= 128 && memcmp(d + len - 128, "TAG", 3) == 0) info->has_id3v1 = 1;
    if (frames == 0) { if (err) *err = WUBU_MP3_FORMAT; return WUBU_MP3_FORMAT; }

    info->frames = frames;
    /* The Xing/Info frame is a tag with a syntactically valid header: it counts as an MPEG frame
     * but carries no audio, and every player (and ffprobe's frame count) excludes it. */
    info->audio_frames = frames - (info->has_xing ? 1 : 0);
    info->avg_bitrate_kbps = (int)(total_kbps / frames);
    /* Duration from frames, not from the bitrate: a VBR stream's average says nothing per-frame. */
    info->duration_ms = (int)(((long long)info->audio_frames * info->samples_per_frame * 1000) / info->sample_rate);
    return WUBU_MP3_OK;
}

int wubu_mp3_frame_header(const void *data, size_t len, size_t offset, int *frame_bytes,
                          int *version, int *sample_rate, int *channels, int *err) {
    if (err) *err = WUBU_MP3_OK;
    Header h;
    int rc = parse_header((const uint8_t *)data, len, offset, &h);
    if (rc != WUBU_MP3_OK) { if (err) *err = rc; return rc; }
    if (frame_bytes) *frame_bytes = h.length;
    if (version) *version = h.version;
    if (sample_rate) *sample_rate = h.sample_rate;
    if (channels) *channels = h.channels;
    return WUBU_MP3_OK;
}

/* ---- stage 2a: side information ------------------------------------------------------------ */

/* MSB-first bit reader over a byte window. Deliberately tiny: side info is at most 32 bytes and
 * the Huffman stage will get its own reader over the main-data area. */
typedef struct { const uint8_t *p; size_t bits, pos; } SideBits;

static int sb_get(SideBits *b, int n) {
    int v = 0;
    for (int i = 0; i < n; i++) {
        if (b->pos >= b->bits) { b->pos++; continue; }        /* past the end reads as zero */
        size_t byte = b->pos >> 3, off = 7 - (b->pos & 7);
        v = (v << 1) | ((b->p[byte] >> off) & 1);
        b->pos++;
    }
    return v;
}

int wubu_mp3_side_info(const void *data, size_t len, size_t offset, int frame_bytes,
                       WubuMp3Side *side, int *err) {
    if (err) *err = WUBU_MP3_OK;
    if (!data || !side || offset + 4 > len || frame_bytes < 4) { if (err) *err = WUBU_MP3_ARGS; return WUBU_MP3_ARGS; }
    const uint8_t *d = (const uint8_t *)data;
    Header h;
    int rc = parse_header(d, len, offset, &h);
    if (rc != WUBU_MP3_OK) { if (err) *err = rc; return rc; }
    if (offset + 4 + (size_t)frame_bytes > len) { if (err) *err = WUBU_MP3_TRUNCATED; return WUBU_MP3_TRUNCATED; }

    int mpeg1 = (h.version == 1);
    int mono = (h.channels == 1);
    memset(side, 0, sizeof *side);
    side->granules = mpeg1 ? 2 : 1;
    /* The standard fixes these, and the walk below must land exactly on the declared size: 32/17
     * bytes for MPEG-1 (stereo/mono), 17/9 for MPEG-2. Getting a field width wrong shifts every
     * later field and the equality check catches it in the first frame. */
    side->side_info_bytes = mpeg1 ? (mono ? 17 : 32) : (mono ? 9 : 17);
    /* The exact bit count, derived from the layout: header + one granule's fields per channel. */
    {
        int nch = mono ? 1 : 2;
        int header_bits = (mpeg1 ? 9 : 8) + (mpeg1 ? (mono ? 5 : 3) : (mono ? 1 : 2)) + (mpeg1 ? 4 * nch : nch);
        int granule_bits = mpeg1 ? 59 : 63;
        side->side_info_bits = header_bits + side->granules * nch * granule_bits;
    }

    const uint8_t *p = d + offset + 4;
    size_t avail = (size_t)frame_bytes - 4;
    if (avail * 8 < (size_t)side->side_info_bits) { if (err) *err = WUBU_MP3_TRUNCATED; return WUBU_MP3_TRUNCATED; }
    SideBits b = { p, (size_t)side->side_info_bits, 0 };

    side->main_data_begin = sb_get(&b, mpeg1 ? 9 : 8);
    sb_get(&b, mpeg1 ? (mono ? 5 : 3) : (mono ? 1 : 2));      /* private bits */
    if (mpeg1) {
        for (int ch = 0; ch < (mono ? 1 : 2); ch++) {
            int v = sb_get(&b, 4);
            for (int band = 0; band < 4; band++) side->scfsi[ch][band] = (v >> (3 - band)) & 1;
        }
    } else {
        for (int ch = 0; ch < (mono ? 1 : 2); ch++) side->scfsi[ch][0] = sb_get(&b, 1);
    }

    for (int gr = 0; gr < side->granules; gr++) {
        for (int ch = 0; ch < (mono ? 1 : 2); ch++) {
            WubuMp3Granule *g = &side->gr[gr][ch];
            g->part2_3_length = sb_get(&b, 12);
            g->big_values = sb_get(&b, 9);
            if (g->big_values > 288) { if (err) *err = WUBU_MP3_UNSUPPORTED; return WUBU_MP3_UNSUPPORTED; }
            g->global_gain = sb_get(&b, 8);
            g->scalefac_compress = sb_get(&b, mpeg1 ? 4 : 9);
            g->window_switching = sb_get(&b, 1);
            if (g->window_switching) {
                /* Order matters: block_type precedes mixed_block_flag, and a switched granule
                 * carries NO region counts -- the regions are implied (8 and 36 for short blocks). */
                g->block_type = sb_get(&b, 2);
                if (g->block_type == 0) { if (err) *err = WUBU_MP3_UNSUPPORTED; return WUBU_MP3_UNSUPPORTED; }
                g->mixed_block_flag = sb_get(&b, 1);
                g->table_select[0] = sb_get(&b, 5);
                g->table_select[1] = sb_get(&b, 5);
                g->table_select[2] = 0;
                g->subblock_gain[0] = sb_get(&b, 3);
                g->subblock_gain[1] = sb_get(&b, 3);
                g->subblock_gain[2] = sb_get(&b, 3);
                g->region0_count = 8;
                g->region1_count = 36;
            } else {
                g->block_type = 0;
                g->table_select[0] = sb_get(&b, 5);
                g->table_select[1] = sb_get(&b, 5);
                g->table_select[2] = sb_get(&b, 5);
                g->region0_count = sb_get(&b, 4);
                g->region1_count = sb_get(&b, 3);
            }
            /* MPEG-1 writes preflag; MPEG-2 derives it from the scalefactor-compress value. */
            g->preflag = mpeg1 ? sb_get(&b, 1) : (g->scalefac_compress >= 500);
            g->scalefac_scale = sb_get(&b, 1);
            g->count1table_select = sb_get(&b, 1);
        }
    }
    side->bits_consumed = (int)b.pos;
    return WUBU_MP3_OK;
}
