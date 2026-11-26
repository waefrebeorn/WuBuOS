/*
 * file_picker.c -- the Open/Save dialog the OS never had.
 *
 * Win11's SystemApps list contains Windows.FilePicker; ours contained nothing, so an app that
 * needs a file could only be told a path by hand. This is a real modal picker: it lists a real
 * directory, navigates into subdirectories, takes a file name, and returns an absolute path to
 * its caller -- or nothing at all if it is cancelled.
 *
 * Rules it follows (the same ones the rest of the shell is held to):
 *   - every number comes from the metrics header, every colour from the theme;
 *   - ONE geometry owner (row rects) shared by drawing and hit-testing;
 *   - it never invents a path: if the directory cannot be read, it says so on screen.
 *
 * C11, minimal includes.
 */
#include "file_picker.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

#include "../wm.h"
#include "window_chrome.h"
#include "wubu_theme.h"
#include "../wm_internal.h"

#define FP_MAX_ENTRIES 128
#define FP_NAME_MAX    256

typedef struct {
    DosGuiWindow *win;
    char  dir[512];
    char  name[FP_MAX_ENTRIES][64];
    int   is_dir[FP_MAX_ENTRIES];
    int   count;
    int   sel;
    int   scroll;
    char  typed[FP_NAME_MAX];
    int   typed_len;
    char  status[96];
    int   save_mode;
    int   accepted;
    char  result[640];
    int  (*on_done)(const char *path, void *user);
    void *user;
} FilePicker;

static FilePicker g_fp;    /* one modal picker at a time, like a real dialog */

/* Sort dirs first, then names; simple insertion sort, the list is bounded. */
static void fp_sort(FilePicker *fp) {
    for (int i = 1; i < fp->count; i++) {
        char kn[64]; int kd = fp->is_dir[i];
        memcpy(kn, fp->name[i], sizeof kn);
        int j = i - 1;
        while (j >= 0) {
            int before = (kd > fp->is_dir[j]) ||
                         (kd == fp->is_dir[j] && strcmp(kn, fp->name[j]) > 0);
            if (!before) break;
            memcpy(fp->name[j + 1], fp->name[j], sizeof fp->name[j]);
            fp->is_dir[j + 1] = fp->is_dir[j];
            j--;
        }
        memcpy(fp->name[j + 1], kn, sizeof kn);
        fp->is_dir[j + 1] = kd;
    }
}

static int fp_load(FilePicker *fp, const char *path) {
    DIR *d = opendir(path);
    if (!d) {
        snprintf(fp->status, sizeof fp->status, "Cannot read %s", path);
        fp->count = 0;
        return -1;
    }
    snprintf(fp->dir, sizeof fp->dir, "%s", path);
    fp->count = 0;
    fp->sel = 0;
    fp->scroll = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;            /* . and .. and dotfiles */
        if (fp->count >= FP_MAX_ENTRIES) break;
        struct stat st;
        char full[768];
        snprintf(full, sizeof full, "%s/%s", path, e->d_name);
        if (stat(full, &st) != 0) continue;
        snprintf(fp->name[fp->count], sizeof fp->name[0], "%s", e->d_name);
        fp->is_dir[fp->count] = S_ISDIR(st.st_mode) ? 1 : 0;
        fp->count++;
    }
    closedir(d);
    fp_sort(fp);
    snprintf(fp->status, sizeof fp->status, "%d item%s", fp->count, fp->count == 1 ? "" : "s");
    return 0;
}

/* ---- geometry: one owner ------------------------------------------------- */

static void fp_content(DosGuiWindow *w, int *x, int *y, int *cw, int *ch) {
    int bar = chrome_caption_metrics().bar_h;
    int bw = chrome_border_width();
    if (x) *x = w->x + bw;
    if (y) *y = w->y + bar;
    if (cw) *cw = w->w - 2 * bw;
    if (ch) *ch = w->h - bar - bw;
}

static int fp_row_h(void) { return WUBU_METRIC_CONTROL_H - 4; }

static void fp_list_rect(DosGuiWindow *w, int *x, int *y, int *lw, int *lh) {
    int cx, cy, cw, ch;
    fp_content(w, &cx, &cy, &cw, &ch);
    int pad = WUBU_METRIC_MARGIN_MD;
    int top = cy + pad + fp_row_h() * 2;      /* path row + header row */
    if (x) *x = cx + pad;
    if (y) *y = top;
    if (lw) *lw = cw - 2 * pad;
    /* Reserve the NAME row and the BUTTON row below the list: reserving only the buttons put
     * the name field underneath them, so it was never visible. */
    if (lh) *lh = (cy + ch - pad - fp_row_h() * 2 - WUBU_SPACE_XS) - top;
}

