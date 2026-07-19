/*
 * wubu_dos_proc.c -- WuBuOS 16-bit DOS process (in-process 8086 compat shim).
 *
 * This is the "compatible window" engine: a 16-bit .COM/.EXE runs INSIDE
 * WuBuOS as an ordinary process via the real 8086 interpreter (wubu_dos_emu),
 * NOT by booting a separate OS / QEMU guest. The program's captured text
 * screen + RGBA frame are exposed to the desktop window and the Styx/9P
 * namespace. No second OS is booted (the pollution is gone).
 *
 * Owns:
 *   - the emulator instance,
 *   - the captured frame (RGBA) for the WM window to blit,
 *   - the Styx /proc/<pid>/dos surface (screen/ctl/status files).
 *
 * Self-contained runtime module. Minimal includes. C11.
 */
#include "wubu_dos_proc.h"
#include "wubu_dos_emu.h"
#include "wubu_container.h"   /* WUBU_PAYLOAD_DOS_* (payload type enum) */
#include "wubu_exec_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>

/* ------------------------------------------------------------------ */
/* Opaque state                                                       */
/* ------------------------------------------------------------------ */

struct WubuDosProc {
    int                wubu_pid;     /* our /proc node id */
    WubuDosProcState   wstate;
    char               dos_path[512];
    unsigned           frames;       /* frames captured */
    /* Last captured RGBA frame for the WM window. */
    uint8_t           *fb;
    int                fb_w, fb_h;
    size_t             fb_bytes;
    WubuDosEmu        *emu;          /* in-proc 8086 shim */
    char               styx_root[256];
};

/* ------------------------------------------------------------------ */
/* Launch / destroy                                                    */
/* ------------------------------------------------------------------ */

WubuDosProc *wubu_dos_proc_launch(const char *dos_path, int fmt) {
    if (!dos_path) return NULL;

    WubuDosProc *p = (WubuDosProc *)calloc(1, sizeof(*p));
    if (!p) return NULL;
    p->wubu_pid = (int)(0x1000 + (uintptr_t)p % 0xEFFF); /* synthetic /proc id */
    p->wstate = DOS_PROC_STARTING;
    p->emu = wubu_dos_emu_create();
    if (!p->emu) { free(p); return NULL; }
    p->fb = (uint8_t *)malloc(WUBU_DOS_FB_MAX_BYTES);
    if (!p->fb) { wubu_dos_emu_destroy(p->emu); free(p); return NULL; }
    snprintf(p->dos_path, sizeof(p->dos_path), "%s", dos_path);

    /* Read the program bytes and load them into the in-process shim. */
    FILE *f = fopen(dos_path, "rb");
    if (!f) { p->wstate = DOS_PROC_FAILED; return p; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 640 * 1024) { fclose(f); p->wstate = DOS_PROC_FAILED; return p; }
    uint8_t *img = (uint8_t *)malloc((size_t)sz);
    if (!img) { fclose(f); p->wstate = DOS_PROC_FAILED; return p; }
    if (fread(img, 1, (size_t)sz, f) != (size_t)sz) { free(img); fclose(f); p->wstate = DOS_PROC_FAILED; return p; }
    fclose(f);

    int rc;
    if (fmt == WUBU_PAYLOAD_DOS_EXE)
        rc = wubu_dos_emu_load_exe(p->emu, img, (size_t)sz);
    else
        rc = wubu_dos_emu_load_com(p->emu, img, (size_t)sz);
    free(img);
    if (rc != 0) { p->wstate = DOS_PROC_FAILED; return p; }

    /* Run to completion (most DOS programs are short; the window loop can
     * also call run again if it wants interactive stepping). */
    WubuDosEmuState st = wubu_dos_emu_run(p->emu, 0);
    p->wstate = (st == WUBU_DOS_TERMINATED) ? DOS_PROC_EXITED
              : (st == WUBU_DOS_ERROR)      ? DOS_PROC_FAILED
              : DOS_PROC_RUNNING;
    return p;
}

void wubu_dos_proc_destroy(WubuDosProc *p) {
    if (!p) return;
    if (p->emu) wubu_dos_emu_destroy(p->emu);
    free(p->fb);
    free(p);
}

int wubu_dos_proc_kill(WubuDosProc *p) {
    if (!p) return -1;
    /* The shim is a thread of our own process; "kill" just stops it. */
    p->wstate = DOS_PROC_EXITED;
    return 0;
}

WubuDosProcState wubu_dos_proc_state(const WubuDosProc *p) {
    return p ? p->wstate : DOS_PROC_STOPPED;
}
const char *wubu_dos_proc_state_name(WubuDosProcState s) {
    switch (s) {
        case DOS_PROC_STOPPED:  return "stopped";
        case DOS_PROC_STARTING: return "starting";
        case DOS_PROC_RUNNING:  return "running";
        case DOS_PROC_EXITED:   return "exited";
        case DOS_PROC_FAILED:   return "failed";
    }
    return "unknown";
}
int wubu_dos_proc_pid(const WubuDosProc *p)      { return p ? 0 : 0; }     /* in-proc */
int wubu_dos_proc_id(const WubuDosProc *p)       { return p ? p->wubu_pid : 0; }

