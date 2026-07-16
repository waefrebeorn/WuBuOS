/*
 * wubu_clipboard.c  --  WuBuOS Clipboard Manager Implementation
 * Phase 2: Wayland data device + primary selection
 */

#include "wubu_clipboard.h"
#include "wubu_clipboard_internal.h"
#include "../hosted/hosted.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <poll.h>

#ifndef WUBU_CLIPBOARD_TEST_MODE
#include <wayland-client.h>
#include "primary-selection-client.header"
#endif

/* -- Safe String Macros (WUBU_SAFE_STRING) -------------------------- */

#define WUBU_STRCPY(dst, src, dst_size) \
    do { \
        if (dst_size > 0) { \
            strncpy((dst), (src), (dst_size) - 1); \
            (dst)[(dst_size) - 1] = '\0'; \
        } \
    } while (0)

#define WUBU_SNPRINTF(dst, dst_size, fmt, ...) \
    do { \
        if (dst_size > 0) { \
            snprintf((dst), (dst_size), (fmt), __VA_ARGS__); \
            (dst)[(dst_size) - 1] = '\0'; \
        } \
    } while (0)

/* Safe strdup with NULL check */

/* -- Internal State ----------------------------------------------- */

/* ClipboardState is defined in wubu_clipboard_internal.h (shared with
 * wubu_clipboard_wl.c). Defined here as the single global instance. */
ClipboardState g_clipboard = {0};

/* -- Internal Helpers (available in both test and Wayland modes) ----- */

/* Helper: Find MIME entry index */

/* Helper: Add/replace MIME entry */

/* Helper: Clear all MIME entries */

/* Helper: Get MIME entry */

/* Non-blocking pipe read helper (shared with wubu_clipboard_wl.c via internal header) */
bool ex_clipboard_read_pipe(int fd, char **out_text, size_t *out_len, int timeout_ms) {
    struct pollfd pfd = { .fd = fd, .events = POLLIN };
    int ret = poll(&pfd, 1, timeout_ms);
    if (ret <= 0) {
        close(fd);
        return false;
    }
    
    char buf[8192];
    size_t total = 0;
    char *result = NULL;
    ssize_t n;
    
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        char *new_result = realloc(result, total + n + 1);
        if (!new_result) {
            free(result);
            close(fd);
            return false;
        }
        result = new_result;
        memcpy(result + total, buf, n);
        total += n;
    }
    close(fd);
    
    if (n < 0) {
        free(result);
        return false;
    }
    
    if (result) {
        result[total] = '\0';
    }
    *out_text = result;
    *out_len = total;
    return true;
}

/* Wayland/X11 protocol transport moved to wubu_clipboard_wl.c
 * (listeners + drag-and-drop handlers; compiled in non-test builds). */

#ifndef WUBU_CLIPBOARD_TEST_MODE

/* -- Public API: Get clipboard text (Wayland mode) ----------------- */

bool wubu_clipboard_get_text(ClipboardSelection selection, char **out_text, size_t *out_len) {
    ClipboardMimeEntry *mimes = (selection == CLIPBOARD_SELECTION_PRIMARY) ? 
        g_clipboard.primary_mimes : g_clipboard.clipboard_mimes;
    int count = (selection == CLIPBOARD_SELECTION_PRIMARY) ? 
        g_clipboard.primary_mime_count : g_clipboard.clipboard_mime_count;
    
    ClipboardMimeEntry *entry = clipboard_get_mime(mimes, count, CLIPBOARD_MIME_TEXT);
    if (!entry || !entry->data || entry->size == 0) return false;
    
    *out_text = malloc(entry->size + 1);
    if (!*out_text) return false;
    
    memcpy(*out_text, entry->data, entry->size);
    (*out_text)[entry->size] = '\0';
    
    if (out_len) *out_len = entry->size;
    return true;
}

bool wubu_clipboard_get_data(ClipboardSelection selection, const char *mime_type,
                              void **out_data, size_t *out_len) {
    ClipboardMimeEntry *mimes = (selection == CLIPBOARD_SELECTION_PRIMARY) ? 
        g_clipboard.primary_mimes : g_clipboard.clipboard_mimes;
    int count = (selection == CLIPBOARD_SELECTION_PRIMARY) ? 
        g_clipboard.primary_mime_count : g_clipboard.clipboard_mime_count;
    
    ClipboardMimeEntry *entry = clipboard_get_mime(mimes, count, mime_type);
    if (!entry || !entry->data || entry->size == 0) return false;
    
    *out_data = malloc(entry->size);
    if (!*out_data) return false;
    
    memcpy(*out_data, entry->data, entry->size);
    if (out_len) *out_len = entry->size;
    return true;
}

#else /* WUBU_CLIPBOARD_TEST_MODE */

/* Test mode implementations - no Wayland */

int wubu_clipboard_init(void *wl_seat) {
    (void)wl_seat;
    memset(&g_clipboard, 0, sizeof(g_clipboard));
    return 0;
}

void wubu_clipboard_shutdown(void) {
    clipboard_clear_mimes(g_clipboard.primary_mimes, &g_clipboard.primary_mime_count);
    clipboard_clear_mimes(g_clipboard.clipboard_mimes, &g_clipboard.clipboard_mime_count);
}

bool wubu_clipboard_has_data(ClipboardSelection selection) {
    if (selection == CLIPBOARD_SELECTION_PRIMARY) {
        for (int i = 0; i < g_clipboard.primary_mime_count; i++) {
            if (g_clipboard.primary_mimes[i].size > 0) return true;
        }
        return false;
    } else {
        for (int i = 0; i < g_clipboard.clipboard_mime_count; i++) {
            if (g_clipboard.clipboard_mimes[i].size > 0) return true;
        }
        return false;
    }
}

