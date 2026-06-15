/*
 * wubu_canvas.h  --  WuBuOS Image Editor (Photoshop class)
 *
 * Cell 397: Layered image editor with plugin architecture.
 *
 * Features from Photoshop:
 *   - Multiple layers with per-layer opacity
 *   - Blend modes (Normal, Multiply, Screen, Overlay, etc.)
 *   - Layer visibility toggle + lock
 *   - Layer reorder (drag in layers panel)
 *   - Selection tools (rect, elliptical, magic wand, lasso)
 *   - Brush, eraser, fill bucket, gradient, line, shape
 *   - Undo/redo with action history
 *   - Zoom + pan (GAAD snap for zoom levels: φ^n)
 *   - Color picker + palette
 *   - Resize canvas / resize image
 *   - Crop
 *   - Filters (blur, sharpen, edge detect, invert, threshold)
 *
 * Plugin architecture:
 *   - Plugin API: init, process_layer, process_selection, destroy
 *   - AI-assisted plugins: inpaint, upscale, style transfer (future)
 *   - Plugins are .wubu containers (Inferno distribution model)
 *   - Plugin registry at /wubu/plugins/
 *
 * Export formats:
 *   - PNG (via stb_image_write or wubu_png)
 *   - GIF (animated, per-frame layers)
 *   - BMP (native, no deps)
 *   - PPM (simplest, native)
 *
 * The canvas IS a .wubu container. Layers are payloads.
 * The editor is the shell. Plugins are nested containers.
 */
#ifndef WUBU_CANVAS_H
#define WUBU_CANVAS_H

#include <stdint.h>
#include <stdbool.h>

/* -- Canvas Limits ------------------------------------------------- */

#define WUBU_CV_MAX_LAYERS     32
#define WUBU_CV_MAX_UNDO       256
#define WUBU_CV_MAX_PALETTE    256
#define WUBU_CV_MAX_PLUGINS    16
#define WUBU_CV_PLUGIN_NAME    64

/* -- Blend Modes -------------------------------------------------- */

typedef enum {
    BLEND_NORMAL    = 0,
    BLEND_MULTIPLY  = 1,    /* a*b/255 */
    BLEND_SCREEN    = 2,    /* 255 - (255-a)*(255-b)/255 */
    BLEND_OVERLAY   = 3,    /* combine multiply + screen */
    BLEND_DIFFERENCE= 4,    /* |a-b| */
    BLEND_ADDITION  = 5,    /* min(a+b, 255) */
    BLEND_SUBTRACT  = 6,    /* max(a-b, 0) */
    BLEND_DARKEN    = 7,    /* min(a,b) */
    BLEND_LIGHTEN   = 8,    /* max(a,b) */
    BLEND_COLOR_DODGE = 9,
    BLEND_COLOR_BURN = 10,
    BLEND_HARD_LIGHT = 11,
    BLEND_SOFT_LIGHT = 12,
} WubuBlendMode;

/* -- Layer -------------------------------------------------------- */

typedef struct {
    char         name[64];
    int          w, h;           /* Layer dimensions (can differ from canvas) */
    int          x, y;           /* Offset within canvas */
    uint32_t    *pixels;         /* XRGB8888 pixel data */
    uint8_t      opacity;        /* 0-255 (0=invisible, 255=fully opaque) */
    WubuBlendMode blend;         /* Blend mode */
    bool         visible;        /* Show/hide */
    bool         locked;         /* Prevent editing */
    bool         is_mask;        /* This layer is a mask for the layer below */
} WubuLayer;

/* -- Selection ---------------------------------------------------- */

typedef enum {
    SEL_NONE       = 0,
    SEL_RECT       = 1,
    SEL_ELLIPSE    = 2,
    SEL_LASSO      = 3,
    SEL_MAGIC_WAND = 4,
} WubuSelKind;

typedef struct {
    WubuSelKind kind;
    int         x, y, w, h;      /* Bounding rect */
    uint8_t    *mask;            /* 1=selected, 0=not (w*h bytes) */
    bool        active;
} WubuSelection;

/* -- Drawing Tool ------------------------------------------------- */

typedef enum {
    TOOL_BRUSH      = 0,
    TOOL_ERASER     = 1,
    TOOL_FILL       = 2,
    TOOL_LINE       = 3,
    TOOL_RECT       = 4,
    TOOL_ELLIPSE    = 5,
    TOOL_PICKER     = 6,
    TOOL_CROP       = 7,
    TOOL_TEXT        = 8,
    TOOL_GRADIENT   = 9,
    TOOL_CLONE      = 10,   /* Clone stamp */
    TOOL_MOVE       = 11,   /* Move layer */
} WubuTool;

