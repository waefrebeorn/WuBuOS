#ifndef WUBU_SCREENSHOT_H
#define WUBU_SCREENSHOT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Screenshot capture modes */
typedef enum {
    WUBU_SSHOT_FULL,       /* PrintScr - entire display */
    WUBU_SSHOT_WINDOW,     /* Alt+PrintScr - focused window */
    WUBU_SSHOT_REGION      /* Shift+PrintScr - user-selected region */
} wubu_sshot_mode_t;

/* Annotation tools */
typedef enum {
    WUBU_ANNOT_NONE,
    WUBU_ANNOT_RECT,
    WUBU_ANNOT_ELLIPSE,
    WUBU_ANNOT_ARROW,
    WUBU_ANNOT_TEXT,
    WUBU_ANNOT_FREEHAND,
    WUBU_ANNOT_HIGHLIGHT
} wubu_annot_tool_t;

/* Annotation style */
typedef struct {
    uint32_t color;        /* ARGB */
    uint32_t fill_color;   /* ARGB, 0 = no fill */
    int thickness;         /* Line thickness */
    int font_size;         /* For text tool */
} wubu_annot_style_t;

/* Annotation object */
typedef struct wubu_annotation {
    wubu_annot_tool_t tool;
    int x1, y1, x2, y2;   /* Rect/ellipse/arrow bounds */
    char *text;            /* For text tool */
    wubu_annot_style_t style;
    struct wubu_annotation *next;
} wubu_annotation_t;

/* Screenshot context */
typedef struct {
    uint32_t *pixels;      /* RGBA buffer */
    int width, height;
    int stride;            /* bytes per row */
    wubu_annotation_t *annotations;
    bool dirty;
} wubu_sshot_t;

/* Screenshots are saved to ~/Pictures/WuBuOS/ */
const char *wubu_screenshot_get_dir(void);

/* Main capture functions */
wubu_sshot_t *wubu_screenshot_capture(wubu_sshot_mode_t mode);
void wubu_screenshot_free(wubu_sshot_t *sshot);

/* Annotation API */
void wubu_screenshot_add_annotation(wubu_sshot_t *sshot,
                                     wubu_annot_tool_t tool,
                                     int x1, int y1, int x2, int y2,
                                     const char *text,
                                     const wubu_annot_style_t *style);
void wubu_screenshot_clear_annotations(wubu_sshot_t *sshot);
void wubu_screenshot_render_annotations(wubu_sshot_t *sshot);

/* Save to file (PNG via bundled stb_image_write) */
bool wubu_screenshot_save(wubu_sshot_t *sshot, const char *filename);
bool wubu_screenshot_save_auto(wubu_sshot_t *sshot, char *out_path, size_t out_size);

/* Generate filename: "Screenshot_YYYY-MM-DD_HH-MM-SS.png" */
void wubu_screenshot_gen_filename(char *buf, size_t n);

/* Copy to clipboard */
bool wubu_screenshot_to_clipboard(wubu_sshot_t *sshot);

/* Hotkey handlers (called from hosted.c input processing) */
void wubu_screenshot_handle_printscr(void);      /* Full screen */
void wubu_screenshot_handle_alt_printscr(void);  /* Active window */
void wubu_screenshot_handle_shift_printscr(void);/* Region select */

/* Region selection overlay (interactive) */
typedef struct wubu_region_selector wubu_region_selector_t;
wubu_region_selector_t *wubu_region_selector_begin(int screen_w, int screen_h,
                                                    void (*on_complete)(int x, int y, int w, int h, void *user),
                                                    void *user);
void wubu_region_selector_end(wubu_region_selector_t *sel);
void wubu_region_selector_mouse(wubu_region_selector_t *sel, int x, int y, bool down);
void wubu_region_selector_render(wubu_region_selector_t *sel, uint32_t *fb, int fb_w, int fb_h);

/* Initialize subsystem (loads config, creates save dir) */
void wubu_screenshot_init(void);
void wubu_screenshot_shutdown(void);

#endif /* WUBU_SCREENSHOT_H */