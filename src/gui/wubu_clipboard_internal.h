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

/* -- Shared types (moved from wubu_clipboard.c so submodules link the same def) -- */
typedef struct {
    char *mime_type;
    void *data;
    size_t size;
} ClipboardMimeEntry;

/* -- MIME-entry helpers (wubu_clipboard_mime.c) -------------------- */
char *wubu_strdup_safe(const char *s);
int  clipboard_find_mime(ClipboardMimeEntry *mimes, int count, const char *mime_type);
bool clipboard_add_mime(ClipboardMimeEntry *mimes, int *count, const char *mime_type,
                        const void *data, size_t size);
void clipboard_clear_mimes(ClipboardMimeEntry *mimes, int *count);
ClipboardMimeEntry *clipboard_get_mime(ClipboardMimeEntry *mimes, int count,
                                       const char *mime_type);

#endif /* WUBU_CLIPBOARD_INTERNAL_H */
