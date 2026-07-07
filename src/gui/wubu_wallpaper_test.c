/*
 * wubu_wallpaper_test.c -- Real wallpaper decode + placement verification.
 *
 * No GPU needed: writes a real 2x2 24-bit BMP to a temp file, decodes it,
 * and asserts the pixels land correctly (XRGB8888). Also checks the five
 * ReactOS placement rects. Mirrors the project's test style (PASS/FAIL).
 */

#include "wubu_wallpaper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) printf("PASS: %s\n", msg); \
    else { printf("FAIL: %s\n", msg); g_fail++; } \
} while (0)

/* Write a real 2x2 24-bit BMP (bottom-up rows), colors:
 *   top row (y=1): red, green   bottom row (y=0): blue, white */
static int write_test_bmp(const char *path) {
    uint8_t red[3]   = {0, 0, 255};   /* BGR: blue=0,green=0,red=255 */
    uint8_t green[3] = {0, 255, 0};
    uint8_t blue[3]  = {255, 0, 0};
    uint8_t white[3] = {255, 255, 255};

    int row_bytes = 2 * 3;            /* 6 */
    int pad = (4 - (row_bytes & 3)) & 3; /* 2 -> pad 2 */
    int stride = row_bytes + pad;     /* 8 */
    int pix_size = 2 * stride;        /* 16 */
    int fsize = 14 + 40 + pix_size;

    uint8_t hdr[14];
    hdr[0]='B'; hdr[1]='M';
    hdr[2]=(uint8_t)(fsize & 0xFF); hdr[3]=(uint8_t)((fsize>>8)&0xFF);
    hdr[4]=(uint8_t)((fsize>>16)&0xFF); hdr[5]=(uint8_t)((fsize>>24)&0xFF);
    hdr[6]=hdr[7]=hdr[8]=hdr[9]=0;
    hdr[10]=54; hdr[11]=0; hdr[12]=0; hdr[13]=0; /* bfOffBits = 54 */

    uint8_t ih[40];
    memset(ih, 0, sizeof(ih));
    ih[0]=40;
    ih[4]=2;          /* width = 2 */
    ih[8]=2;          /* height = 2 (positive => bottom-up) */
    ih[12]=1; ih[13]=0; /* planes */
    ih[14]=24; ih[15]=0; /* bpp */
    /* compression = 0, sizeImage = 0, rest 0 */

    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    fwrite(hdr, 1, 14, f);
    fwrite(ih, 1, 40, f);
    /* bottom row first (y=0): blue, white */
    fwrite(blue, 1, 3, f); fwrite(white, 1, 3, f);
    uint8_t padv[2] = {0,0}; fwrite(padv, 1, pad, f);
    /* top row (y=1): red, green */
    fwrite(red, 1, 3, f); fwrite(green, 1, 3, f); fwrite(padv, 1, pad, f);
    fclose(f);
    return 1;
}

int main(void) {
    const char *bmp = "/tmp/wubu_wp_test.bmp";
    printf("=== WuBuOS Wallpaper Test ===\n");

    CHECK(write_test_bmp(bmp), "write 2x2 test BMP to /tmp");

    WubuWallpaper wp;
    int ok = wubu_wallpaper_load(bmp, &wp);
    CHECK(ok == 1, "wubu_wallpaper_load returns success on real BMP");
    CHECK(wp.w == 2 && wp.h == 2, "decoded dimensions 2x2");

    /* Bottom-up: src row 0 = bottom = blue,white ; src row 1 = top = red,green.
     * Decoded display buffer (dst_y=0 == top): top-left=red, top-right=green,
     * bottom-left=blue, bottom-right=white. */
    uint32_t px00 = wp.pixels[0];  /* top-left     = red   (0x000000FF) */
    uint32_t px10 = wp.pixels[1];  /* top-right    = green (0x0000FF00) */
    uint32_t px01 = wp.pixels[2];  /* bottom-left  = blue  (0x00FF0000) */
    uint32_t px11 = wp.pixels[3];  /* bottom-right = white (0x00FFFFFF) */
    CHECK((px00 & 0xFFFFFF) == 0x000000FF, "top-left pixel is red");
    CHECK((px10 & 0xFFFFFF) == 0x0000FF00, "top-right pixel is green");
    CHECK((px01 & 0xFFFFFF) == 0x00FF0000, "bottom-left pixel is blue");
    CHECK((px11 & 0xFFFFFF) == 0x00FFFFFF, "bottom-right pixel is white");

    wubu_wallpaper_free(&wp);
    CHECK(wp.pixels == NULL, "wubu_wallpaper_free nulls pixels");

    /* Unsupported format -> fail, not crash. */
    WubuWallpaper bad;
    CHECK(wubu_wallpaper_load("/nonexistent/file.xyz", &bad) == 0,
          "unknown format returns 0");

    /* Placement rect math (fb 1024x768, taskbar 28). */
    int x,y,w,h;
    wubu_wallpaper_rect(WUBU_WP_CENTER, 100, 100, 1024, 768, 28, &x,&y,&w,&h);
    CHECK(w == 100 && h == 100, "CENTER keeps native size");
    CHECK(x == (1024-100)/2 && y == (768-28-100)/2, "CENTER centered");

    wubu_wallpaper_rect(WUBU_WP_TILE, 100, 100, 1024, 768, 28, &x,&y,&w,&h);
    CHECK(w == 100 && h == 100, "TILE keeps native tile size");

    wubu_wallpaper_rect(WUBU_WP_STRETCH, 100, 100, 1024, 768, 28, &x,&y,&w,&h);
    CHECK(w == 1024 && h == 740, "STRETCH fills (fb_w x avail_h)");

    wubu_wallpaper_rect(WUBU_WP_FIT, 200, 100, 1024, 768, 28, &x,&y,&w,&h);
    CHECK(w <= 1024 && h <= 740, "FIT stays inside bounds");
    CHECK(abs(w*100 - h*200) <= 2, "FIT preserves aspect (200x100 => 2:1)");

    wubu_wallpaper_rect(WUBU_WP_FILL, 200, 100, 1024, 768, 28, &x,&y,&w,&h);
    CHECK(w >= 1024 || h >= 740, "FILL covers (>= one dimension)");
    CHECK(abs(w*100 - h*200) <= 2, "FILL preserves aspect");

    if (g_fail == 0) {
        printf("\n✅ All wubu_wallpaper tests passed\n");
        return 0;
    }
    printf("\n❌ %d wallpaper test(s) failed\n", g_fail);
    return 1;
}
