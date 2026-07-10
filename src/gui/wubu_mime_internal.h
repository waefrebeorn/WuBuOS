/* wubu_mime_internal.h -- WuBuOS MIME layer internal header.
 * Shared declarations for mime sub-modules (desktop parser).
 * Public API + types in wubu_mime.h.
 */

#ifndef WUBU_MIME_INTERNAL_H
#define WUBU_MIME_INTERNAL_H

#include "wubu_mime.h"

/* -- Desktop-file parser (implemented in wubu_mime_desktop.c) -------- */
bool parse_desktop_file(const char *path, DesktopEntry *entry);
bool str_endswith(const char *s, const char *suffix);
const char *get_file_extension(const char *filename);

#endif /* WUBU_MIME_INTERNAL_H */