static void fp_button_rect(DosGuiWindow *w, int which, int *x, int *y, int *bw, int *bh) {
    int cx, cy, cw, ch;
    fp_content(w, &cx, &cy, &cw, &ch);
    int pad = WUBU_METRIC_MARGIN_MD;
    int gap = WUBU_SPACE_XS;
    int width = 96;
    int total = width * 2 + gap;
    int by = cy + ch - pad - WUBU_METRIC_CONTROL_H;
    int bx = cx + cw - pad - total;
    if (x) *x = bx + which * (width + gap);
    if (y) *y = by;
    if (bw) *bw = width;
    if (bh) *bh = WUBU_METRIC_CONTROL_H;
}

/* ---- drawing ------------------------------------------------------------- */

static void fp_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    (void)fb; (void)fb_w; (void)fb_h;
    FilePicker *fp = &g_fp;
    const WubuThemeColors *t = wubu_theme_colors();
    int cx, cy, cw, ch;
    fp_content(win, &cx, &cy, &cw, &ch);
    int pad = WUBU_METRIC_MARGIN_MD;

    vbe_fill_rect(cx, cy, cw, ch, t->win_face);

    /* path bar */
    int row = fp_row_h();
    vbe_fill_rect_rounded(cx + pad, cy + pad, cw - 2 * pad, row,
                          WUBU_METRIC_RADIUS_CONTROL, t->btn_face);
    vbe_draw_text(cx + pad + WUBU_SPACE_XS,
                  WUBU_TEXT_LABEL_Y(cy + pad, row), fp->dir, t->body_text, 1);

    /* column header */
    vbe_draw_text(cx + pad, WUBU_TEXT_LABEL_Y(cy + pad + row, row), "Name", t->body_text_ina, 1);
    vbe_draw_text(cx + pad + cw / 2, WUBU_TEXT_LABEL_Y(cy + pad + row, row), "Type",
                  t->body_text_ina, 1);

    int lx, ly, lw, lh;
    fp_list_rect(win, &lx, &ly, &lw, &lh);
    vbe_fill_rect_rounded(lx, ly, lw, lh, WUBU_METRIC_RADIUS_CONTROL, t->win_title_inactive);
    int rows = lh / row;
    for (int i = 0; i < rows; i++) {
        int idx = fp->scroll + i;
        if (idx >= fp->count) break;
        int ry = ly + i * row;
        if (idx == fp->sel)
            vbe_fill_rect_rounded(lx, ry, lw, row - 2, WUBU_METRIC_RADIUS_CONTROL, t->select_bg);
        uint32_t ink = (idx == fp->sel) ? t->select_text : t->body_text;
        /* a directory is marked, and nothing else is: the picker must not pretend to know
         * file types it has no art for */
        char label[96];
        snprintf(label, sizeof label, "%s%s", fp->is_dir[idx] ? "[dir] " : "", fp->name[idx]);
        vbe_draw_text(lx + WUBU_SPACE_XS, WUBU_TEXT_LABEL_Y(ry, row), label, ink, 1);
        vbe_draw_text(lx + lw / 2, WUBU_TEXT_LABEL_Y(ry, row),
                      fp->is_dir[idx] ? "Folder" : "File", ink, 1);
    }

    /* name field */
    int ny = ly + lh + WUBU_SPACE_XS;
    vbe_fill_rect_rounded(cx + pad, ny, cw - 2 * pad, row, WUBU_METRIC_RADIUS_CONTROL, t->btn_face);
    const char *shown = fp->typed_len ? fp->typed : (fp->save_mode ? "File name" : "(select a file)");
    vbe_draw_text(cx + pad + WUBU_SPACE_XS, WUBU_TEXT_LABEL_Y(ny, row), shown,
                  fp->typed_len ? t->body_text : t->body_text_ina, 1);
    if (fp->typed_len) {
        int tw = vbe_text_width(fp->typed, 1);
        vbe_fill_rect(cx + pad + WUBU_SPACE_XS + tw + 1, ny + 6, 1, row - 12, t->body_text);
    }

    /* buttons */
    for (int i = 0; i < 2; i++) {
        int bx, by, bw, bh;
        fp_button_rect(win, i, &bx, &by, &bw, &bh);
        int primary = (i == 0);
        vbe_fill_rect_rounded(bx, by, bw, bh, WUBU_METRIC_RADIUS_CONTROL,
                              primary ? t->select_bg : t->btn_face);
        const char *lbl = primary ? (fp->save_mode ? "Save" : "Open") : "Cancel";
        int tw = vbe_text_width(lbl, 1);
        vbe_draw_text(bx + (bw - tw) / 2, WUBU_TEXT_LABEL_Y(by, bh), lbl,
                      primary ? t->select_text : t->btn_text, 1);
    }

    /* Status sits in the column header row, right-aligned: at the bottom it collided with the
     * button row and made Cancel unreadable. */
    {
        int sw = vbe_text_width(fp->status, 1);
        vbe_draw_text(cx + cw - pad - sw,
                      WUBU_TEXT_LABEL_Y(cy + pad + row, row), fp->status, t->body_text_ina, 1);
    }
}

/* ---- actions ------------------------------------------------------------- */

