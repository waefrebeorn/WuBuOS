/*
 * wubu_clipboard.c  --  WuBuOS Clipboard Manager Implementation
 * Phase 2: Wayland data device + primary selection
 */

#include "wubu_clipboard.h"
#include "../hosted/hosted.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <wayland-client.h>

/* -- Internal State ----------------------------------------------- */

typedef struct {
    /* Primary selection (highlight/middle-click) */
    char *primary_text;
    size_t primary_len;
    
    /* Clipboard selection (Ctrl+C/V) */
    char *clipboard_text;
    size_t clipboard_len;
    
    /* Wayland objects */
    struct wl_data_device *data_device;
    struct wl_data_device_manager *data_device_manager;
    struct wl_seat *seat;
    
    /* Current offer/source */
    struct wl_data_offer *current_offer;
    struct wl_data_source *current_source;
    ClipboardSelection current_selection;
    
    /* Callbacks */
    ClipboardChangedCallback changed_cb;
    ClipboardDnDCallback dnd_cb;
    
    bool dnd_active;
} ClipboardState;

static ClipboardState g_clipboard = {0};

/* -- Wayland Listeners -------------------------------------------- */

static void data_offer_offer(void *data, struct wl_data_offer *offer, const char *mime_type) {
    (void)data; (void)offer; (void)mime_type;
    /* MIME type offered - we just accept text/plain for now */
}

static const struct wl_data_offer_listener data_offer_listener = {
    .offer = data_offer_offer,
};

static void data_device_data_offer(void *data, struct wl_data_device *device, struct wl_data_offer *offer) {
    (void)data; (void)device;
    g_clipboard.current_offer = offer;
    wl_data_offer_add_listener(offer, &data_offer_listener, NULL);
}

static void data_device_enter(void *data, struct wl_data_device *device, uint32_t serial,
                               struct wl_surface *surface, wl_fixed_t x, wl_fixed_t y,
                               struct wl_data_offer *offer) {
    (void)data; (void)device; (void)serial; (void)surface; (void)x; (void)y;
    g_clipboard.current_offer = offer;
    g_clipboard.dnd_active = true;
    if (g_clipboard.dnd_cb) g_clipboard.dnd_cb(true);
}

static void data_device_leave(void *data, struct wl_data_device *device) {
    (void)data; (void)device;
    g_clipboard.current_offer = NULL;
    g_clipboard.dnd_active = false;
    if (g_clipboard.dnd_cb) g_clipboard.dnd_cb(false);
}

static void data_device_motion(void *data, struct wl_data_device *device, uint32_t time,
                                wl_fixed_t x, wl_fixed_t y) {
    (void)data; (void)device; (void)time; (void)x; (void)y;
}

static void data_device_drop(void *data, struct wl_data_device *device) {
    (void)data; (void)device;
    g_clipboard.dnd_active = false;
    if (g_clipboard.dnd_cb) g_clipboard.dnd_cb(false);
}

static void data_device_selection(void *data, struct wl_data_device *device, struct wl_data_offer *offer) {
    (void)data; (void)device;
    g_clipboard.current_offer = offer;
    g_clipboard.current_selection = CLIPBOARD_SELECTION_CLIPBOARD;
    
    if (offer) {
        wl_data_offer_add_listener(offer, &data_offer_listener, NULL);
        /* Data will be received via data_source_send when request is made */
    }
}

static const struct wl_data_device_listener data_device_listener = {
    .data_offer = data_device_data_offer,
    .enter = data_device_enter,
    .leave = data_device_leave,
    .motion = data_device_motion,
    .drop = data_device_drop,
    .selection = data_device_selection,
};

static void data_source_target(void *data, struct wl_data_source *source, const char *mime_type) {
    (void)data; (void)source; (void)mime_type;
}

static void data_source_send(void *data, struct wl_data_source *source, const char *mime_type, int32_t fd) {
    (void)data; (void)source;
    /* Write our data to the fd */
    if (strcmp(mime_type, CLIPBOARD_MIME_TEXT) == 0 && g_clipboard.clipboard_text) {
        write(fd, g_clipboard.clipboard_text, g_clipboard.clipboard_len);
    }
    close(fd);
}

