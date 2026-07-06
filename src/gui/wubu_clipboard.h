/*
 * wubu_clipboard.h  --  WuBuOS Clipboard Manager
 *
 * Phase 2: GNOME-standard desktop services.
 * Wayland clipboard (data device) + primary selection manager.
 * Provides synchronous API for applications to read/write clipboard.
 */

#ifndef WUBU_CLIPBOARD_H
#define WUBU_CLIPBOARD_H

#include <wayland-client.h>

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* -- MIME Types --------------------------------------------------- */

#define CLIPBOARD_MIME_TEXT      "text/plain;charset=utf-8"
#define CLIPBOARD_MIME_UTF8      "text/plain;charset=utf-8"
#define CLIPBOARD_MIME_HTML      "text/html"
#define CLIPBOARD_MIME_IMAGE_PNG "image/png"
#define CLIPBOARD_MIME_IMAGE_JPEG "image/jpeg"
#define CLIPBOARD_MIME_URI_LIST  "text/uri-list"

/* -- Clipboard Types ---------------------------------------------- */

typedef enum {
    CLIPBOARD_SELECTION_PRIMARY = 0,   /* Middle-click / highlight */
    CLIPBOARD_SELECTION_CLIPBOARD = 1, /* Ctrl+C / Ctrl+V */
} ClipboardSelection;

/* -- Clipboard Data Source ---------------------------------------- */

typedef struct {
    const char *mime_type;      /* e.g., "text/plain" */
    const void *data;
    size_t size;
} ClipboardData;

/* -- Clipboard Manager API ---------------------------------------- */

/* Initialize clipboard manager (sets up Wayland data device) */
int  wubu_clipboard_init(void *wl_seat);
void wubu_clipboard_shutdown(void);

/* Check if clipboard has data */
bool wubu_clipboard_has_data(ClipboardSelection selection);

/* Get clipboard data (allocates buffer, caller frees) */
bool wubu_clipboard_get_text(ClipboardSelection selection, char **out_text, size_t *out_len);
bool wubu_clipboard_get_data(ClipboardSelection selection, const char *mime_type,
                              void **out_data, size_t *out_len);

/* Set clipboard data */
bool wubu_clipboard_set_text(ClipboardSelection selection, const char *text);
bool wubu_clipboard_set_data(ClipboardSelection selection,
                              const ClipboardData *data, int count);

/* Clear clipboard */
void wubu_clipboard_clear(ClipboardSelection selection);

/* Wayland offer handling (call from Wayland data device callbacks) */
void wubu_clipboard_handle_offer(void *offer);
void wubu_clipboard_handle_data_source(void *source);
void wubu_clipboard_handle_dnd_action(uint32_t action);

/* -- Drag-and-Drop Handlers (called from hosted.c) --------------- */

/* DnD handlers - implemented in wubu_clipboard.c */
void wubu_clipboard_handle_dnd_enter(uint32_t serial, struct wl_surface *surface,
                                     int x, int y, struct wl_data_offer *offer);
void wubu_clipboard_handle_dnd_leave(void);
void wubu_clipboard_handle_dnd_motion(uint32_t time, int x, int y);
void wubu_clipboard_handle_dnd_drop(void);

/* -- Convenience Helpers ------------------------------------------ */

/* Quick text copy/paste */
bool wubu_clipboard_copy(const char *text);
bool wubu_clipboard_paste(char **out_text);

/* Primary selection (highlight) */
bool wubu_primary_copy(const char *text);
bool wubu_primary_paste(char **out_text);

/* -- Event Callbacks ---------------------------------------------- */

/* Called when clipboard content changes */
typedef void (*ClipboardChangedCallback)(ClipboardSelection selection);
void wubu_clipboard_set_changed_callback(ClipboardChangedCallback cb);

/* Called when DND enters/leaves */
typedef void (*ClipboardDnDCallback)(bool entered, int x, int y);
void wubu_clipboard_set_dnd_callback(ClipboardDnDCallback cb);

#endif /* WUBU_CLIPBOARD_H */