typedef struct {
    WubuTool  tool;
    uint32_t  fg_color;      /* Foreground color */
    uint32_t  bg_color;      /* Background color */
    int       brush_size;    /* Diameter in pixels */
    int       brush_hardness;/* 0-100 (0=soft, 100=hard) */
    int       brush_opacity; /* 0-100 */
    bool      anti_alias;    /* Anti-aliased drawing? */
    int       brush_x, brush_y;  /* Last brush position */
} WubuToolState;

/* -- Plugin API --------------------------------------------------- */

typedef struct WubuCanvas WubuCanvas;

/* Plugin callback signatures */
typedef int  (*PluginInit)(WubuCanvas *cv, const char *config);
typedef int  (*PluginProcessLayer)(WubuCanvas *cv, int layer_idx,
                                    WubuSelection *sel, void *user_data);
typedef int  (*PluginProcessImage)(WubuCanvas *cv, void *user_data);
typedef void (*PluginDestroy)(void *user_data);

typedef struct {
    char               name[WUBU_CV_PLUGIN_NAME];
    char               version[16];
    PluginInit         init;
    PluginProcessLayer process_layer;
    PluginProcessImage process_image;
    PluginDestroy      destroy;
    void              *user_data;
    bool               active;
} WubuPlugin;

/* -- Undo Action -------------------------------------------------- */

typedef enum {
    CV_UNDO_BRUSH   = 1,
    CV_UNDO_LAYER   = 2,    /* Add/remove/reorder layer */
    CV_UNDO_TRANSFORM = 3,  /* Move/resize/rotate */
    CV_UNDO_FILTER  = 4,
} WubuCvUndoKind;

typedef struct {
    WubuCvUndoKind kind;
    int            layer_idx;
    int            x, y, w, h;      /* Affected region */
    uint32_t      *saved_pixels;    /* Before-state pixels */
    int            saved_len;
} WubuCvUndo;

/* -- GIF Animation ------------------------------------------------ */

typedef struct {
    int       delay_ms;       /* Frame delay in milliseconds */
    int       n_layers;       /* Which layers compose this frame */
    int       layer_indices[WUBU_CV_MAX_LAYERS];
} WubuGifFrame;

typedef struct {
    WubuGifFrame frames[256];
    int          n_frames;
    bool         loop;          /* Loop animation? */
    int          loop_count;    /* 0 = infinite */
} WubuGifAnim;

/* -- Complete Canvas State ----------------------------------------- */

struct WubuCanvas {
    int           w, h;          /* Canvas dimensions */
    char          filename[512];
    bool          modified;

    WubuLayer     layers[WUBU_CV_MAX_LAYERS];
    int           n_layers;
    int           active_layer;  /* Currently editing */

    WubuSelection selection;
    WubuToolState tool;

    WubuCvUndo   undo_stack[WUBU_CV_MAX_UNDO];
    int           undo_pos;
    int           undo_count;

    /* View */
    double        zoom;          /* 1.0 = 100% */
    int           pan_x, pan_y; /* Viewport offset */
    bool          show_grid;
    bool          show_rulers;

    /* Palette */
    uint32_t      palette[WUBU_CV_MAX_PALETTE];
    int           n_palette;

    /* Plugins */
    WubuPlugin    plugins[WUBU_CV_MAX_PLUGINS];
    int           n_plugins;

    /* GIF animation */
    WubuGifAnim   gif;
};

/* -- Canvas Lifecycle --------------------------------------------- */

WubuCanvas *wubu_cv_create(int w, int h);
void        wubu_cv_destroy(WubuCanvas *cv);

/* -- Layer Operations -------------------------------------------- */

int   wubu_cv_layer_add(WubuCanvas *cv, const char *name);
int   wubu_cv_layer_add_from_data(WubuCanvas *cv, const char *name,
                                   const uint32_t *pixels, int w, int h);
void  wubu_cv_layer_remove(WubuCanvas *cv, int idx);
void  wubu_cv_layer_move(WubuCanvas *cv, int from, int to);
void  wubu_cv_layer_dup(WubuCanvas *cv, int idx);
void  wubu_cv_layer_merge_down(WubuCanvas *cv, int idx);
void  wubu_cv_layer_flatten(WubuCanvas *cv);     /* All → one */

