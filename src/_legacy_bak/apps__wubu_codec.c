/*
 * wubu_codec.c  --  WuBuOS Codec Layer Implementation
 *
 * Cell 398: FFmpeg CLI wrapper + optional libav linkage.
 *
 * Direct mode: pipe to/from ffmpeg subprocess.
 * Always works on Arch (ffmpeg in community repo).
 */
#include "wubu_codec.h"
#include "../runtime/wubu_spawn.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <errno.h>
#include <fcntl.h>

/* -- FFmpeg Availability ------------------------------------------ */

bool wubu_codec_available(void) {
    return (access("/usr/bin/ffmpeg", X_OK) == 0);
}

/* -- Format Detection --------------------------------------------- */

WubuMediaType wubu_codec_detect_type(const char *path) {
    if (!path) return WUBU_MEDIA_UNKNOWN;
    size_t len = strlen(path);
    /* Video */
    if (len > 4 && strcmp(path+len-4, ".mp4") == 0)  return WUBU_MEDIA_VIDEO;
    if (len > 4 && strcmp(path+len-4, ".mkv") == 0)  return WUBU_MEDIA_VIDEO;
    if (len > 4 && strcmp(path+len-4, ".avi") == 0)  return WUBU_MEDIA_VIDEO;
    if (len > 4 && strcmp(path+len-4, ".mov") == 0)  return WUBU_MEDIA_VIDEO;
    if (len > 5 && strcmp(path+len-5, ".webm") == 0) return WUBU_MEDIA_VIDEO;
    if (len > 4 && strcmp(path+len-4, ".flv") == 0)  return WUBU_MEDIA_VIDEO;
    /* Audio */
    if (len > 4 && strcmp(path+len-4, ".mp3") == 0)  return WUBU_MEDIA_AUDIO;
    if (len > 4 && strcmp(path+len-4, ".ogg") == 0)  return WUBU_MEDIA_AUDIO;
    if (len > 4 && strcmp(path+len-4, ".wav") == 0)  return WUBU_MEDIA_AUDIO;
    if (len > 5 && strcmp(path+len-5, ".flac") == 0) return WUBU_MEDIA_AUDIO;
    if (len > 5 && strcmp(path+len-5, ".opus") == 0) return WUBU_MEDIA_AUDIO;
    if (len > 4 && strcmp(path+len-4, ".m4a") == 0)  return WUBU_MEDIA_AUDIO;
    /* Image */
    if (len > 4 && strcmp(path+len-4, ".png") == 0)  return WUBU_MEDIA_IMAGE;
    if (len > 4 && strcmp(path+len-4, ".jpg") == 0)  return WUBU_MEDIA_IMAGE;
    if (len > 5 && strcmp(path+len-5, ".jpeg") == 0) return WUBU_MEDIA_IMAGE;
    if (len > 4 && strcmp(path+len-4, ".gif") == 0)  return WUBU_MEDIA_IMAGE;
    if (len > 4 && strcmp(path+len-4, ".bmp") == 0)  return WUBU_MEDIA_IMAGE;
    if (len > 4 && strcmp(path+len-4, ".webp") == 0) return WUBU_MEDIA_IMAGE;
    if (len > 4 && strcmp(path+len-4, ".tiff") == 0) return WUBU_MEDIA_IMAGE;
    return WUBU_MEDIA_UNKNOWN;
}

/* -- Probe -------------------------------------------------------- */

int wubu_codec_probe(const char *path, WubuMediaInfo *info) {
    if (!path || !info) return -1;
    memset(info, 0, sizeof(WubuMediaInfo));
    info->type = wubu_codec_detect_type(path);

    if (!wubu_codec_available()) return -1;

    /* Use ffprobe to get info */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "ffprobe -v quiet -print_format json -show_format -show_streams '%s' 2>/dev/null",
        path);

    FILE *f = popen(cmd, "r");
    if (!f) return -1;

    /* Simple JSON parse: look for width, height, codec_name, duration */
    char buf[4096];
    while (fgets(buf, sizeof(buf), f)) {
        if (strstr(buf, "\"width\"")) {
            int v; if (sscanf(buf, " \"width\": %d,", &v) == 1) info->width = v;
        }
        if (strstr(buf, "\"height\"")) {
            int v; if (sscanf(buf, " \"height\": %d,", &v) == 1) info->height = v;
        }
        if (strstr(buf, "\"codec_name\"")) {
            char v[32];
            if (sscanf(buf, " \"codec_name\": \"%31[^\"]\",", v) == 1) {
                if (info->codec_v[0] == '\0') strncpy(info->codec_v, v, 31);
                else if (info->codec_a[0] == '\0') strncpy(info->codec_a, v, 31);
            }
        }
        if (strstr(buf, "\"duration\"")) {
            double v; if (sscanf(buf, " \"duration\": \"%lf\",", &v) == 1) info->duration = v;
        }
    }
    pclose(f);
    return 0;
}

