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

/* Maximum MIME types we track per selection */


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

static ClipboardState g_clipboard = {0};

/* -- Internal Helpers (available in both test and Wayland modes) ----- */

/* Helper: Find MIME entry index */

/* Helper: Add/replace MIME entry */

/* Helper: Clear all MIME entries */

/* Helper: Get MIME entry */

/* Non-blocking pipe read helper */
static bool ex_clipboard_read_pipe(int fd, char **out_text, size_t *out_len, int timeout_ms) {
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

/* -- Wayland Listeners -------------------------------------------- */

#ifndef WUBU_CLIPBOARD_TEST_MODE

static void data_offer_offer(void *data, struct wl_data_offer *offer, const char *mime_type) {
    (void)data; (void)offer;
    /* Track all offered MIME types - we accept them all */
}

/* -- Primary Selection Offer Listeners ---------------------------- */

static void primary_offer_offer(void *data, struct zwp_primary_selection_offer_v1 *offer, const char *mime_type) {
    (void)data; (void)offer;
    /* Track all offered MIME types for primary selection */
}

static const struct zwp_primary_selection_offer_v1_listener primary_offer_listener = {
    .offer = primary_offer_offer,
};

/* -- Primary Selection Source Listeners --------------------------- */

static void primary_source_send(void *data, struct zwp_primary_selection_source_v1 *source, const char *mime_type, int32_t fd) {
    (void)data; (void)source;
    /* Write our data to the fd for the requested MIME type */
    ClipboardMimeEntry *entry = clipboard_get_mime(g_clipboard.primary_mimes, g_clipboard.primary_mime_count, mime_type);
    if (entry && entry->data) {
        write(fd, entry->data, entry->size);
    }
    close(fd);
}

static void primary_source_cancelled(void *data, struct zwp_primary_selection_source_v1 *source) {
    (void)data; (void)source;
    if (g_clipboard.current_primary_source == source) {
        g_clipboard.current_primary_source = NULL;
    }
}

static const struct zwp_primary_selection_source_v1_listener primary_source_listener = {
    .send = primary_source_send,
    .cancelled = primary_source_cancelled,
};

/* -- Data Source Listeners ---------------------------------------- */

static void data_source_target(void *data, struct wl_data_source *source, const char *mime_type) {
    (void)data; (void)source;
    /* We'll write the data when the receive request comes */
}

static void data_source_send(void *data, struct wl_data_source *source, const char *mime_type, int32_t fd) {
    (void)data; (void)source;
    ClipboardMimeEntry *entry = clipboard_get_mime(g_clipboard.clipboard_mimes, g_clipboard.clipboard_mime_count, mime_type);
    if (entry && entry->data) {
        write(fd, entry->data, entry->size);
    }
    close(fd);
}

static void data_source_cancelled(void *data, struct wl_data_source *source) {
    (void)data; (void)source;
    if (g_clipboard.current_source == source) {
        g_clipboard.current_source = NULL;
    }
}

static const struct wl_data_source_listener data_source_listener = {
    .target = data_source_target,
    .send = data_source_send,
    .cancelled = data_source_cancelled,
};

/* -- Data Offer Listeners ----------------------------------------- */

static const struct wl_data_offer_listener data_offer_listener = {
    .offer = data_offer_offer,
};

/* -- Drag-and-Drop Handlers (called from hosted.c) ----------------- */

static struct wl_data_offer *g_current_dnd_offer = NULL;
static struct wl_surface *g_dnd_target_surface = NULL;

void wubu_clipboard_handle_data_offer(struct wl_data_offer *offer) {
    /* Store the offer for later data retrieval */
    g_clipboard.current_offer = offer;
    wl_data_offer_add_listener(offer, &data_offer_listener, NULL);
    fprintf(stderr, "Clipboard: data_offer received\n");
}

void wubu_clipboard_handle_dnd_enter(uint32_t serial, struct wl_surface *surface, 
                                     int x, int y, struct wl_data_offer *offer) {
    fprintf(stderr, "Clipboard: DnD enter (serial=%u, x=%d, y=%d)\n", serial, x, y);
    g_current_dnd_offer = offer;
    g_dnd_target_surface = surface;
    wl_data_offer_add_listener(offer, &data_offer_listener, NULL);
    if (g_clipboard.dnd_cb) {
        g_clipboard.dnd_cb(true, x, y);
    }
    g_clipboard.dnd_active = true;
}

void wubu_clipboard_handle_dnd_leave(void) {
    fprintf(stderr, "Clipboard: DnD leave\n");
    g_current_dnd_offer = NULL;
    g_dnd_target_surface = NULL;
    if (g_clipboard.dnd_cb) {
        g_clipboard.dnd_cb(false, 0, 0);
    }
    g_clipboard.dnd_active = false;
}

void wubu_clipboard_handle_dnd_motion(uint32_t time, wl_fixed_t x, wl_fixed_t y) {
    (void)time;
    if (g_clipboard.dnd_cb) {
        g_clipboard.dnd_cb(true, wl_fixed_to_int(x), wl_fixed_to_int(y));
    }
}

void wubu_clipboard_handle_dnd_drop(void) {
    fprintf(stderr, "Clipboard: DnD drop\n");
    if (g_current_dnd_offer) {
        /* Request the data - prefer text/plain */
        int pipefd[2];
        if (pipe(pipefd) == 0) {
            wl_data_offer_receive(g_current_dnd_offer, "text/plain", pipefd[1]);
            close(pipefd[1]);
            /* Read the data */
            char buf[4096];
            ssize_t n = read(pipefd[0], buf, sizeof(buf) - 1);
            if (n > 0) {
                buf[n] = '\0';
                fprintf(stderr, "Clipboard: DnD received text: %.100s\n", buf);
            }
            close(pipefd[0]);
        }
    }
    g_clipboard.dnd_active = false;
}

/* -- Clipboard Manager API (Wayland mode) ------------------------- */

int wubu_clipboard_init(void *wl_seat) {
    memset(&g_clipboard, 0, sizeof(g_clipboard));
    g_clipboard.seat = (struct wl_seat *)wl_seat;
    
    if (!g_clipboard.seat) return -1;
    
    /* Get data device manager from global */
    extern wayland_state_t g_wl;
    g_clipboard.data_device_manager = g_wl.data_device_manager;
    
    if (!g_clipboard.data_device_manager) return -1;
    
    /* Get primary selection device manager from global */
    extern struct zwp_primary_selection_device_manager_v1 *g_primary_selection_manager;
    g_clipboard.primary_device_manager = g_primary_selection_manager;
    
    return 0;
}

void wubu_clipboard_shutdown(void) {
    clipboard_clear_mimes(g_clipboard.primary_mimes, &g_clipboard.primary_mime_count);
    clipboard_clear_mimes(g_clipboard.clipboard_mimes, &g_clipboard.clipboard_mime_count);
    
    if (g_clipboard.current_source) {
        wl_data_source_destroy(g_clipboard.current_source);
        g_clipboard.current_source = NULL;
    }
    
    if (g_clipboard.current_primary_source) {
        zwp_primary_selection_source_v1_destroy(g_clipboard.current_primary_source);
        g_clipboard.current_primary_source = NULL;
    }
    
    if (g_clipboard.data_device) {
        wl_data_device_destroy(g_clipboard.data_device);
        g_clipboard.data_device = NULL;
    }
    
    if (g_clipboard.primary_device) {
        zwp_primary_selection_device_v1_destroy(g_clipboard.primary_device);
        g_clipboard.primary_device = NULL;
    }
}

bool wubu_clipboard_set_text(ClipboardSelection selection, const char *text) {
    if (!text) return wubu_clipboard_set_data(selection, &(ClipboardData){.mime_type = CLIPBOARD_MIME_TEXT, .data = text, .size = strlen(text)}, 1);
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
    
    /* If clipboard selection, create Wayland data source with all MIME types */
    if (selection == CLIPBOARD_SELECTION_CLIPBOARD && g_clipboard.data_device_manager) {
        if (g_clipboard.current_source) {
            wl_data_source_destroy(g_clipboard.current_source);
        }
        
        g_clipboard.current_source = wl_data_device_manager_create_data_source(g_clipboard.data_device_manager);
        wl_data_source_add_listener(g_clipboard.current_source, &data_source_listener, NULL);
        
        for (int i = 0; i < *mime_count; i++) {
            wl_data_source_offer(g_clipboard.current_source, mimes[i].mime_type);
        }
        
        wl_data_device_set_selection(g_clipboard.data_device, g_clipboard.current_source, 0);
    }
    
    /* If primary selection, create Wayland primary selection source */
    if (selection == CLIPBOARD_SELECTION_PRIMARY && g_clipboard.primary_device_manager) {
        if (g_clipboard.current_primary_source) {
            zwp_primary_selection_source_v1_destroy(g_clipboard.current_primary_source);
        }
        
        g_clipboard.current_primary_source = zwp_primary_selection_device_manager_v1_create_source(g_clipboard.primary_device_manager);
        zwp_primary_selection_source_v1_add_listener(g_clipboard.current_primary_source, &primary_source_listener, NULL);
        
        for (int i = 0; i < *mime_count; i++) {
            zwp_primary_selection_source_v1_offer(g_clipboard.current_primary_source, mimes[i].mime_type);
        }
        
        zwp_primary_selection_device_v1_set_selection(g_clipboard.primary_device, g_clipboard.current_primary_source, 0);
    }
    
    if (g_clipboard.changed_cb) g_clipboard.changed_cb(selection);
    return true;
}

void wubu_clipboard_clear(ClipboardSelection selection) {
    if (selection == CLIPBOARD_SELECTION_PRIMARY) {
        clipboard_clear_mimes(g_clipboard.primary_mimes, &g_clipboard.primary_mime_count);
        
        /* Clear Wayland primary selection */
        if (g_clipboard.primary_device) {
            zwp_primary_selection_device_v1_set_selection(g_clipboard.primary_device, NULL, 0);
        }
        if (g_clipboard.current_primary_source) {
            zwp_primary_selection_source_v1_destroy(g_clipboard.current_primary_source);
            g_clipboard.current_primary_source = NULL;
        }
    } else {
        clipboard_clear_mimes(g_clipboard.clipboard_mimes, &g_clipboard.clipboard_mime_count);
        
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

void wubu_clipboard_handle_primary_offer(void *offer) {
    g_clipboard.current_primary_offer = (struct zwp_primary_selection_offer_v1*)offer;
    if (offer) {
        zwp_primary_selection_offer_v1_add_listener(offer, &primary_offer_listener, NULL);
    }
}

void wubu_clipboard_handle_primary_selection(void *offer) {
    g_clipboard.current_primary_offer = (struct zwp_primary_selection_offer_v1*)offer;
    g_clipboard.current_selection = CLIPBOARD_SELECTION_PRIMARY;
    if (offer) {
        zwp_primary_selection_offer_v1_add_listener(offer, &primary_offer_listener, NULL);
        /* Read the primary selection data - non-blocking with timeout */
        int fds[2];
        if (pipe(fds) == 0) {
            zwp_primary_selection_offer_v1_receive(offer, CLIPBOARD_MIME_TEXT, fds[1]);
            close(fds[1]);  /* Close write end immediately */
            char *text = NULL;
            size_t len = 0;
            if (ex_clipboard_read_pipe(fds[0], &text, &len, 100)) {
                /* Clear existing and add text/plain */
                clipboard_clear_mimes(g_clipboard.primary_mimes, &g_clipboard.primary_mime_count);
                clipboard_add_mime(g_clipboard.primary_mimes, &g_clipboard.primary_mime_count, CLIPBOARD_MIME_TEXT, text, len);
                free(text);
                if (g_clipboard.changed_cb) g_clipboard.changed_cb(CLIPBOARD_SELECTION_PRIMARY);
            } else {
                /* Timeout or error - don't block UI */
            }
        }
    } else {
        /* Selection cleared */
        clipboard_clear_mimes(g_clipboard.primary_mimes, &g_clipboard.primary_mime_count);
        if (g_clipboard.changed_cb) g_clipboard.changed_cb(CLIPBOARD_SELECTION_PRIMARY);
    }
}

void wubu_clipboard_handle_data_source(void *source) {
    g_clipboard.current_source = (struct wl_data_source*)source;
    if (source) {
        wl_data_source_add_listener(source, &data_source_listener, NULL);
    }
}

void wubu_clipboard_handle_primary_source(void *source) {
    g_clipboard.current_primary_source = (struct zwp_primary_selection_source_v1*)source;
    if (source) {
        zwp_primary_selection_source_v1_add_listener(source, &primary_source_listener, NULL);
    }
}

void wubu_clipboard_handle_dnd_action(uint32_t action) {
    (void)action;
}

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

/* Wayland offer handling (no-ops in test mode) */
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


