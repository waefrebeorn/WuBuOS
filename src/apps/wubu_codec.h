/*
 * wubu_codec.h  --  WuBuOS Codec Layer (FFmpeg integration)
 *
 * Cell 398: Universal media decode/encode API.
 *
 * FFmpeg provides: every codec. We wrap it in a C API
 * that containers can call. The codec layer IS a .wubu
 * container  --  Inferno distribution model.
 *
 * Codecs available (via system ffmpeg on Arch):
 *   - Video: H.264, H.265/HEVC, VP8, VP9, AV1, MPEG-4
 *   - Audio: AAC, MP3, Opus, Vorbis, FLAC, WAV/PCM
 *   - Container: MP4, WebM, MKV, OGG, FLV, AVI, MOV
 *   - Image: PNG, JPEG, GIF, WebP, BMP, TIFF
 *
 * The API is two-mode:
 *   1. Direct: ffmpeg CLI via pipe (always works on Arch)
 *   2. Libav:  libavcodec/libavformat linkage (faster, no fork)
 *
 * Direct mode is the default (no linking required).
 * Libav mode is opt-in at build time (-DHAVE_LIBAV).
 *
 * For containers: the codec layer runs as a .wubu container
 * with GPU passthrough (NVDEC/NVENC via /dev/dri).
 */
#ifndef WUBU_CODEC_H
#define WUBU_CODEC_H

#include <stdint.h>
#include <stdbool.h>

/* -- Codec Constants ----------------------------------------------- */

#define WUBU_CODEC_MAX_STREAMS   32
#define WUBU_CODEC_MAX_METADATA  64
#define WUBU_CODEC_PIPE_BUF     4096

/* -- Media Type ---------------------------------------------------- */

typedef enum {
    WUBU_MEDIA_UNKNOWN  = 0,
    WUBU_MEDIA_VIDEO    = 1,
    WUBU_MEDIA_AUDIO    = 2,
    WUBU_MEDIA_IMAGE    = 3,
    WUBU_MEDIA_SUBTITLE = 4,
    WUBU_MEDIA_DATA     = 5,
} WubuMediaType;

/* -- Format Detection ---------------------------------------------- */

typedef struct {
    char      format[32];     /* "mp4", "webm", "mkv", etc */
    char      codec_v[32];   /* Video codec: "h264", "vp9", etc */
    char      codec_a[32];   /* Audio codec: "aac", "opus", etc */
    WubuMediaType type;
    int       width, height;  /* Video dimensions */
    int       sample_rate;   /* Audio sample rate */
    int       channels;      /* Audio channels */
    double    duration;      /* Duration in seconds */
    int64_t   bit_rate;      /* Bit rate */
    int       n_streams;     /* Total streams */
} WubuMediaInfo;

/* -- Video Frame -------------------------------------------------- */

typedef struct {
    uint32_t *pixels;        /* XRGB8888 frame data */
    int       width, height;
    double    pts;           /* Presentation timestamp */
    double    duration;      /* Frame duration */
    int       key_frame;     /* Is this a key frame? */
} WubuVideoFrame;

/* -- Audio Frame -------------------------------------------------- */

typedef struct {
    int16_t  *samples;       /* Interleaved PCM samples */
    int       n_samples;     /* Sample count (per channel) */
    int       channels;
    int       sample_rate;
    double    pts;
} WubuAudioFrame;

/* -- Decode Context ------------------------------------------------ */

typedef struct {
    char           path[512];      /* Source file path */
    WubuMediaInfo  info;           /* Detected format info */
    int            pipe_fd;        /* Pipe to ffmpeg process (direct mode) */
    int            pid;            /* ffmpeg child PID */
    bool           eof;
    bool           use_libav;      /* libav mode vs direct mode */
    void          *av_ctx;         /* Opaque libav context (if use_libav) */
} WubuDecoder;

/* -- Encode Context ------------------------------------------------ */

