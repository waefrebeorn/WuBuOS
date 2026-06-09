/*
 * wubu_screenshot.h — WuBuOS Screenshot/Snipping Tool
 *
 * Save framebuffer to PPM/PNG, create GIF animations,
 * and provide an in-OS snipping tool widget.
 */

#ifndef WUBU_SCREENSHOT_H
#define WUBU_SCREENSHOT_H

#include <stdint.h>
#include <stdbool.h>

/* ────────────────────────────────────────────────────────────────── */

typedef enum {
    SHOT_FMT_PPM  = 0,  /* Simple, no deps */
    SHOT_FMT_PNG  = 1,  /* Requires libpng or stb_image_write */
    SHOT_FMT_BMP  = 2,  /* Basic Windows bitmap */
} WubuShotFormat;

/* Snapshot a window or region to file */
int wubu_shot_window(const char *path, int win_id, WubuShotFormat fmt);
int wubu_shot_region(const char *path, int x, int y, int w, int h, WubuShotFormat fmt);
int wubu_shot_fullscreen(const char *path, WubuShotFormat fmt);

/* GIF animation recording */
typedef struct {
    char         output_path[256];
    int          frame_count;
    int          max_frames;
    int          delay_ms;       /* Frame delay in ms */
    int          frame_w, frame_h;
    uint32_t    *frame_buffer;   /* Accumulated frames */
    int          frame_idx;
    bool         active;
} WubuGifRecorder;

int wubu_gif_start(const char *path, int w, int h, int delay_ms, int max_frames);
int wubu_gif_add_frame(int x, int y, int w, int h);
int wubu_gif_stop(void);

/* In-OS Snipping Tool Widget */
typedef enum {
    SNIP_MODE_FULLSCREEN = 0,
    SNIP_MODE_WINDOW     = 1,
    SNIP_MODE_RECTANGLE  = 2,
    SNIP_MODE_FREEFORM   = 3,
} WubuSnipMode;

typedef struct {
    WubuSnipMode mode;
    int          start_x, start_y;
    int          end_x, end_y;
    bool         selecting;
    bool         active;
    void        *overlay_buffer;  /* For drawing selection overlay */
} WubuSnipTool;

int  wubu_snip_tool_init(void);
void wubu_snip_tool_shutdown(void);
void wubu_snip_tool_activate(WubuSnipMode mode);
void wubu_snip_tool_deactivate(void);
void wubu_snip_tool_render(uint32_t *fb, int fb_w, int fb_h);
bool wubu_snip_tool_handle_mouse(int x, int y, int btn, int kind);
int  wubu_snip_tool_save(const char *path, WubuShotFormat fmt);

/* PPM writer (no deps) */
int wubu_write_ppm(const char *path, const uint32_t *buf, int w, int h);

/* BMP writer (no deps) */
int wubu_write_bmp(const char *path, const uint32_t *buf, int w, int h);

/* PNG writer (uses stb_image_write if available) */
int wubu_write_png(const char *path, const uint32_t *buf, int w, int h);

/* Test accessors */
WubuSnipTool *wubu_snip_tool_state(void);
WubuGifRecorder *wubu_gif_recorder_state(void);

#endif