bool wubu_clipboard_get_text(ClipboardSelection selection, char **out_text, size_t *out_len) {
    ClipboardMimeEntry *mimes = (selection == CLIPBOARD_SELECTION_PRIMARY) ? 
        g_clipboard.primary_mimes : g_clipboard.clipboard_mimes;
    int count = (selection == CLIPBOARD_SELECTION_PRIMARY) ? 
        g_clipboard.primary_mime_count : g_clipboard.clipboard_mime_count;
    
    ClipboardMimeEntry *entry = clipboard_get_mime(mimes, count, CLIPBOARD_MIME_TEXT);
    if (!entry || !entry->data || entry->size == 0) return false;
    
    *out_text = malloc(entry->size + 1);
    if (!*out_text) return false;
    
    memcpy(*out_text, entry->data, entry->size);
    (*out_text)[entry->size] = '\0';
    
    if (out_len) *out_len = entry->size;
    return true;
}

bool wubu_clipboard_get_data(ClipboardSelection selection, const char *mime_type,
                              void **out_data, size_t *out_len) {
    ClipboardMimeEntry *mimes = (selection == CLIPBOARD_SELECTION_PRIMARY) ? 
        g_clipboard.primary_mimes : g_clipboard.clipboard_mimes;
    int count = (selection == CLIPBOARD_SELECTION_PRIMARY) ? 
        g_clipboard.primary_mime_count : g_clipboard.clipboard_mime_count;
    
    ClipboardMimeEntry *entry = clipboard_get_mime(mimes, count, mime_type);
    if (!entry || !entry->data || entry->size == 0) return false;
    
    *out_data = malloc(entry->size);
    if (!*out_data) return false;
    
    memcpy(*out_data, entry->data, entry->size);
    if (out_len) *out_len = entry->size;
    return true;
}

bool wubu_clipboard_set_text(ClipboardSelection selection, const char *text) {
    if (!text || text[0] == '\0') return wubu_clipboard_clear(selection), true;
    
    return wubu_clipboard_set_data(selection, &(ClipboardData){
        .mime_type = CLIPBOARD_MIME_TEXT,
        .data = text,
        .size = strlen(text)
    }, 1);
}

bool wubu_clipboard_set_data(ClipboardSelection selection,
                              const ClipboardData *data, int count) {
    if (count <= 0) return wubu_clipboard_clear(selection), true;
    
    ClipboardMimeEntry *mimes;
    int *mime_count;
    
    if (selection == CLIPBOARD_SELECTION_PRIMARY) {
        mimes = g_clipboard.primary_mimes;
        mime_count = &g_clipboard.primary_mime_count;
    } else {
        mimes = g_clipboard.clipboard_mimes;
        mime_count = &g_clipboard.clipboard_mime_count;
    }
    
    /* Clear existing */
    clipboard_clear_mimes(mimes, mime_count);
    
    /* Add all MIME types */
    for (int i = 0; i < count; i++) {
        if (!clipboard_add_mime(mimes, mime_count, data[i].mime_type, data[i].data, data[i].size)) {
            clipboard_clear_mimes(mimes, mime_count);
            return false;
        }
    }
    
    if (g_clipboard.changed_cb) g_clipboard.changed_cb(selection);
    return true;
}

void wubu_clipboard_clear(ClipboardSelection selection) {
    if (selection == CLIPBOARD_SELECTION_PRIMARY) {
        clipboard_clear_mimes(g_clipboard.primary_mimes, &g_clipboard.primary_mime_count);
    } else {
        clipboard_clear_mimes(g_clipboard.clipboard_mimes, &g_clipboard.clipboard_mime_count);
    }
    
    if (g_clipboard.changed_cb) g_clipboard.changed_cb(selection);
}

/* Wayland offer handling (no-ops in test mode) — the real Wayland
 * implementations live in wubu_clipboard_wl.c (compiled only in
 * non-test builds). */
void wubu_clipboard_handle_offer(void *offer) { (void)offer; }
void wubu_clipboard_handle_selection(void *offer) { (void)offer; }
void wubu_clipboard_handle_primary_offer(void *offer) { (void)offer; }
void wubu_clipboard_handle_primary_selection(void *offer) { (void)offer; }
void wubu_clipboard_handle_data_source(void *source) { (void)source; }
void wubu_clipboard_handle_primary_source(void *source) { (void)source; }
void wubu_clipboard_handle_dnd_action(uint32_t action) { (void)action; }

#endif /* WUBU_CLIPBOARD_TEST_MODE */

/* -- Callbacks ---------------------------------------------------- */

void wubu_clipboard_set_changed_callback(ClipboardChangedCallback cb) {
    g_clipboard.changed_cb = cb;
}

void wubu_clipboard_set_dnd_callback(ClipboardDnDCallback cb) {
    g_clipboard.dnd_cb = cb;
}

/* -- Convenience Helpers Implementation ----------------------------- */

bool wubu_clipboard_copy(const char *text) {
    return wubu_clipboard_set_text(CLIPBOARD_SELECTION_CLIPBOARD, text);
}

bool wubu_clipboard_paste(char **out_text) {
    return wubu_clipboard_get_text(CLIPBOARD_SELECTION_CLIPBOARD, out_text, NULL);
}

bool wubu_primary_copy(const char *text) {
    return wubu_clipboard_set_text(CLIPBOARD_SELECTION_PRIMARY, text);
}

bool wubu_primary_paste(char **out_text) {
    return wubu_clipboard_get_text(CLIPBOARD_SELECTION_PRIMARY, out_text, NULL);
}


