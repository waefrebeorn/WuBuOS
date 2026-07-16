/* wubu_clipboard_internal.h -- Internal helpers shared by clipboard sub-modules.
 * Public API + types in wubu_clipboard.h. The MIME-entry helpers live in
 * wubu_clipboard_mime.c and are declared here so wubu_clipboard.c (and any
 * future submodule) links the SAME implementation (no double-coding).
 */

#ifndef WUBU_CLIPBOARD_INTERNAL_H
#define WUBU_CLIPBOARD_INTERNAL_H

#define WUBU_CLIPBOARD_MAX_MIME_TYPES 8

#include "wubu_clipboard.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifndef WUBU_CLIPBOARD_TEST_MODE
#include <wayland-client.h>
#include "primary-selection-client.header"
#endif

/* -- Shared types (moved from wubu_clipboard.c so submodules link the same def) -- */
typedef struct {
    char *mime_type;
    void *data;
    size_t size;
} ClipboardMimeEntry;

/* -- Clipboard internal state (shared by wubu_clipboard.c + wubu_clipboard_wl.c) -- */
typedef struct {
    /* Primary selection (highlight/middle-click) */
    ClipboardMimeEntry primary_mimes[WUBU_CLIPBOARD_MAX_MIME_TYPES];
    int primary_mime_count;

    /* Clipboard selection (Ctrl+C/V) */
    ClipboardMimeEntry clipboard_mimes[WUBU_CLIPBOARD_MAX_MIME_TYPES];
    int clipboard_mime_count;

    /* Wayland objects */
#ifndef WUBU_CLIPBOARD_TEST_MODE
    struct wl_data_device *data_device;
    struct wl_data_device_manager *data_device_manager;
    struct wl_seat *seat;

    /* Primary selection Wayland objects */
    struct zwp_primary_selection_device_manager_v1 *primary_device_manager;
    struct zwp_primary_selection_device_v1 *primary_device;

    /* Current offer/source */
    struct wl_data_offer *current_offer;
    struct zwp_primary_selection_offer_v1 *current_primary_offer;
    struct wl_data_source *current_source;
    struct zwp_primary_selection_source_v1 *current_primary_source;
    ClipboardSelection current_selection;
#endif

    /* Callbacks */
    ClipboardChangedCallback changed_cb;
    ClipboardDnDCallback dnd_cb;

    bool dnd_active;
} ClipboardState;

/* -- MIME-entry helpers (wubu_clipboard_mime.c) -------------------- */
char *wubu_strdup_safe(const char *s);
int  clipboard_find_mime(ClipboardMimeEntry *mimes, int count, const char *mime_type);
bool clipboard_add_mime(ClipboardMimeEntry *mimes, int *count, const char *mime_type,
                        const void *data, size_t size);
void clipboard_clear_mimes(ClipboardMimeEntry *mimes, int *count);
ClipboardMimeEntry *clipboard_get_mime(ClipboardMimeEntry *mimes, int count,
                                       const char *mime_type);

/* -- Shared internal state (defined in wubu_clipboard.c) ----------- */
extern ClipboardState g_clipboard;
bool ex_clipboard_read_pipe(int fd, char **out_text, size_t *out_len, int timeout_ms);

#endif /* WUBU_CLIPBOARD_INTERNAL_H */
