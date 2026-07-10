/* dosgui_explorer_internal.h  --  Internal helpers shared by explorer sub-modules */

#ifndef WUBU_DOSGUI_EXPLORER_INTERNAL_H
#define WUBU_DOSGUI_EXPLORER_INTERNAL_H

#include "dosgui_explorer.h"
#include "wubu_theme.h"
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <libgen.h>

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

/* -- Global state (defined in dosgui_explorer.c) ------------------- */
extern ExExplorerState g_explorer;

/* -- 9P/Styx filesystem backend (implemented in dosgui_explorer_fs.c) --
 * Thin shim mapping POSIX-style ops onto the Styx 9P filesystem (styxfs_*). */
int ex_9p_stat(const char *path, struct stat *st);
int ex_9p_mkdir(const char *path, mode_t mode);
int ex_9p_unlink(const char *path);
int ex_9p_rename(const char *oldpath, const char *newpath);
int ex_9p_open(const char *path, int flags);
ssize_t ex_9p_read(int fd, void *buf, size_t count);
ssize_t ex_9p_write(int fd, const void *buf, size_t count);
int ex_9p_close(int fd);
int ex_9p_readdir(const char *path, struct dirent ***entries);
DIR *ex_9p_opendir(const char *path);

/* -- Theme helpers (static inline — zero overhead, no symbol clash) -- */
static inline const WubuThemeColors *tc(void) { return wubu_theme_colors(); }
static inline const WubuTheme *th(void) { return wubu_theme_get(); }
static inline int ex_row_h(void) { return th()->rounded_buttons ? 24 : EX_ROW_H; }
static inline int ex_tree_indent(void) { return th()->rounded_buttons ? 14 : EX_TREE_INDENT; }
static inline int ex_title_h(void) { return th()->rounded_buttons ? 24 : 22; }
static inline int ex_toolbar_h(void) { return EX_TOOLBAR_H; }
static inline int ex_breadcrumb_h(void) { return EX_BREADCRUMB_H; }
static inline int ex_statusbar_h(void) { return EX_STATUSBAR_H; }
static inline int ex_border_w(void) { return th()->rounded_buttons ? 3 : 2; }

/* -- Safe dirname wrapper (shared) --------------------------------- */
static inline void ex_dirname_safe(const char *path, char *out, size_t out_size) {
    if (!path || !out || out_size == 0) return;
    char copy[EX_MAX_PATH];
    strncpy(copy, path, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';
    char *d = dirname(copy);
    WUBU_STRCPY(out, d, out_size);
}

/* -- Render layer (implemented in dosgui_explorer_render.c) -------- */
void ex_render_tree(ExExplorerState *ex, uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int h);
void ex_render_breadcrumbs(ExExplorerState *ex, uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int h);
void ex_render_toolbar(ExExplorerState *ex, uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int h);
void ex_render_file_list(ExExplorerState *ex, uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int h);
void ex_render_preview(ExExplorerState *ex, uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int h);
void ex_render_statusbar(ExExplorerState *ex, uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int h);
void ex_render_context_menu(ExExplorerState *ex, uint32_t *fb, int fb_w, int fb_h);

#endif /* WUBU_DOSGUI_EXPLORER_INTERNAL_H */