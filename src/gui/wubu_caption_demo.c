/* wubu_caption_demo.c -- render the caption gradient and the focus flash to a
 * PPM so the effect can be looked at without a display. Not part of the WM. */
#include <stdio.h>
#include <stdlib.h>
#include "wubu_caption.h"

#define W 480
#define H 160

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "caption.ppm";
    WubuThemeColors c = {0};
    c.win_title_active = 0x0A246A;
    c.win_title_inactive = 0x808080;
    const uint32_t accent = 0xFFB060;

    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return 1; }
    fprintf(f, "P6\n%d %d\n255\n", W, H);

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            uint32_t rgb;
            if (y < 40) {
                /* active caption, 40px tall, drawn across the full width */
                rgb = wubu_caption_px(&c, y, 40, true);
            } else if (y < 80) {
                /* inactive caption: flat */
                rgb = wubu_caption_px(&c, y - 40, 40, false);
            } else {
                /* focus flash: t runs left to right over this band */
                double t = (double)x / (W - 1);
                rgb = wubu_caption_focus_glow(c.win_title_active, accent, t);
            }
            fputc((rgb >> 16) & 0xFF, f);
            fputc((rgb >> 8) & 0xFF, f);
            fputc(rgb & 0xFF, f);
        }
    }
    fclose(f);
    printf("wrote %s (%dx%d): active gradient / inactive flat / focus flash\n", path, W, H);
    return 0;
}