/* wubu_fbterm.c — minimal framebuffer echo of the ring-0 console.
 *
 * Wave 0: the same line editor that drives COM1 and PS/2 draws into the
 * GOP framebuffer, so a human looking at the metal display sees the typed
 * command. The public globals are read by the WEMU gate, which hashes the
 * actual pixels rather than trusting a printed marker.
 *
 * C11, freestanding. One concern: fb terminal surface.
 */
#include "vbe.h"
#include <stdint.h>
#include <string.h>

/* Non-static on purpose: the guest gate locates these in the ELF symtab. */
uint64_t wubu_fbterm_fb;
uint64_t wubu_fbterm_hash;

#define FB_X 16
#define FB_Y 16
#define FB_W 640
#define FB_H 128

static char shown[8][128];
static int rows, cur;

static uint64_t hash_region(void)
{
    uint64_t h = 0xcbf29ce484222325ull;
    int hgt = vbe_height();
    int wdt = vbe_width();
    for (int y = FB_Y; y < FB_Y + FB_H && y < hgt; y += 4) {
        for (int x = FB_X; x < FB_X + FB_W && x < wdt; x += 4) {
            h ^= vbe_get_pixel(x, y);
            h *= 0x100000001b3ull;
        }
    }
    return h;
}

static void render(const char *line)
{
    uint32_t *fb = vbe_framebuffer();
    if (!fb) return;
    wubu_fbterm_fb = (uint64_t)(uintptr_t)fb;
    vbe_fill_rect(FB_X, FB_Y, FB_W, FB_H, 0x00101820u);
    vbe_rect(FB_X, FB_Y, FB_W, FB_H, 0x00406080u);
    vbe_draw_text(FB_X + 8, FB_Y + 8, "WuBuOS console (metal)",
                  0x00a8d8ffu, 1);
    int y = FB_Y + 28;
    for (int i = 0; i < rows; i++) {
        vbe_draw_text(FB_X + 8, y, shown[i], 0x00d0d0d0u, 1);
        y += 14;
    }
    if (line) vbe_draw_text(FB_X + 8, y, line, 0x00ffffffu, 1);
    wubu_fbterm_hash = hash_region();
}

void wubu_fbterm_current(const char *line)
{
    if (!vbe_framebuffer()) return;
    render(line);
}

void wubu_fbterm_submit(const char *line)
{
    if (!vbe_framebuffer()) return;
    if (rows < 8) {
        strncpy(shown[rows], line, sizeof shown[0] - 1);
        shown[rows][sizeof shown[0] - 1] = 0;
        rows++;
    } else {
        for (int i = 0; i < 7; i++) memcpy(shown[i], shown[i + 1], sizeof shown[0]);
        strncpy(shown[7], line, sizeof shown[0] - 1);
        shown[7][sizeof shown[0] - 1] = 0;
    }
    cur = 0;
    render("WuBuOS> ");
}