static void data_source_cancelled(void *data, struct wl_data_source *source) {
    (void)data; (void)source;
    /* Source was cancelled (e.g., another app took ownership) */
    if (g_clipboard.current_source == source) {
        g_clipboard.current_source = NULL;
    }
}

static void data_source_dnd_drop_performed(void *data, struct wl_data_source *source) {
    (void)data; (void)source;
}

static void data_source_dnd_finished(void *data, struct wl_data_source *source) {
    (void)data; (void)source;
    if (g_clipboard.current_source == source) {
        g_clipboard.current_source = NULL;
    }
}

static void data_source_action(void *data, struct wl_data_source *source, uint32_t action) {
    (void)data; (void)source; (void)action;
}

static const struct wl_data_source_listener data_source_listener = {
    .target = data_source_target,
    .send = data_source_send,
    .cancelled = data_source_cancelled,
    .dnd_drop_performed = data_source_dnd_drop_performed,
    .dnd_finished = data_source_dnd_finished,
    .action = data_source_action,
};

/* -- Seat Capabilities -------------------------------------------- */

static void seat_capabilities(void *data, struct wl_seat *seat, uint32_t caps) {
    (void)data; (void)seat;
    if (caps & WL_SEAT_CAPABILITY_POINTER && g_clipboard.data_device_manager && !g_clipboard.data_device) {
        g_clipboard.data_device = wl_data_device_manager_get_data_device(
            g_clipboard.data_device_manager, g_clipboard.seat);
        wl_data_device_add_listener(g_clipboard.data_device, &data_device_listener, NULL);
    }
}

static void seat_name(void *data, struct wl_seat *seat, const char *name) {
    (void)data; (void)seat; (void)name;
}

static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_capabilities,
    .name = seat_name,
};

/* -- Clipboard Manager API ---------------------------------------- */

int wubu_clipboard_init(void *wl_seat) {
    memset(&g_clipboard, 0, sizeof(g_clipboard));
    g_clipboard.seat = (struct wl_seat *)wl_seat;
    
    if (!g_clipboard.seat) return -1;
    
    /* Get data device manager from global */
    extern wayland_state_t g_wl;
    g_clipboard.data_device_manager = g_wl.data_device_manager;
    
    if (!g_clipboard.data_device_manager) return -1;
    
    wl_seat_add_listener(g_clipboard.seat, &seat_listener, NULL);
    
    return 0;
}

void wubu_clipboard_shutdown(void) {
    if (g_clipboard.primary_text) { free(g_clipboard.primary_text); g_clipboard.primary_text = NULL; }
    if (g_clipboard.clipboard_text) { free(g_clipboard.clipboard_text); g_clipboard.clipboard_text = NULL; }
    
    if (g_clipboard.current_source) {
        wl_data_source_destroy(g_clipboard.current_source);
        g_clipboard.current_source = NULL;
    }
    
    if (g_clipboard.data_device) {
        wl_data_device_destroy(g_clipboard.data_device);
        g_clipboard.data_device = NULL;
    }
}

bool wubu_clipboard_has_data(ClipboardSelection selection) {
    if (selection == CLIPBOARD_SELECTION_PRIMARY) {
        return g_clipboard.primary_text != NULL && g_clipboard.primary_len > 0;
    } else {
        return g_clipboard.clipboard_text != NULL && g_clipboard.clipboard_len > 0;
    }
}

bool wubu_clipboard_get_text(ClipboardSelection selection, char **out_text, size_t *out_len) {
    char **src_text;
    size_t *src_len;
    
    if (selection == CLIPBOARD_SELECTION_PRIMARY) {
        src_text = &g_clipboard.primary_text;
        src_len = &g_clipboard.primary_len;
    } else {
        src_text = &g_clipboard.clipboard_text;
        src_len = &g_clipboard.clipboard_len;
    }
    
    if (!*src_text || *src_len == 0) return false;
    
    *out_text = malloc(*src_len + 1);
    if (!*out_text) return false;
    
    memcpy(*out_text, *src_text, *src_len);
    (*out_text)[*src_len] = '\0';
    
    if (out_len) *out_len = *src_len;
    return true;
}

