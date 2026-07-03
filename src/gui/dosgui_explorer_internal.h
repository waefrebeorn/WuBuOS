/* dosgui_explorer_internal.h  --  Internal helpers shared by explorer sub-modules */

#ifndef WUBU_DOSGUI_EXPLORER_INTERNAL_H
#define WUBU_DOSGUI_EXPLORER_INTERNAL_H

#include "dosgui_explorer.h"

/* -- Safe String Macros (shared across all explorer files) ---------- */

#ifndef WUBU_STRCPY
#define WUBU_STRCPY(dst, src, dst_size) \
    do { \
        if (dst_size > 0) { \
            strncpy((dst), (src), (dst_size) - 1); \
            (dst)[(dst_size) - 1] = '\0'; \
        } \
    } while (0)
#endif

#ifndef WUBU_SNPRINTF
#define WUBU_SNPRINTF(dst, dst_size, fmt, ...) \
    do { \
        if (dst_size > 0) { \
            snprintf((dst), (dst_size), (fmt), __VA_ARGS__); \
            (dst)[(dst_size) - 1] = '\0'; \
        } \
    } while (0)
#endif

#ifndef WUBU_STRLCAT
#define WUBU_STRLCAT(dst, src, dst_size) \
    do { \
        size_t _dst_len = strlen(dst); \
        size_t _src_len = strlen(src); \
        if (_dst_len + _src_len + 1 <= dst_size) { \
            memcpy((dst) + _dst_len, (src), _src_len + 1); \
        } else if (_dst_len < dst_size) { \
            size_t _avail = (dst_size) - _dst_len - 1; \
            memcpy((dst) + _dst_len, (src), _avail); \
            (dst)[(dst_size) - 1] = '\0'; \
        } \
    } while (0)
#endif

/* -- Internal helpers exposed for sub-modules ---------------------- */

/* Update breadcrumbs (called by zip module after populating entries) */
void ex_update_breadcrumbs(ExExplorerState *ex);

#endif /* WUBU_DOSGUI_EXPLORER_INTERNAL_H */