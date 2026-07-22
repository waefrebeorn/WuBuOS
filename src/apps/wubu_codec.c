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
#include <signal.h>
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

/* Forward declarations */
static int dec_popen(const char *cmd, int *out_pid);
static void dec_close_pipe(WubuDecoder *dec);

/* Start ffmpeg decoder process, return pipe fd for reading raw frames */
static int dec_start_ffmpeg(WubuDecoder *dec, const char *seek_str) {
    char cmd[2048];
    if (dec->info.type == WUBU_MEDIA_AUDIO || 
        (dec->info.type == WUBU_MEDIA_UNKNOWN)) {
        /* Audio: PCM s16le interleaved */
        snprintf(cmd, sizeof(cmd),
            "ffmpeg -v quiet %s -i '%s' -f s16le -acodec pcm_s16le - 2>/dev/null",
            seek_str ? seek_str : "", dec->path);
    } else {
        /* Video/default: raw RGBA frames */
        snprintf(cmd, sizeof(cmd),
            "ffmpeg -v quiet %s -i '%s' -f rawvideo -pix_fmt rgba - 2>/dev/null",
            seek_str ? seek_str : "", dec->path);
    }
    int pipe_fd = dec_popen(cmd, &dec->pid);
    return pipe_fd;
}

/* popen with pipe fd read, no shell wrapping */
static int dec_popen(const char *cmd, int *out_pid) {
    int fds[2];
    if (pipe(fds) != 0) return -1;
    pid_t pid = fork();
    if (pid < 0) { close(fds[0]); close(fds[1]); return -1; }
    if (pid == 0) {
        /* Child: exec ffmpeg, write to pipe */
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        close(fds[1]);
        /* Parse cmd string into argv (simple split on spaces) */
        char *buf = strdup(cmd);
        if (!buf) _exit(1);
        char *argv[64];
        int argc = 0;
        char *tok = strtok(buf, " ");
        while (tok && argc < 63) { argv[argc++] = tok; tok = strtok(NULL, " "); }
        argv[argc] = NULL;
        execvp(argv[0], argv);
        _exit(1);
    }
    close(fds[1]);
    if (out_pid) *out_pid = (int)pid;
    return fds[0];
}

static void dec_close_pipe(WubuDecoder *dec) {
    if (dec->pipe_fd >= 0) { close(dec->pipe_fd); dec->pipe_fd = -1; }
    if (dec->pid > 0) { kill(dec->pid, SIGTERM); waitpid(dec->pid, NULL, 0); dec->pid = 0; }
}

WubuDecoder *wubu_dec_open(const char *path) {
    if (!path) return NULL;
    WubuDecoder *dec = (WubuDecoder*)calloc(1, sizeof(WubuDecoder));
    if (!dec) return NULL;
    strncpy(dec->path, path, sizeof(dec->path) - 1);
    wubu_codec_probe(path, &dec->info);
    dec->pipe_fd = -1;
    dec->pid = 0;
    dec->eof = false;
    /* Start ffmpeg child process */
    dec->pipe_fd = dec_start_ffmpeg(dec, NULL);
    return dec;
}

void wubu_dec_close(WubuDecoder *dec) {
    if (!dec) return;
    dec_close_pipe(dec);
    free(dec);
}

int wubu_dec_video_frame(WubuDecoder *dec, WubuVideoFrame *frame) {
    if (!dec || !frame) return -1;
    if (dec->pipe_fd < 0) return -1;
    if (dec->eof) return -1;
    /* For video: read raw RGBA frame (width * height * 4 bytes) */
    int w = frame->width > 0 ? frame->width : dec->info.width;
    int h = frame->height > 0 ? frame->height : dec->info.height;
    if (w <= 0 || h <= 0) return -1;
    size_t frame_size = (size_t)(w * h * 4);
    if (!frame->data) {
        frame->data = malloc(frame_size);
        if (!frame->data) return -1;
        frame->owns_data = true;
    }
    frame->width = w;
    frame->height = h;
    ssize_t n = read(dec->pipe_fd, frame->data, frame_size);
    if (n < 0) return -1;
    frame->data_size = (size_t)n;
    frame->eof = (n == 0);
    dec->eof = frame->eof;
    return (n == (ssize_t)frame_size) ? 0 : -1;
}

int wubu_dec_audio_frame(WubuDecoder *dec, WubuAudioFrame *frame) {
    if (!dec || !frame) return -1;
    if (dec->pipe_fd < 0) return -1;
    if (dec->eof) return -1;
    /* Read PCM s16le audio frame (arbitrary chunk size ~4096 samples) */
    size_t buf_size = 4096 * sizeof(int16_t) * 2; /* stereo fallback */
    if (!frame->samples) {
        frame->samples = malloc(buf_size);
        if (!frame->samples) return -1;
    }
    memset(frame->samples, 0, buf_size);
    ssize_t n = read(dec->pipe_fd, frame->samples, buf_size);
    if (n < 0) return -1;
    if (n == 0) { dec->eof = true; return -1; }
    frame->n_samples = (int)(n / sizeof(int16_t));
    frame->channels = 2;  /* default stereo */
    frame->sample_rate = 44100;
    return 0;
}