bool wubu_clipboard_get_data(ClipboardSelection selection, const char *mime_type,
                              void **out_data, size_t *out_len) {
    (void)mime_type;  /* Only text/plain supported for now */
    return wubu_clipboard_get_text(selection, (char**)out_data, out_len);
}

bool wubu_clipboard_set_text(ClipboardSelection selection, const char *text) {
    if (!text) return wubu_clipboard_clear(selection), true;
    
    size_t len = strlen(text);
    char **dst_text;
    size_t *dst_len;
    
    if (selection == CLIPBOARD_SELECTION_PRIMARY) {
        dst_text = &g_clipboard.primary_text;
        dst_len = &g_clipboard.primary_len;
    } else {
        dst_text = &g_clipboard.clipboard_text;
        dst_len = &g_clipboard.clipboard_len;
    }
    
    /* Free old */
    if (*dst_text) free(*dst_text);
    
    *dst_text = malloc(len + 1);
    if (!*dst_text) return false;
    
    memcpy(*dst_text, text, len + 1);
    *dst_len = len;
    
    /* If clipboard selection, create Wayland data source */
    if (selection == CLIPBOARD_SELECTION_CLIPBOARD && g_clipboard.data_device_manager) {
        if (g_clipboard.current_source) {
            wl_data_source_destroy(g_clipboard.current_source);
        }
        
        g_clipboard.current_source = wl_data_device_manager_create_data_source(g_clipboard.data_device_manager);
        wl_data_source_add_listener(g_clipboard.current_source, &data_source_listener, NULL);
        wl_data_source_offer(g_clipboard.current_source, CLIPBOARD_MIME_TEXT);
        
        wl_data_device_set_selection(g_clipboard.data_device, g_clipboard.current_source, 0);
    }
    
    if (g_clipboard.changed_cb) g_clipboard.changed_cb(selection);
    return true;
}

bool wubu_clipboard_set_data(ClipboardSelection selection,
                              const ClipboardData *data, int count) {
    /* Only text/plain for now */
    for (int i = 0; i < count; i++) {
        if (strcmp(data[i].mime_type, CLIPBOARD_MIME_TEXT) == 0) {
            return wubu_clipboard_set_text(selection, (const char*)data[i].data);
        }
    }
    return false;
}

void wubu_clipboard_clear(ClipboardSelection selection) {
    if (selection == CLIPBOARD_SELECTION_PRIMARY) {
        if (g_clipboard.primary_text) { free(g_clipboard.primary_text); g_clipboard.primary_text = NULL; }
        g_clipboard.primary_len = 0;
    } else {
        if (g_clipboard.clipboard_text) { free(g_clipboard.clipboard_text); g_clipboard.clipboard_text = NULL; }
        g_clipboard.clipboard_len = 0;
        
        /* Clear Wayland selection */
        if (g_clipboard.data_device) {
            wl_data_device_set_selection(g_clipboard.data_device, NULL, 0);
        }
        if (g_clipboard.current_source) {
            wl_data_source_destroy(g_clipboard.current_source);
            g_clipboard.current_source = NULL;
        }
    }
    
    if (g_clipboard.changed_cb) g_clipboard.changed_cb(selection);
}

/* Wayland offer handling */
void wubu_clipboard_handle_offer(void *offer) {
    g_clipboard.current_offer = (struct wl_data_offer*)offer;
    if (offer) {
        wl_data_offer_add_listener(offer, &data_offer_listener, NULL);
    }
}

void wubu_clipboard_handle_selection(void *offer) {
    g_clipboard.current_offer = (struct wl_data_offer*)offer;
    g_clipboard.current_selection = CLIPBOARD_SELECTION_CLIPBOARD;
    if (offer) {
        wl_data_offer_add_listener(offer, &data_offer_listener, NULL);
        /* We'll receive data when the source sends it - the fd is provided by the source */
    }
}

void wubu_clipboard_handle_data_source(void *source) {
    g_clipboard.current_source = (struct wl_data_source*)source;
    if (source) {
        wl_data_source_add_listener(source, &data_source_listener, NULL);
    }
}

void wubu_clipboard_handle_dnd_action(uint32_t action) {
    (void)action;
}

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