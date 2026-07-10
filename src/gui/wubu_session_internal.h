/* wubu_session_internal.h -- Session internal header.
 * Shared JSON-read helpers (used by both autostart + session restore) +
 * module-private state. Public API + types in wubu_session.h.
 */

#ifndef WUBU_SESSION_INTERNAL_H
#define WUBU_SESSION_INTERNAL_H

#include <stddef.h>
#include "wubu_session.h"

/* -- Shared JSON scalar readers (defined in wubu_session_autostart.c) ----- */
bool json_read_bool(const char *str);
int  json_read_int(const char *str);
void json_read_string(const char *src, char *dst, size_t dst_len);

/* -- Config path helpers (defined in wubu_session_autostart.c) -------- */
const char *session_file_path(void);
const char *session_autostart_path(void);
bool ensure_session_dirs(void);

#endif /* WUBU_SESSION_INTERNAL_H */