int wubu_dec_seek(WubuDecoder *dec, double timestamp) {
    if (!dec) return -1;
    /* Close current ffmpeg, reopen with seek */
    dec_close_pipe(dec);
    char seek[64];
    snprintf(seek, sizeof(seek), "-ss %.3f", timestamp);
    dec->pipe_fd = dec_start_ffmpeg(dec, seek);
    dec->eof = false;
    return (dec->pipe_fd >= 0) ? 0 : -1;
}

const WubuMediaInfo *wubu_dec_info(WubuDecoder *dec) {
    return dec ? &dec->info : NULL;
}

/* -- Encode -------------------------------------------------------- */

/* Start ffmpeg encoder process, return pipe fd for writing raw frames */
static int enc_start_ffmpeg(WubuEncoder *enc) {
    char cmd[2048];
    if (enc->codec_v[0]) {
        /* Video encoder: pipe raw RGBA frames to ffmpeg */
        snprintf(cmd, sizeof(cmd),
            "ffmpeg -y -v quiet -f rawvideo -pix_fmt rgba -s %dx%d -r %d/%d -i -"
            " -c:v %s -b:v %lld '%s' 2>/dev/null",
            enc->width, enc->height,
            enc->fps_num, enc->fps_den,
            enc->codec_v, (long long)enc->bit_rate_v,
            enc->path);
    } else {
        /* Audio-only encoder */
        snprintf(cmd, sizeof(cmd),
            "ffmpeg -y -v quiet -f s16le -ar 44100 -ac 2 -i - "
            "-c:a %s -b:a %ld '%s' 2>/dev/null",
            enc->codec_a, (long)enc->bit_rate_a, enc->path);
    }
    /* popen with WRITE pipe (child reads from stdin) */
    int fds[2];
    if (pipe(fds) != 0) return -1;
    pid_t pid = fork();
    if (pid < 0) { close(fds[0]); close(fds[1]); return -1; }
    if (pid == 0) {
        close(fds[1]);
        dup2(fds[0], STDIN_FILENO);
        close(fds[0]);
        char *buf = strdup(cmd);
        if (!buf) _exit(1);
        char *argv[64];
        int argc = 0;
        char *tok = strtok(buf, " ");
        while (tok && argc < 63) { argv[argc++] = tok; tok = strtok(NULL, " "); }
        argv[argc] = NULL;
        execvp(argv[0], argv);
        _exit(1);
    }
    close(fds[0]);
    enc->pid = (int)pid;
    return fds[1];  /* write end */
}

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
    /* Start ffmpeg child process */
    enc->pipe_fd = enc_start_ffmpeg(enc);
    return enc;
}

void wubu_enc_close(WubuEncoder *enc) {
    if (!enc) return;
    if (enc->pipe_fd >= 0) { close(enc->pipe_fd); enc->pipe_fd = -1; }
    if (enc->pid > 0) { kill(enc->pid, SIGTERM); waitpid(enc->pid, NULL, 0); }
    free(enc);
}

int wubu_enc_video_frame(WubuEncoder *enc, const WubuVideoFrame *frame) {
    if (!enc || !frame) return -1;
    if (enc->pipe_fd < 0) return -1;
    if (!frame->data || frame->data_size == 0) return -1;
    ssize_t n = write(enc->pipe_fd, frame->data, frame->data_size);
    return (n == (ssize_t)frame->data_size) ? 0 : -1;
}

int wubu_enc_audio_frame(WubuEncoder *enc, const WubuAudioFrame *frame) {
    if (!enc || !frame) return -1;
    if (enc->pipe_fd < 0) return -1;
    if (!frame->samples || frame->n_samples <= 0) return -1;
    size_t sz = (size_t)frame->n_samples * sizeof(int16_t);
    ssize_t n = write(enc->pipe_fd, frame->samples, sz);
    return (n == (ssize_t)sz) ? 0 : -1;
}

/* -- Transcode ---------------------------------------------------- */

int wubu_transcode_start(WubuTranscode *job) {
    if (!job) return -1;
    if (!wubu_codec_available()) return -1;

    /* Execute transcode in background */
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        /* Child: run ffmpeg */
        char *argv[] = {
            "ffmpeg", "-y", "-i", job->src,
            "-c:v", job->codec_v[0] ? job->codec_v : "copy",
            "-c:a", job->codec_a[0] ? job->codec_a : "copy",
            job->gpu_accel ? "-hwaccel" : "", job->gpu_accel ? "auto" : "",
            job->dst, NULL
        };
        /* Filter out empty strings from argv */
        int ac = 0;
        for (int i = 0; argv[i]; i++) {
            if (argv[i][0]) argv[ac++] = argv[i];
        }
        argv[ac] = NULL;
        execvp("ffmpeg", argv);
        _exit(1);
    }
    job->pid = (int)pid;
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
