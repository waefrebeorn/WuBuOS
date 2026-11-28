/* wubu_mp3.h -- MPEG-1/2/2.5 Layer III (MP3) frame layer, ours.
 *
 * Own implementation (docs/GOAL-SLERM.json, SLERM-2007). Stage 1 covers the CONTAINER: syncing
 * to the first frame, walking every frame, and reporting what the stream actually is -- version,
 * sample rate, channels, bitrate, frame count and duration. That is the part a file manager needs
 * to describe an audio file, and the part an independent decoder (ffprobe) can referee exactly.
 *
 * Stage 2 (Huffman, requantisation, IMDCT, polyphase synthesis) is NOT here yet; nothing in this
 * API pretends to return samples.
 */
#ifndef WUBU_MP3_H
#define WUBU_MP3_H

#include <stddef.h>
#include <stdint.h>

enum {
    WUBU_MP3_OK          =  0,
    WUBU_MP3_ARGS        = -1,
    WUBU_MP3_FORMAT      = -2,   /* no frame sync anywhere in the first window */
    WUBU_MP3_TRUNCATED   = -3,   /* a frame header claims more bytes than remain */
    WUBU_MP3_UNSUPPORTED = -4    /* a layer other than III, or a reserved field */
};

typedef struct {
    int version;            /* 1, 2 or 25 (MPEG-2.5) */
    int layer;              /* always 3 */
    int sample_rate;        /* Hz */
    int channels;           /* 1 or 2 */
    int samples_per_frame;  /* 1152 for MPEG-1, 576 for MPEG-2/2.5 */
    int first_bitrate_kbps;
    int avg_bitrate_kbps;
    int frames;             /* MPEG frames walked, INCLUDING a Xing/Info metadata frame */
    int audio_frames;       /* frames that carry audio: `frames` minus the Xing/Info frame */
    int has_xing;           /* the first frame is a Xing/Info tag (metadata, not audio) */
    int duration_ms;
    int vbr;                /* a Xing/Info header was found, or the bitrate varies */
    int has_id3v2;
    int first_frame_offset;  /* where the walk actually starts (after ID3v2, junk, padding) */
    int has_id3v1;
} WubuMp3;

/* Walk the stream and describe it. Reads the whole buffer (frame lengths must be followed), but
 * allocates nothing. */
int wubu_mp3_info(const void *data, size_t len, WubuMp3 *info, int *err);

/* ---- stage 2a: side information ------------------------------------------------------------ */
/* One granule of one channel, as the frame declares it. Nothing here is decoded into samples yet;
 * these are the numbers the Huffman and requantisation stages will consume. */
typedef struct {
    int part2_3_length;
    int big_values;
    int global_gain;
    int scalefac_compress;
    int window_switching;
    int block_type;          /* 0,1 long; 2 short; 3 start */
    int mixed_block_flag;
    int table_select[3];
    int subblock_gain[3];
    int region0_count, region1_count;
    int preflag, scalefac_scale, count1table_select;
} WubuMp3Granule;

typedef struct {
    int main_data_begin;
    int scfsi[2][4];         /* [channel][band] -- MPEG-1 long-block scalefactor sharing */
    WubuMp3Granule gr[2][2]; /* [granule][channel]; MPEG-2 uses granule 0 only */
    int granules;            /* 2 for MPEG-1, 1 for MPEG-2/2.5 */
    int side_info_bytes;     /* the PUBLISHED figure: 17/32 (MPEG-1), 9/17 (MPEG-2) bytes */
    int side_info_bits;      /* the real bit count, which is NOT bytes*8 for MPEG-2:
                              * 136/256 for MPEG-1 mono/stereo (exact), 73/138 for MPEG-2.
                              * The published byte counts are rounded DOWN there, so main data
                              * starts mid-byte. Asserting bytes*8 made every MPEG-2 frame fail. */
    int bits_consumed;       /* what our reader actually consumed -- must equal side_info_bytes*8 */
} WubuMp3Side;

/* Describe the frame whose header starts at `offset`: its byte length and stream parameters.
 * Exposed because a player must step frames by their own lengths, and because a test that
 * re-implements this arithmetic is a second decoder pretending to be an oracle. */
int wubu_mp3_frame_header(const void *data, size_t len, size_t offset, int *frame_bytes,
                          int *version, int *sample_rate, int *channels, int *err);

/* Parse the side information of the frame starting at `offset` (an absolute file offset that must
 * point at a valid frame header). `frame_bytes` is the frame's length. */
int wubu_mp3_side_info(const void *data, size_t len, size_t offset, int frame_bytes,
                       WubuMp3Side *side, int *err);

const char *wubu_mp3_reason(int err);

#endif
