/* wubu_syscall_vbe.c -- VBE syscall handlers (self-contained).
 *
 * sys_vbe_*: thin wrappers translating int64 args -> kernel vbe_* API.
 * Declared in wubu_syscall.h; vbe_* in kernel/vbe.h. Minimal includes.
 */

#include "wubu_syscall.h"
#include "vbe.h"

int64_t sys_vbe_fill_rect(int64_t x, int64_t y, int64_t w, int64_t h, int64_t color, int64_t _unused) {
    (void)_unused;
    vbe_fill_rect((int)x, (int)y, (int)w, (int)h, (uint32_t)color);
    return 0;
}

int64_t sys_vbe_fill_circle(int64_t x, int64_t y, int64_t r, int64_t color, int64_t _unused, int64_t _unused2) {
    (void)_unused; (void)_unused2;
    vbe_fill_circle((int)x, (int)y, (int)r, (uint32_t)color);
    return 0;
}

int64_t sys_vbe_draw_text(int64_t x, int64_t y, int64_t str_ptr, int64_t color, int64_t scale, int64_t _unused) {
    (void)_unused;
    const char *s = (const char *)str_ptr;
    if (s) vbe_draw_text((int)x, (int)y, s, (uint32_t)color, (int)scale);
    return 0;
}

int64_t sys_vbe_draw_char(int64_t x, int64_t y, int64_t c, int64_t color, int64_t scale, int64_t _unused) {
    (void)_unused;
    vbe_draw_char((int)x, (int)y, (char)c, (uint32_t)color, (int)scale);
    return 0;
}

int64_t sys_vbe_vline(int64_t x, int64_t y1, int64_t y2, int64_t color, int64_t _unused, int64_t _unused2) {
    (void)_unused; (void)_unused2;
    vbe_vline((int)x, (int)y1, (int)y2, (uint32_t)color);
    return 0;
}

int64_t sys_vbe_hline(int64_t x1, int64_t x2, int64_t y, int64_t color, int64_t _unused, int64_t _unused2) {
    (void)_unused; (void)_unused2;
    vbe_hline((int)x1, (int)x2, (int)y, (uint32_t)color);
    return 0;
}

int64_t sys_vbe_text_width(int64_t str_ptr, int64_t scale, int64_t _unused, int64_t _unused2, int64_t _unused3, int64_t _unused4) {
    (void)_unused; (void)_unused2; (void)_unused3; (void)_unused4;
    const char *s = (const char *)str_ptr;
    if (!s) return 0;
    return (int64_t)vbe_text_width(s, (int)scale);
}

int64_t sys_vbe_swap(int64_t _unused, int64_t _unused2, int64_t _unused3, int64_t _unused4, int64_t _unused5, int64_t _unused6) {
    (void)_unused; (void)_unused2; (void)_unused3; (void)_unused4; (void)_unused5; (void)_unused6;
    vbe_swap();
    return 0;
}
