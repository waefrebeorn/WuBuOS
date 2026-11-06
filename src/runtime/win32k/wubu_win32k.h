/*
 * wubu_win32k.h -- WuBuOS win32k-lite : kernel-mode window-manager object model
 * (window station / desktop / window class / message queue / window) on the
 * wubu_cap authority core.
 *
 * This is the P1 slice of the "make Windows systems" backbone (see
 * os-studies/reactos_study.md + docs/TRIPLE_DA_OS_BASELINE_2026-11-06.md):
 * win32k on Windows/ReactOS is the kernel-mode USER+ GDI store — windows,
 * message queues, window classes, desktops, hooks. Here the object *store*
 * is the wubu_cap registry and every object is a cap-backed token:
 *
 *   - create a station/desktop/class/window  -> mints a cap token
 *   - every mutation (post/get/peek/destroy) resolves the token through
 *     wubu_cap_resolve(pid, tok, required_rights) — audience + rights
 *     + generation audited on every call
 *   - wubu_win32k_revoke() -> wubu_cap_revoke() : the handle stops granting
 *     access while the object RECORD survives (the NT authority property,
 *     the same one vsl_nt_cap proves at the syscall layer).
 *
 * The /n/win32 surface begins here as the wubu_win32k_snapshot() serializer
 * (a text view of the live registry); binding it to a Styx9P node is the
 * follow-on P1b.
 *
 * Self-contained C11. No god headers.
 */
#ifndef WUBU_WIN32K_H
#define WUBU_WIN32K_H

#include <stdint.h>
#include <stddef.h>
#include "wubu_cap.h"

/* Object subtypes carried in cap->kind_data (kind is WUBU_CAP_KIND_WINDOW). */
#define WUBU_WIN32K_SUB_STATION 1u
#define WUBU_WIN32K_SUB_DESKTOP 2u
#define WUBU_WIN32K_SUB_CLASS   3u
#define WUBU_WIN32K_SUB_WINDOW  4u

/* GDI surface objects (kind WUBU_CAP_KIND_GDI). The LOCK ORDER is the win32k
 * discipline: DC < BRUSH < PEN < BITMAP. To acquire exclusive ownership of one,
 * no LOWER-order object may be exclusively locked by a different caller. */
#define WUBU_WIN32K_SUB_DC      0x10u
#define WUBU_WIN32K_SUB_BRUSH   0x11u
#define WUBU_WIN32K_SUB_PEN     0x12u
#define WUBU_WIN32K_SUB_BITMAP  0x13u

/* Registry limits (P1 slice). */
#define WUBU_WIN32K_MAX_OBJ    1024u
#define WUBU_WIN32K_NAME_MAX   64u
#define WUBU_WIN32K_QUEUE_MAX  64u

/* Message (a tiny Win32-ish MSG). */
typedef struct wubu_win32k_msg {
    uint32_t msg;        /* WM_* / HWND callback contract: 0x0111 = WM_COMMAND */
    uint64_t wparam;
    uint64_t lparam;
} wubu_win32k_msg_t;

void wubu_win32k_init(void);

/* ---- Window station (the root of a windowing session) ---- */
int wubu_win32k_station_create(const char *name, int32_t owner_pid,
                               wubu_cap_token_t *out);

/* ---- Desktop (inside a station) ---- */
int wubu_win32k_desktop_create(const char *name, wubu_cap_token_t station,
                               int32_t caller_pid, wubu_cap_token_t *out);

/* ---- Window class (names a wndproc; windows are instances of a class) ---- */
int wubu_win32k_class_register(wubu_cap_token_t desktop, const char *name,
                               uintptr_t wndproc, int32_t caller_pid);
int wubu_win32k_class_unregister(wubu_cap_token_t desktop, const char *name,
                                 int32_t caller_pid);

/* ---- Window (owns a message queue) ---- */
int wubu_win32k_window_create(wubu_cap_token_t desktop, const char *class_name,
                              const char *title, int32_t owner_pid,
                              wubu_cap_token_t *out);
int wubu_win32k_window_destroy(wubu_cap_token_t w, int32_t caller_pid);

/* ---- Message queue ops. post returns the queue depth. peek is non-
 *      destructive. get is destructive FIFO; 0 when empty. ----- */
int wubu_win32k_post_message(wubu_cap_token_t w, int32_t caller_pid,
                             uint32_t msg, uint64_t wp, uint64_t lp);
int wubu_win32k_peek_message(wubu_cap_token_t w, int32_t caller_pid,
                             wubu_win32k_msg_t *out);
int wubu_win32k_get_message(wubu_cap_token_t w, int32_t caller_pid,
                            wubu_win32k_msg_t *out);

/* ---- Authority: revoke a token. The record survives; every access then
 *      fails the cap gate (the NT authority-cascade property). ----- */
int wubu_win32k_revoke(wubu_cap_token_t tok);

/* ---- GDI surface objects (cap-backed DC/brush/pen/bitmap).
 * Lock order (the win32k discipline): DC < BRUSH < PEN < BITMAP. acquire()
 * enforces it; refcount is shared (addref/deref); mark_destroy() revokes the
 * cap but the object is only FREED once shared refs are 0 AND it is not
 * exclusively locked (delayed deletion). ------------------------------ */
int wubu_win32k_gdi_create(uint32_t subtype, const char *label,
                           int32_t owner_pid, wubu_cap_token_t *out);
int wubu_win32k_gdi_addref(wubu_cap_token_t g, int32_t caller_pid);
int wubu_win32k_gdi_deref(wubu_cap_token_t g, int32_t caller_pid);
int wubu_win32k_gdi_acquire(wubu_cap_token_t g, int32_t caller_pid);
int wubu_win32k_gdi_release(wubu_cap_token_t g, int32_t caller_pid);
int wubu_win32k_gdi_mark_destroy(wubu_cap_token_t g, int32_t caller_pid);

/* ---- /n/win32 view: serialize the live registry as grouped text. ----- */
size_t wubu_win32k_snapshot(char *buf, size_t cap);

#endif /* WUBU_WIN32K_H */