void  wubu_cv_layer_set_opacity(WubuCanvas *cv, int idx, uint8_t opacity);
void  wubu_cv_layer_set_blend(WubuCanvas *cv, int idx, WubuBlendMode blend);
void  wubu_cv_layer_set_visible(WubuCanvas *cv, int idx, bool visible);
void  wubu_cv_layer_set_locked(WubuCanvas *cv, int idx, bool locked);
WubuLayer *wubu_cv_layer_get(WubuCanvas *cv, int idx);

/* -- Compositing -------------------------------------------------- */

/* Blend two pixels with given opacity and blend mode */
uint32_t wubu_blend(uint32_t dst, uint32_t src, uint8_t opacity,
                     WubuBlendMode mode);

/* Composite all visible layers to a flat buffer */
void wubu_cv_composite(WubuCanvas *cv, uint32_t *out, int out_w, int out_h);

/* -- Drawing Tools ------------------------------------------------ */

void wubu_cv_brush(WubuCanvas *cv, int x, int y);
void wubu_cv_eraser(WubuCanvas *cv, int x, int y);
void wubu_cv_fill(WubuCanvas *cv, int x, int y);
void wubu_cv_line(WubuCanvas *cv, int x0, int y0, int x1, int y1);
void wubu_cv_rect(WubuCanvas *cv, int x, int y, int w, int h, bool filled);
void wubu_cv_ellipse(WubuCanvas *cv, int cx, int cy, int rx, int ry, bool filled);
void wubu_cv_gradient(WubuCanvas *cv, int x0, int y0, int x1, int y1);
uint32_t wubu_cv_pick(WubuCanvas *cv, int x, int y);

/* -- Selection ---------------------------------------------------- */

void wubu_cv_select_rect(WubuCanvas *cv, int x, int y, int w, int h);
void wubu_cv_select_ellipse(WubuCanvas *cv, int cx, int cy, int rx, int ry);
void wubu_cv_select_none(WubuCanvas *cv);
void wubu_cv_select_all(WubuCanvas *cv);
void wubu_cv_select_invert(WubuCanvas *cv);

/* -- Filters (built-in) ------------------------------------------- */

void wubu_cv_filter_blur(WubuCanvas *cv, int radius);
void wubu_cv_filter_sharpen(WubuCanvas *cv, int amount);
void wubu_cv_filter_edge(WubuCanvas *cv);
void wubu_cv_filter_invert(WubuCanvas *cv);
void wubu_cv_filter_threshold(WubuCanvas *cv, int threshold);
void wubu_cv_filter_grayscale(WubuCanvas *cv);

/* -- Plugin API --------------------------------------------------- */

int  wubu_cv_plugin_register(WubuCanvas *cv, const WubuPlugin *plugin);
int  wubu_cv_plugin_run(WubuCanvas *cv, int plugin_idx);
void wubu_cv_plugin_unregister(WubuCanvas *cv, int plugin_idx);

/* -- File I/O ----------------------------------------------------- */

int  wubu_cv_save_png(WubuCanvas *cv, const char *path);
int  wubu_cv_save_bmp(WubuCanvas *cv, const char *path);
int  wubu_cv_save_ppm(WubuCanvas *cv, const char *path);
int  wubu_cv_save_gif(WubuCanvas *cv, const char *path);
int  wubu_cv_load(WubuCanvas *cv, const char *path);

/* -- Undo/Redo ---------------------------------------------------- */

void wubu_cv_undo(WubuCanvas *cv);
void wubu_cv_redo(WubuCanvas *cv);

/* -- View --------------------------------------------------------- */

void wubu_cv_zoom_in(WubuCanvas *cv);     /* φ scale: zoom × φ */
void wubu_cv_zoom_out(WubuCanvas *cv);    /* zoom / φ */
void wubu_cv_zoom_fit(WubuCanvas *cv);
void wubu_cv_pan(WubuCanvas *cv, int dx, int dy);

/* -- Canvas ops --------------------------------------------------- */

void wubu_cv_resize(WubuCanvas *cv, int new_w, int new_h);
void wubu_cv_crop(WubuCanvas *cv, int x, int y, int w, int h);
void wubu_cv_flip_h(WubuCanvas *cv);
void wubu_cv_flip_v(WubuCanvas *cv);
void wubu_cv_rotate_90(WubuCanvas *cv, bool clockwise);

#endif /* WUBU_CANVAS_H */
