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
    int has_id3v1;
} WubuMp3;

/* Walk the stream and describe it. Reads the whole buffer (frame lengths must be followed), but
 * allocates nothing. */
int wubu_mp3_info(const void *data, size_t len, WubuMp3 *info, int *err);

const char *wubu_mp3_reason(int err);

#endif