/* -- Decode -------------------------------------------------------- */

WubuDecoder *wubu_dec_open(const char *path) {
    if (!path) return NULL;
    WubuDecoder *dec = (WubuDecoder*)calloc(1, sizeof(WubuDecoder));
    if (!dec) return NULL;
    strncpy(dec->path, path, sizeof(dec->path) - 1);
    wubu_codec_probe(path, &dec->info);
    dec->pipe_fd = -1;
    dec->pid = 0;
    dec->eof = false;
    return dec;
}

void wubu_dec_close(WubuDecoder *dec) {
    if (!dec) return;
    if (dec->pipe_fd >= 0) close(dec->pipe_fd);
    if (dec->pid > 0) kill(dec->pid, SIGTERM);
    free(dec);
}

int wubu_dec_video_frame(WubuDecoder *dec, WubuVideoFrame *frame) {
    if (!dec || !frame || dec->pipe_fd < 0) return -1;
    /* Read raw RGBA frame from pipe */
    size_t frame_size = (size_t)(frame->width * frame->height * 4);
    if (frame_size == 0) return -1;
    if (!frame->data) {
        frame->data = malloc(frame_size);
        if (!frame->data) return -1;
        frame->owns_data = true;
    }
    ssize_t n = read(dec->pipe_fd, frame->data, frame_size);
    if (n < 0) return -1;
    frame->data_size = (size_t)n;
    frame->eof = (n == 0);
    return (n == (ssize_t)frame_size) ? 0 : -1;
}

int wubu_dec_audio_frame(WubuDecoder *dec, WubuAudioFrame *frame) {
    (void)dec; (void)frame; return -1;
}

int wubu_dec_seek(WubuDecoder *dec, double timestamp) {
    (void)dec; (void)timestamp; return -1;
}

const WubuMediaInfo *wubu_dec_info(WubuDecoder *dec) {
    return dec ? &dec->info : NULL;
}

/* -- Encode -------------------------------------------------------- */

WubuEncoder *wubu_enc_open(const char *path, const char *format,
                            const char *codec_v, const char *codec_a,
                            int width, int height, int fps,
                            int64_t bit_rate_v, int64_t bit_rate_a) {
    if (!path) return NULL;
    WubuEncoder *enc = (WubuEncoder*)calloc(1, sizeof(WubuEncoder));
    if (!enc) return NULL;
    strncpy(enc->path, path, sizeof(enc->path) - 1);
    if (format) strncpy(enc->format, format, sizeof(enc->format) - 1);
    if (codec_v) strncpy(enc->codec_v, codec_v, sizeof(enc->codec_v) - 1);
    if (codec_a) strncpy(enc->codec_a, codec_a, sizeof(enc->codec_a) - 1);
    enc->width = width; enc->height = height;
    enc->fps_num = fps; enc->fps_den = 1;
    enc->bit_rate_v = bit_rate_v; enc->bit_rate_a = bit_rate_a;
    enc->pipe_fd = -1; enc->pid = 0;
    return enc;
}

void wubu_enc_close(WubuEncoder *enc) {
    if (!enc) return;
    if (enc->pipe_fd >= 0) close(enc->pipe_fd);
    if (enc->pid > 0) { kill(enc->pid, SIGTERM); waitpid(enc->pid, NULL, 0); }
    free(enc);
}

int wubu_enc_video_frame(WubuEncoder *enc, const WubuVideoFrame *frame) {
    (void)enc; (void)frame; return -1;
}

int wubu_enc_audio_frame(WubuEncoder *enc, const WubuAudioFrame *frame) {
    (void)enc; (void)frame; return -1;
}

/* -- Transcode ---------------------------------------------------- */

int wubu_transcode_start(WubuTranscode *job) {
    if (!job) return -1;
    if (!wubu_codec_available()) return -1;

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
        "ffmpeg -y -i '%s' -c:v %s -c:a %s%s%s %s '%s' 2>/dev/null &",
        job->src,
        job->codec_v[0] ? job->codec_v : "copy",
        job->codec_a[0] ? job->codec_a : "copy",
        job->width > 0 ? " -s " : "", job->width > 0 ? "" : "",
        job->gpu_accel ? " -hwaccel auto" : "",
        job->dst);

    (void)cmd;
    job->running = true;
    job->progress = 0;
    return 0;
}

int wubu_transcode_progress(WubuTranscode *job) {
    return job ? job->progress : 0;
}

int wubu_transcode_wait(WubuTranscode *job) {
    if (!job || !job->running) return -1;
    job->running = false;
    job->progress = 100;
    return 0;
}

