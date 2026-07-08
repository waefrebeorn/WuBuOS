/*
 * wubu_wallpaper.h -- WuBuOS Wallpaper Decoder + Placement
 *
 * Learned from ReactOS dll/cpl/desk/background.c (PLACEMENT_* enum) and
 * shell32 desktop background painting. Decodes a real raster image into an
 * XRGB8888 buffer (matching WuBuOS VBE framebuffer pixel format) and computes
 * the destination rect for each placement mode (Center/Tile/Stretch/Fit/Fill).
 *
 * No external image libraries (WSL has no libcurl/dev headers, and we forbid
 * system()/stub gaps). BMP (24/32 bpp, uncompressed) is decoded natively.
 * Other formats return 0 (caller falls back to the gradient wallpaper).
 *
 * Also provides a bundled default wallpaper path (WuBuOS teal-blue gradient
 * with centered "W" logo) shipped in screenshots/media/wubuos-default.bmp.
 *
 * Pixel format: 0x00BBGGRR (same as vbe_set_pixel / draw_wallpaper).
 */

#ifndef WUBU_WALLPAPER_H
#define WUBU_WALLPAPER_H

#include <stdint.h>
#include <stdbool.h>

/* Placement modes -- mirrors ReactOS PLACEMENT / PLACEMENT_VALUE. */
typedef enum {
    WUBU_WP_CENTER  = 0,
    WUBU_WP_TILE    = 1,
    WUBU_WP_STRETCH = 2,
    WUBU_WP_FIT     = 3,
    WUBU_WP_FILL    = 4
} WubuWallpaperMode;

/* Decoded wallpaper image. Caller owns `pixels` (free with free()). */
typedef struct {
    uint32_t *pixels;   /* w*h XRGB8888 rows, bottom-up source order preserved */
    int       w;
    int       h;
} WubuWallpaper;

/*
 * Decode a wallpaper file into `out`.
 * Returns 1 on success (out->pixels allocated, caller frees), 0 on failure
 * (unsupported format / read error). On 0, *out is zeroed.
 */
int wubu_wallpaper_load(const char *path, WubuWallpaper *out);

/*
 * Get the path to the bundled default wallpaper (ships with the source tree).
 * Returns NULL if the file doesn't exist (caller falls through to gradient).
 */
const char *wubu_wallpaper_default_path(void);

/* Free a WubuWallpaper produced by wubu_wallpaper_load(). */
void wubu_wallpaper_free(WubuWallpaper *wp);

/*
 * Compute the destination rectangle for a placement mode, matching ReactOS
 * background.c semantics:
 *   CENTER  -- native size, centered (no scaling)
 *   TILE    -- native size, repeated (rect = one tile; caller tiles)
 *   STRETCH -- distort to exactly fb_w x (fb_h - task_h)
 *   FIT     -- scale to fit entirely inside (letterbox), preserve aspect
 *   FILL    -- scale to cover (crop overflow), preserve aspect
 * `taskbar_h` is subtracted from available height (desktop excludes taskbar).
 * Outputs the integer dst rect; scale factors returned for sampling.
 */
void wubu_wallpaper_rect(WubuWallpaperMode mode,
                         int img_w, int img_h,
                         int fb_w, int fb_h, int taskbar_h,
                         int *out_x, int *out_y, int *out_w, int *out_h);

#endif /* WUBU_WALLPAPER_H */
