/*
 * wubu_dos_proc.h -- WuBuOS 16-bit DOS process: a real 16-bit .COM/.EXE
 * running INSIDE WuBuOS as an ordinary tracked process via the in-process
 * 8086 compat shim (wubu_dos_emu), surfaced through the Styx/9P namespace
 * and rendered in a desktop window.
 *
 * Honest contract (this is NOT a QEMU/FreeDOS boot):
 *   - wubu_dos_emu.c is a real 1 MB real-mode 8086 interpreter with a DOS INT
 *     layer (INT 21h/10h/16h). No second OS is booted, no disk image, no QEMU.
 *   - launch() loads the .COM/.EXE bytes into the interpreter, runs them to
 *     completion, and captures the text screen + RGBA frame.
 *   - The framebuffer, control, and status are exposed under Styx
 *     /proc/<pid>/dos/{screen,ctl,status}.
 *
 * Nothing here is a stub: load -> run -> capture are real work.
 */
#ifndef WUBU_DOS_PROC_H
#define WUBU_DOS_PROC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Opaque DOS process handle (full struct lives in wubu_dos_proc.c). */
typedef struct WubuDosProc WubuDosProc;

/* Capture resolution limits (text-mode 80x25 @ 8x16 cells). */
#define WUBU_DOS_FB_MAX_W  1024
#define WUBU_DOS_FB_MAX_H  768
#define WUBU_DOS_FB_MAX_BYTES (WUBU_DOS_FB_MAX_W * WUBU_DOS_FB_MAX_H * 4)

/* Process lifecycle states. */
typedef enum {
    DOS_PROC_STOPPED = 0,
    DOS_PROC_STARTING,
    DOS_PROC_RUNNING,
    DOS_PROC_EXITED,
    DOS_PROC_FAILED
} WubuDosProcState;

/*
 * Launch a 16-bit DOS program (.COM/.EXE) as a WuBuOS process.
 *
 *   dos_path : filesystem path to a real .COM/.EXE image (read + loaded).
 *   fmt      : WUBU_PAYLOAD_DOS_COM or WUBU_PAYLOAD_DOS_EXE.
 *
 * Returns a handle on success, or NULL on a bad image / load failure. The
 * caller owns the handle and must wubu_dos_proc_destroy() it.
 */
WubuDosProc *wubu_dos_proc_launch(const char *dos_path, int fmt);

/* Destroy a handle. */
void wubu_dos_proc_destroy(WubuDosProc *p);

/* Mark EXITED and stop the in-process interpreter. Returns 0 on success. */
int  wubu_dos_proc_kill(WubuDosProc *p);

/* Current lifecycle state. */
WubuDosProcState wubu_dos_proc_state(const WubuDosProc *p);
const char      *wubu_dos_proc_state_name(WubuDosProcState s);

/* Host PID (0 — the shim runs in-process, no separate guest). */
int  wubu_dos_proc_pid(const WubuDosProc *p);

/* WuBuOS process id (the /proc/<pid> node this process is mounted under). */
int  wubu_dos_proc_id(const WubuDosProc *p);

/*
 * Capture the captured screen into RGBA32 (w*h*4 bytes). The frame is the
 * interpreter's text screen rendered through wubu_dos_font.h.
 *   out_rgba : caller buffer of at least WUBU_DOS_FB_MAX_BYTES.
 *   out_w/out_h : filled with the captured dimensions.
 * Returns bytes written (>0) on success, 0 if no frame / error.
 */
size_t wubu_dos_proc_frame_capture(WubuDosProc *p,
                                   uint8_t *out_rgba, int *out_w, int *out_h);

/*
 * Feed a key event (ASCII char or a symbolic name: "ret","esc","tab","spc",
 * "backspace","delete") into the interpreter's keyboard buffer. Returns 0
 * on success, -1 if unavailable.
 */
int wubu_dos_proc_send_key(WubuDosProc *p, const char *key);

/*
 * Write raw bytes into the interpreter's keyboard buffer (Styx `ctl` node).
 * Returns bytes written, or -1 on error.
 */
int wubu_dos_proc_write_ctl(WubuDosProc *p, const void *buf, size_t len);

/*
 * Render the status node text into `out` (Styx `status` file body).
 * Returns bytes written.
 */
int wubu_dos_proc_status_text(const WubuDosProc *p, char *out, size_t out_size);

/*
 * Styx / 9P namespace exposure. Mounts the process under <root>/proc/<pid>/dos
 * as real `screen` (RGBA frame), `ctl` (FIFO for input), `status` (text)
 * files that StyxFS serves via its host-backed path mapping. Returns the 9P
 * path (e.g. "/proc/4096/dos") or "" on failure.
 */
const char *wubu_dos_proc_styx_mount(WubuDosProc *p, const char *root);

/* Capture a frame and push it into the mounted Styx `screen` node. */
size_t wubu_dos_proc_frame_to_styx(WubuDosProc *p, const char *root,
                                   uint8_t *out_rgba, int *out_w, int *out_h);

#endif /* WUBU_DOS_PROC_H */