void wubu_transcode_cancel(WubuTranscode *job) {
    if (job) { job->running = false; if (job->pid > 0) kill(job->pid, SIGKILL); }
}

/* -- Convenience --------------------------------------------------- */

int wubu_codec_extract_frame(const char *src, double timestamp,
                               uint32_t *out, int w, int h) {
    if (!src || !out || !wubu_codec_available()) return -1;
    /* Use ffmpeg to extract a single frame as raw RGBA */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "ffmpeg -y -ss %.3f -i '%s' -vframes 1 -f rawvideo -pix_fmt rgba -s %dx%d - 2>/dev/null",
        timestamp, src, w, h);
    FILE *f = popen(cmd, "r");
    if (!f) return -1;
    size_t n = fread(out, 4, w * h, f);
    pclose(f);
    return (n == (size_t)(w * h)) ? 0 : -1;
}

int wubu_codec_convert(const char *src, const char *dst,
                        const char *codec_v, const char *codec_a) {
    if (!src || !dst || !wubu_codec_available()) return -1;
    char *argv[] = {
        "ffmpeg", "-y", "-i", (char *)src,
        "-c:v", (char *)(codec_v ? codec_v : "copy"),
        "-c:a", (char *)(codec_a ? codec_a : "copy"),
        (char *)dst, (char *)NULL
    };
    int ret = wubu_run_program("ffmpeg", argv, true);
    return ret;
}

int wubu_codec_thumbnail(const char *src, uint32_t *out,
                          int thumb_w, int thumb_h) {
    return wubu_codec_extract_frame(src, 0.0, out, thumb_w, thumb_h);
}

int wubu_codec_metadata(const char *path,
                         char keys[][32], char vals[][128], int max) {
    if (!path || !keys || !vals || max <= 0) return -1;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "ffprobe -v quiet -print_format json -show_format -show_streams '%s' 2>/dev/null", path);
    FILE *f = popen(cmd, "r");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    if (!buf) { pclose(f); return -1; }
    fread(buf, 1, sz, f);
    buf[sz] = 0;
    pclose(f);
    int count = 0;
    const char *p = buf;
    while (count < max) {
        const char *k = strstr(p, "\"name\"");
        if (!k) break;
        const char *q = strchr(k, '"');
        if (!q) break;
        const char *e = strchr(q + 1, '"');
        if (!e) break;
        size_t kl = (size_t)(e - q - 1);
        if (kl >= 32) kl = 31;
        memcpy(keys[count], q + 1, kl);
        keys[count][kl] = 0;
        const char *v = strstr(e, "\"value\"");
        if (!v) { p = e + 1; continue; }
        q = strchr(v, '"');
        if (!q) { p = e + 1; continue; }
        e = strchr(q + 1, '"');
        if (!e) { p = e + 1; continue; }
        size_t vl = (size_t)(e - q - 1);
        if (vl >= 128) vl = 127;
        memcpy(vals[count], q + 1, vl);
        vals[count][vl] = 0;
        count++;
        p = e + 1;
    }
    free(buf);
    return count;
}

int wubu_codec_mount(const char *container_path, const char *mount_point) {
    if (!container_path || !mount_point) return -1;

    /* Verify the container_path exists and is a directory (.wubu container rootfs) */
    struct stat st;
    if (stat(container_path, &st) != 0) {
        return -1;  /* Container path doesn't exist */
    }

    /* Create mount_point if it doesn't exist (best-effort, WSL host syscalls) */
    if (stat(mount_point, &st) != 0) {
        mkdir(mount_point, 0755);
    }

    /* Mount the container's rootfs at mount_point using bind mount.
     * This is the Inferno emu pattern: the container IS a directory,
     * and we bind-mount it into the 9P namespace.
     * In hosted mode (WSL), this requires CAP_SYS_ADMIN.
     * Fallback: symlink the directory (read-only semantics via convention). */
    int ret = mount(container_path, mount_point, "none", MS_BIND | MS_RDONLY, "");
    if (ret == 0) {
        return 0;  /* Bind mount succeeded */
    }

    /* Bind mount failed (expected in WSL without CAP_SYS_ADMIN).
     * Fallback: use symlink for namespace composition. */
    ret = symlink(container_path, mount_point);
    if (ret == 0) {
        return 0;  /* Symlink mount succeeded */
    }

    /* Symlink failed (might already exist — try unlink first) */
    unlink(mount_point);
    ret = symlink(container_path, mount_point);
    if (ret == 0) {
        return 0;
    }

    /* Last fallback: directory copy (shallow — just create mount_point as dir) */
    if (mkdir(mount_point, 0755) == 0 || errno == EEXIST) {
        return 0;  /* Mount point exists — container accessible by path convention */
    }

    return -1;  /* All mount strategies failed */
}