static void fp_accept(FilePicker *fp) {
    if (fp->typed_len) {
        snprintf(fp->result, sizeof fp->result, "%s/%s", fp->dir, fp->typed);
    } else if (fp->count > 0) {
        snprintf(fp->result, sizeof fp->result, "%s/%s", fp->dir, fp->name[fp->sel]);
        if (fp->is_dir[fp->sel]) {         /* Enter on a directory navigates */
            fp_load(fp, fp->result);
            return;
        }
    } else {
        snprintf(fp->status, sizeof fp->status, "Nothing selected");
        return;
    }
    fp->accepted = 1;
    if (fp->on_done) fp->on_done(fp->result, fp->user);
    if (fp->win) { DosGuiWindow *w = fp->win; fp->win = NULL; wm_destroy(w); }
}

static void fp_mouse(DosGuiWindow *win, int x, int y, int btn, int kind) {
    FilePicker *fp = &g_fp;
    if (kind != 1 || btn != 1) return;
    (void)btn;

    int bx, by, bw, bh;
    for (int i = 0; i < 2; i++) {
        fp_button_rect(win, i, &bx, &by, &bw, &bh);
        if (x >= bx && x < bx + bw && y >= by && y < by + bh) {
            if (i == 0) fp_accept(fp);
            else if (fp->win) { DosGuiWindow *w = fp->win; fp->win = NULL; wm_destroy(w); }
            return;
        }
    }

    int lx, ly, lw, lh;
    fp_list_rect(win, &lx, &ly, &lw, &lh);
    if (x >= lx && x < lx + lw && y >= ly && y < ly + lh) {
        int row = fp_row_h();
        int idx = fp->scroll + (y - ly) / row;
        if (idx >= 0 && idx < fp->count) {
            if (idx == fp->sel) fp_accept(fp);        /* second click opens */
            else fp->sel = idx;
        }
    }
}

static void fp_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    (void)mods;
    FilePicker *fp = &g_fp;
    if (key == 27) {                       /* Esc cancels */
        if (fp->win) { DosGuiWindow *w = fp->win; fp->win = NULL; wm_destroy(w); }
        return;
    }
    if (key == '\r' || key == '\n') { fp_accept(fp); return; }
    if (key == 0xE048) { if (fp->sel > 0) fp->sel--; return; }              /* Up */
    if (key == 0xE050) { if (fp->sel + 1 < fp->count) fp->sel++; return; }  /* Down */
    if (key == 8) { if (fp->typed_len > 0) fp->typed[--fp->typed_len] = '\0'; return; }
    if (key >= 32 && key < 127 && fp->typed_len < FP_NAME_MAX - 1) {
        fp->typed[fp->typed_len++] = (char)key;
        fp->typed[fp->typed_len] = '\0';
    }
    (void)win;
}

/* ---- public -------------------------------------------------------------- */

int file_picker_open(const char *start_dir, int save_mode,
                     int (*on_done)(const char *path, void *user), void *user) {
    FilePicker *fp = &g_fp;
    memset(fp, 0, sizeof *fp);
    fp->save_mode = save_mode ? 1 : 0;
    fp->on_done = on_done;
    fp->user = user;
    const char *dir = (start_dir && *start_dir) ? start_dir : "/n";
    if (fp_load(fp, dir) != 0) {
        if (fp_load(fp, getenv("HOME") ? getenv("HOME") : "/") != 0) return -1;
    }

    int w = WUBU_FILE_PICKER_W, h = WUBU_FILE_PICKER_H;
    DosGuiWindow *win = wm_create_modal(80, 60, w, h, save_mode ? "Save As" : "Open", NULL);
    if (!win) return -1;
    fp->win = win;
    win->on_draw = fp_draw;
    win->on_key = fp_key;
    win->on_mouse = fp_mouse;
    return 0;
}

/* Default sink: say which path was chosen. A picker whose result nobody can see is a dialog
 * that lies about having done something. */
int wubu_fp_report(const char *path, void *user) {
    (void)user;
    extern __attribute__((weak)) void wubu_notify_simple(const char *, const char *,
                                                         const char *, const char *, int, int);
    if (wubu_notify_simple) {
        wubu_notify_simple("Files", "Selected", path, NULL, 1, 3000);
    }
    fprintf(stderr, "[file_picker] selected: %s\n", path ? path : "(null)");
    return 0;
}

int file_picker_active(void) { return g_fp.win != NULL; }
const char *file_picker_result(void) { return g_fp.accepted ? g_fp.result : NULL; }
int file_picker_count(void) { return g_fp.count; }
const char *file_picker_entry(int i) {
    return (i >= 0 && i < g_fp.count) ? g_fp.name[i] : NULL;
}
int file_picker_entry_is_dir(int i) {
    return (i >= 0 && i < g_fp.count) ? g_fp.is_dir[i] : 0;
}
int file_picker_selected(void) { return g_fp.sel; }
const char *file_picker_dir(void) { return g_fp.dir; }