typedef struct {
    char           path[512];      /* Output file path */
    char           format[32];     /* Output format */
    char           codec_v[32];   /* Video codec */
    char           codec_a[32];   /* Audio codec */
    int            width, height;
    int            fps_num, fps_den;  /* Frame rate fraction */
    int            sample_rate;
    int            channels;
    int64_t        bit_rate_v;     /* Video bit rate */
    int64_t        bit_rate_a;     /* Audio bit rate */
    int            pipe_fd;        /* Pipe from ffmpeg process */
    int            pid;
    bool           use_libav;
    void          *av_ctx;
} WubuEncoder;

/* -- Transcode Job ------------------------------------------------ */

typedef struct {
    char      src[512];
    char      dst[512];
    char      codec_v[32];
    char      codec_a[32];
    char      format[32];
    int       width, height;    /* 0 = keep original */
    int64_t   bit_rate_v;
    int64_t   bit_rate_a;
    double    seek_start;       /* 0 = from beginning */
    double    seek_end;         /* 0 = to end */
    bool      gpu_accel;        /* Use GPU decode/encode */
    int       progress;         /* 0-100 */
    int       pid;              /* ffmpeg child PID */
    bool      running;
} WubuTranscode;

/* ==================================================================
 *  API: Format Detection
 * ================================================================== */

/* Probe a media file for format/codec info */
int  wubu_codec_probe(const char *path, WubuMediaInfo *info);

/* Detect media type from file extension */
WubuMediaType wubu_codec_detect_type(const char *path);

/* Check if ffmpeg is available on the system */
bool wubu_codec_available(void);

/* ==================================================================
 *  API: Decode (read media)
 * ================================================================== */

/* Open a media file for decoding */
WubuDecoder *wubu_dec_open(const char *path);
void         wubu_dec_close(WubuDecoder *dec);

/* Read next video frame (XRGB8888) */
int  wubu_dec_video_frame(WubuDecoder *dec, WubuVideoFrame *frame);

/* Read next audio frame (PCM) */
int  wubu_dec_audio_frame(WubuDecoder *dec, WubuAudioFrame *frame);

/* Seek to timestamp */
int  wubu_dec_seek(WubuDecoder *dec, double timestamp);

/* Get media info */
const WubuMediaInfo *wubu_dec_info(WubuDecoder *dec);

/* ==================================================================
 *  API: Encode (write media)
 * ================================================================== */

/* Open an encoder for writing */
WubuEncoder *wubu_enc_open(const char *path, const char *format,
                            const char *codec_v, const char *codec_a,
                            int width, int height, int fps,
                            int64_t bit_rate_v, int64_t bit_rate_a);
void         wubu_enc_close(WubuEncoder *enc);

/* Write a video frame */
int  wubu_enc_video_frame(WubuEncoder *enc, const WubuVideoFrame *frame);

/* Write an audio frame */
int  wubu_enc_audio_frame(WubuEncoder *enc, const WubuAudioFrame *frame);

/* ==================================================================
 *  API: Transcode (convert media)
 * ================================================================== */

/* Start a transcode job (runs ffmpeg in background) */
int  wubu_transcode_start(WubuTranscode *job);

/* Check transcode progress (0-100) */
int  wubu_transcode_progress(WubuTranscode *job);

/* Wait for transcode to finish */
int  wubu_transcode_wait(WubuTranscode *job);

/* Cancel a running transcode */
void wubu_transcode_cancel(WubuTranscode *job);

/* ==================================================================
 *  API: Convenience (one-shot operations)
 * ================================================================== */

/* Extract a single frame as XRGB8888 */
int  wubu_codec_extract_frame(const char *src, double timestamp,
                               uint32_t *out, int w, int h);

/* Convert any media to any format */
int  wubu_codec_convert(const char *src, const char *dst,
                          const char *codec_v, const char *codec_a);

/* Get video thumbnail (first key frame) */
int  wubu_codec_thumbnail(const char *src, uint32_t *out,
                            int thumb_w, int thumb_h);

/* Get media metadata (title, artist, etc) */
int  wubu_codec_metadata(const char *path,
                           char keys[][32], char vals[][128], int max);

/* Container codec access: mount a codec in a .wubu container */
int  wubu_codec_mount(const char *container_path, const char *mount_point);

#endif /* WUBU_CODEC_H */
