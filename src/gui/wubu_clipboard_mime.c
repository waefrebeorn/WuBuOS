/* wubu_clipboard_mime.c -- Clipboard MIME-entry helpers (self-contained).
 *
 * Pure helpers for the X/primary selection MIME arrays: safe strdup, find/add/
 * clear/get MIME entries. Take the mime array by pointer (no shared global).
 * Minimal includes.
 */

#include "wubu_clipboard_internal.h"

char *wubu_strdup_safe(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char *dup = malloc(len);
    if (!dup) return NULL;
    memcpy(dup, s, len);
    return dup;
}
int clipboard_find_mime(ClipboardMimeEntry *mimes, int count, const char *mime_type) {
    for (int i = 0; i < count; i++) {
        if (mimes[i].mime_type && strcmp(mimes[i].mime_type, mime_type) == 0) {
            return i;
        }
    }
    return -1;
}
bool clipboard_add_mime(ClipboardMimeEntry *mimes, int *count, const char *mime_type, const void *data, size_t size) {
    int idx = clipboard_find_mime(mimes, *count, mime_type);
    if (idx >= 0) {
        free(mimes[idx].data);
        free(mimes[idx].mime_type);
    } else if (*count < WUBU_CLIPBOARD_MAX_MIME_TYPES) {
        idx = (*count)++;
    } else {
        return false;
    }
    
    mimes[idx].mime_type = strdup(mime_type);
    mimes[idx].data = malloc(size);
    if (!mimes[idx].mime_type || !mimes[idx].data) {
        free(mimes[idx].mime_type);
        free(mimes[idx].data);
        return false;
    }
    memcpy(mimes[idx].data, data, size);
    mimes[idx].size = size;
    return true;
}
void clipboard_clear_mimes(ClipboardMimeEntry *mimes, int *count) {
    for (int i = 0; i < *count; i++) {
        free(mimes[i].mime_type);
        free(mimes[i].data);
        mimes[i].mime_type = NULL;
        mimes[i].data = NULL;
        mimes[i].size = 0;
    }
    *count = 0;
}
ClipboardMimeEntry* clipboard_get_mime(ClipboardMimeEntry *mimes, int count, const char *mime_type) {
    int idx = clipboard_find_mime(mimes, count, mime_type);
    return idx >= 0 ? &mimes[idx] : NULL;
}