/* ------------------------------------------------------------------ */
/* Frame capture (text screen -> RGBA)                                */
/* ------------------------------------------------------------------ */

size_t wubu_dos_proc_frame_capture(WubuDosProc *p,
                                   uint8_t *out_rgba, int *out_w, int *out_h) {
    if (!p || !p->emu) return 0;
    int w = 0, h = 0;
    size_t n = wubu_dos_emu_frame_rgba(p->emu, p->fb, &w, &h);
    if (n == 0) return 0;
    if (out_rgba && n <= WUBU_DOS_FB_MAX_BYTES) memcpy(out_rgba, p->fb, n);
    if (out_w) *out_w = w;
    if (out_h) *out_h = h;
    p->fb_w = w; p->fb_h = h; p->fb_bytes = n;
    p->frames++;
    return n;
}

int wubu_dos_proc_send_key(WubuDosProc *p, const char *key) {
    if (!p || !p->emu || !key) return -1;
    /* Translate a symbolic key name to an ASCII byte (best effort). */
    if (strcmp(key, "ret") == 0) { wubu_dos_emu_key(p->emu, '\r'); return 0; }
    if (strcmp(key, "esc") == 0) { wubu_dos_emu_key(p->emu, 0x1B); return 0; }
    if (strcmp(key, "tab") == 0) { wubu_dos_emu_key(p->emu, '\t'); return 0; }
    if (strcmp(key, "spc") == 0) { wubu_dos_emu_key(p->emu, ' '); return 0; }
    if (strcmp(key, "backspace") == 0) { wubu_dos_emu_key(p->emu, '\b'); return 0; }
    if (strcmp(key, "delete") == 0) { wubu_dos_emu_key(p->emu, 0x7F); return 0; }
    if (strlen(key) == 1) { wubu_dos_emu_key(p->emu, (uint8_t)key[0]); return 0; }
    return -1;
}

int wubu_dos_proc_write_ctl(WubuDosProc *p, const void *buf, size_t len) {
    if (!p || !p->emu) return -1;
    const uint8_t *b = (const uint8_t *)buf;
    for (size_t i = 0; i < len; i++) wubu_dos_emu_key(p->emu, b[i]);
    return (int)len;
}

int wubu_dos_proc_status_text(const WubuDosProc *p, char *out, size_t out_size) {
    if (!p || !out || out_size == 0) return -1;
    int n = snprintf(out, out_size,
        "pid=%d state=%s frames=%u exit=%d\n",
        p->wubu_pid, wubu_dos_proc_state_name(p->wstate),
        p->frames, wubu_dos_emu_exit_code(p->emu));
    return n;
}

/* ------------------------------------------------------------------ */
/* Styx / 9P namespace exposure (host-backed, served by StyxFS)       */
/* ------------------------------------------------------------------ */

static int mkdir_p(const char *path) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *q = tmp + 1; *q; q++) {
        if (*q == '/') {
            *q = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
            *q = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
    return 0;
}

const char *wubu_dos_proc_styx_mount(WubuDosProc *p, const char *root) {
    if (!p || !root) return "";
    static char styx_path[256];
    if (mkdir_p(root) != 0) return "";
    char dir[512];
    snprintf(dir, sizeof(dir), "%s/proc/%d/dos", root, p->wubu_pid);
    if (mkdir_p(dir) != 0) return "";
    char fstatus[640];
    snprintf(fstatus, sizeof(fstatus), "%s/status", dir);
    { char buf[256]; int n = wubu_dos_proc_status_text(p, buf, sizeof(buf));
      FILE *s = fopen(fstatus, "w"); if (s) { if (n > 0) fwrite(buf,1,n,s); fclose(s); } }
    char fscreen[640];
    snprintf(fscreen, sizeof(fscreen), "%s/screen", dir);
    { FILE *s = fopen(fscreen, "w"); if (s) fclose(s); }
    char fctl[640];
    snprintf(fctl, sizeof(fctl), "%s/ctl", dir);
    unlink(fctl); mkfifo(fctl, 0644);
    snprintf(styx_path, sizeof(styx_path), "/proc/%d/dos", p->wubu_pid);
    return styx_path;
}

static void styx_refresh_status(WubuDosProc *p, const char *root) {
    if (!root) return;
    char fstatus[640];
    snprintf(fstatus, sizeof(fstatus), "%s/proc/%d/dos/status", root, p->wubu_pid);
    char buf[256]; int n = wubu_dos_proc_status_text(p, buf, sizeof(buf));
    FILE *s = fopen(fstatus, "w"); if (s) { if (n > 0) fwrite(buf,1,n,s); fclose(s); }
}

size_t wubu_dos_proc_frame_to_styx(WubuDosProc *p, const char *root,
                                   uint8_t *out_rgba, int *out_w, int *out_h) {
    size_t n = wubu_dos_proc_frame_capture(p, out_rgba, out_w, out_h);
    if (n == 0 || !root) return n;
    char fscreen[640];
    snprintf(fscreen, sizeof(fscreen), "%s/proc/%d/dos/screen", root, p->wubu_pid);
    FILE *s = fopen(fscreen, "wb");
    if (s) { fwrite(p->fb, 1, n, s); fclose(s); }
    styx_refresh_status(p, root);
    return n;
}
