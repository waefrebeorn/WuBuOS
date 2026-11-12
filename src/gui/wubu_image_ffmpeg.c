#define _POSIX_C_SOURCE 200809L
/*
 * wubu_image_ffmpeg.c -- image codecs backed by ffmpeg.
 *
 * See wubu_image_ffmpeg.h.  One concern: turn a file on disk into pixels,
 * or pixels into a file, using ffmpeg as the codec backbone.
 *
 * The subprocess contract is deliberately boring:
 *   decode:  ffmpeg -v error -i IN -f rawvideo -pix_fmt rgba -   -> stdout
 *   encode:  ffmpeg -v error -f rawvideo -pix_fmt rgba -s WxH -i - OUT
 * rawvideo is ffmpeg's own trivial codec, so no format negotiation is needed
 * on the pipe -- only the file at the far end needs a real backend.
 *
 * C11, opaque API.
 */

#include "wubu_image_ffmpeg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/wait.h>

#define FFMPEG_NAMES_MAX 4
static const char *const FFMPEG_NAMES[FFMPEG_NAMES_MAX] = {
    "ffmpeg", "ffmpeg-9", "ffmpeg-8", "ffmpeg-7"
};

static char g_ffmpeg[256];
static int  g_ffmpeg_found;
static char g_version[128];

/* Locate an ffmpeg binary once. WUBU_FFMPEG overrides the search. */
static const char *ffmpeg_path(void) {
    if (g_ffmpeg_found) return g_ffmpeg[0] ? g_ffmpeg : NULL;

    const char *env = getenv("WUBU_FFMPEG");
    if (env && env[0]) {
        snprintf(g_ffmpeg, sizeof(g_ffmpeg), "%s", env);
        g_ffmpeg_found = 1;
        return g_ffmpeg;
    }

    for (int i = 0; i < FFMPEG_NAMES_MAX; i++) {
        const char *name = FFMPEG_NAMES[i];
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "command -v %s 2>/dev/null", name);
        FILE *p = popen(cmd, "r");
        if (!p) continue;
        char found[256] = {0};
        if (fgets(found, sizeof(found), p)) {
            size_t n = strlen(found);
            while (n && (found[n - 1] == '\n' || found[n - 1] == '\r'))
                found[--n] = 0;
            if (found[0]) {
                snprintf(g_ffmpeg, sizeof(g_ffmpeg), "%s", found);
                pclose(p);
                g_ffmpeg_found = 1;
                return g_ffmpeg;
            }
        }
        pclose(p);
    }

    g_ffmpeg_found = 1;      /* searched and not found */
    g_ffmpeg[0] = 0;
    return NULL;
}

int wubu_image_ffmpeg_available(void) {
    return ffmpeg_path() != NULL;
}

const char *wubu_image_ffmpeg_version(void) {
    if (g_version[0]) return g_version;
    const char *ff = ffmpeg_path();
    if (!ff) return "(ffmpeg not found)";
    char vcmd[512];
    snprintf(vcmd, sizeof(vcmd), "%s -version 2>/dev/null | head -1", ff);
    FILE *p = popen(vcmd, "r");
    if (!p) return "(unknown)";
    if (fgets(g_version, sizeof(g_version), p)) {
        size_t n = strlen(g_version);
        while (n && (g_version[n - 1] == '\n' || g_version[n - 1] == '\r'))
            g_version[--n] = 0;
    } else {
        snprintf(g_version, sizeof(g_version), "(unknown)");
    }
    pclose(p);
    return g_version;
}

/* Run `cmd`, feed `in` (may be NULL) to stdin, collect stdout into *out.
 * Returns the exit status, or -1 on a pipe/fork failure. */
