/* Gate for screenshot format selection.
 *
 * The defect this pins: the hosted binary wrote PPM bytes into any path given, so `--screenshot
 * shot.png` produced a file whose name lied about its contents -- no viewer opens it, and tools that
 * trust the extension fail with a confusing error. The test therefore checks MAGIC BYTES, not that
 * "a file was created": the old code passed that weaker check perfectly.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hosted_screenshot.h"

/* The real png_encode_rgba drags zlib and the Wayland screenshot module into the link. This gate is
 * about DISPATCH (which encoder a path selects) and file writing, so the encoder is stubbed with one
 * that emits the PNG magic: if the .png path does not reach it, the magic is absent and the check
 * fails. The encoder's own correctness is not this test's claim. */
size_t png_encode_rgba(const uint32_t *px, int w, int h, uint8_t **out);
size_t png_encode_rgba(const uint32_t *px, int w, int h, uint8_t **out)
{
    (void)px;
    if (w <= 0 || h <= 0 || !out) return 0;
    uint8_t *buf = (uint8_t *)malloc(8);
    if (!buf) return 0;
    const uint8_t magic[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    for (int i = 0; i < 8; i++) buf[i] = magic[i];
    *out = buf;
    return 8;
}

#define W 4
#define H 2

static int g_pass, g_fail;

static void ok(int cond, const char *what)
{
    if (cond) {
        g_pass++;
    } else {
        g_fail++;
        printf("  FAIL %s\n", what);
    }
}

static int magic_is(const char *path, const unsigned char *want, int n)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    unsigned char got[8];
    size_t r = fread(got, 1, (size_t)n, f);
    fclose(f);
    return r == (size_t)n && memcmp(got, want, (size_t)n) == 0;
}

int main(void)
{
    static uint32_t fb[W * H];
    for (int i = 0; i < W * H; i++) fb[i] = 0x00336699u;   /* arbitrary XRGB */

    const unsigned char png_magic[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};

    ok(hosted_screenshot_write("/tmp/wubu_shot_a.png", fb, W, H) == 1, "writing .png reports success");
    ok(magic_is("/tmp/wubu_shot_a.png", png_magic, 8), "a .png path produces PNG bytes");

    ok(hosted_screenshot_write("/tmp/wubu_shot_b.PPM", fb, W, H) == 1, "writing .PPM reports success");
    ok(magic_is("/tmp/wubu_shot_b.PPM", (const unsigned char *)"P6\n", 3), "a .PPM path produces P6 bytes");

    ok(hosted_screenshot_write("/tmp/wubu_shot_c.PNG", fb, W, H) == 1, "writing .PNG reports success");
    ok(magic_is("/tmp/wubu_shot_c.PNG", png_magic, 8), "the extension check is case-insensitive");

    ok(hosted_screenshot_write("/tmp/wubu_shot_d.bin", fb, W, H) == 1, "an unknown extension still writes");
    ok(magic_is("/tmp/wubu_shot_d.bin", (const unsigned char *)"P6\n", 3), "an unknown extension falls back to P6");

    ok(hosted_screenshot_write(NULL, fb, W, H) == 0, "NULL path is refused");
    ok(hosted_screenshot_write("/tmp/wubu_shot_e.png", NULL, W, H) == 0, "NULL framebuffer is refused");
    ok(hosted_screenshot_write("/tmp/wubu_shot_e.png", fb, 0, H) == 0, "zero width is refused");

    /* A short name cannot panic the extension check. */
    ok(hosted_screenshot_write("ab", fb, W, H) == 1, "a 2-character path is handled");

    printf("hosted_screenshot: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