static int run_collect(const char *cmd, const void *in, size_t in_len,
                       char **out, size_t *out_len) {
    int in_pipe[2], out_pipe[2];
    if (pipe(in_pipe) || pipe(out_pipe)) return -1;

    pid_t pid = fork();
    if (pid < 0) return -1;

    if (pid == 0) {
        dup2(in_pipe[0], 0);
        dup2(out_pipe[1], 1);
        close(in_pipe[0]); close(in_pipe[1]);
        close(out_pipe[0]); close(out_pipe[1]);
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }

    close(in_pipe[0]);
    close(out_pipe[1]);
    if (in && in_len) {
        size_t done = 0;
        while (done < in_len) {
            ssize_t w = write(in_pipe[1], (const char *)in + done, in_len - done);
            if (w <= 0) break;
            done += (size_t)w;
        }
    }
    close(in_pipe[1]);

    size_t cap = 65536, len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) { close(out_pipe[0]); waitpid(pid, NULL, 0); return -1; }
    for (;;) {
        if (len + 4096 > cap) {
            cap *= 2;
            char *nb = (char *)realloc(buf, cap);
            if (!nb) { free(buf); close(out_pipe[0]); waitpid(pid, NULL, 0); return -1; }
            buf = nb;
        }
        ssize_t r = read(out_pipe[0], buf + len, cap - len);
        if (r <= 0) break;
        len += (size_t)r;
    }
    close(out_pipe[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    *out = buf;
    *out_len = len;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

int wubu_image_load_ffmpeg(const char *path,
                           uint32_t **out_rgba, int *out_w, int *out_h) {
    if (!path || !out_rgba || !out_w || !out_h) return -1;
    const char *ff = ffmpeg_path();
    if (!ff) return -1;

    /* Ask ffprobe for the dimensions -- parsing ffmpeg's stream banner is
     * brittle because the "WIDTHxHEIGHT" pair can appear several times. */
    int w = 0, h = 0;
    {
        char probe[1024];
        snprintf(probe, sizeof(probe),
                 "ffprobe -v error -select_streams v:0 "
                 "-show_entries stream=width,height -of csv=p=0 '%s' 2>/dev/null",
                 path);
        char *dim = NULL;
        size_t dim_len = 0;
        if (run_collect(probe, NULL, 0, &dim, &dim_len) == 0 && dim) {
            /* csv=p=0 prints "WIDTH,HEIGHT" */
            char *comma = strchr(dim, ',');
            if (comma) {
                *comma = '\0';
                w = atoi(dim);
                h = atoi(comma + 1);
            }
        }
        free(dim);
    }

    /* Decode the file to raw RGBA on stdout. */
    char cmd[1200];
    snprintf(cmd, sizeof(cmd),
             "\"%s\" -v error -i \"%s\" -f rawvideo -pix_fmt rgba - 2>/dev/null",
             ff, path);
    char *raw = NULL;
    size_t raw_len = 0;
    int rc = run_collect(cmd, NULL, 0, &raw, &raw_len);
    if (rc != 0 || !raw || raw_len < 4) { free(raw); return -1; }

    if (w <= 0 || h <= 0 || (size_t)w * (size_t)h * 4 > raw_len) {
        free(raw);
        return -1;
    }

    uint32_t *rgba = (uint32_t *)malloc((size_t)w * h * 4);
    if (!rgba) { free(raw); return -1; }
    memcpy(rgba, raw, (size_t)w * h * 4);
    free(raw);

    *out_rgba = rgba;
    *out_w = w;
    *out_h = h;
    return 0;
}

int wubu_image_save_ffmpeg(const char *path,
                           const uint32_t *rgba, int w, int h,
                           const char *fmt) {
    if (!path || !rgba || w <= 0 || h <= 0) return -1;
    const char *ff = ffmpeg_path();
    if (!ff) return -1;

    /* Let ffmpeg pick the muxer from the file extension.  Forcing -f png
     * is wrong on distros whose build carries no png muxer (Ubuntu ships
     * image2 + apng and infers the rest from the name). */
    char cmd[1200];
    snprintf(cmd, sizeof(cmd),
             "\"%s\" -v error -f rawvideo -pix_fmt rgba -s %dx%d -i - "
             "-y \"%s\" 2>/dev/null",
             ff, w, h, path);

    char *out = NULL;
    size_t out_len = 0;
    int rc = run_collect(cmd, rgba, (size_t)w * h * 4, &out, &out_len);
    free(out);
    return rc == 0 ? 0 : -1;
}
