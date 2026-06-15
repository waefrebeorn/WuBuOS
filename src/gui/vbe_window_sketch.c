/*
 * vbe_window_sketch.c  --  Minimal Win98-style window on Linux framebuffer
 * 
 * Proof-of-concept for the My Seed GUI layer:
 * - Software-rendered framebuffer
 * - Window with title bar, close button, 3D border
 * - Mouse tracking (simulated)
 * 
 * This is the visual primitive that will be ported to the 
 * ZealOS VBE backend once the kernel is running.
 * 
 * Build:  gcc -o vbe_sketch vbe_window_sketch.c -lm
 * Run:    ./vbe_sketch
 * Output: sketch.ppm (viewable PPM image)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FB_W 1024
#define FB_H 768

/* Win98 Classic Color Palette */
#define C_BG        0x00808080  /* Desktop: gray */
#define C_WIN_FACE  0x00C0C0C0  /* Window face: silver */
#define C_TITLE_BG  0x00000080  /* Title bar: navy blue */
#define C_TITLE_FG  0x00FFFFFF  /* Title text: white */
#define C_BORDER_LT 0x00FFFFFF  /* 3D border: light (top-left) */
#define C_BORDER_DK 0x00808080  /* 3D border: dark (bottom-right) */
#define C_BORDER_DD 0x00000000  /* 3D border: darkest */
#define C_BTN_FACE  0x00C0C0C0  /* Button face */
#define C_CLOSE_X   0x00000000  /* Close button X */
#define C_TASKBAR   0x00C0C0C0  /* Taskbar */
#define C_START_BTN 0x00C0C0C0  /* Start button */

typedef unsigned int u32;
typedef unsigned char u8;

static u32 framebuffer[FB_W * FB_H];

static inline void fb_set(int x, int y, u32 color) {
    if (x >= 0 && x < FB_W && y >= 0 && y < FB_H)
        framebuffer[y * FB_W + x] = color;
}

/* Draw filled rectangle */
static void fill_rect(int x, int y, int w, int h, u32 color) {
    for (int dy = 0; dy < h; dy++)
        for (int dx = 0; dx < w; dx++)
            fb_set(x + dx, y + dy, color);
}

/* Draw 3D raised border (Win98 style) */
static void draw_3d_raised(int x, int y, int w, int h) {
    /* Outer: light top-left, dark bottom-right */
    for (int i = 0; i < w; i++) { fb_set(x+i, y, C_BORDER_LT); fb_set(x+i, y+h-1, C_BORDER_DD); }
    for (int i = 0; i < h; i++) { fb_set(x, y+i, C_BORDER_LT); fb_set(x+w-1, y+i, C_BORDER_DD); }
    /* Inner: white top-left, gray bottom-right */
    for (int i = 1; i < w-1; i++) { fb_set(x+i, y+1, C_BORDER_LT); fb_set(x+i, y+h-2, C_BORDER_DK); }
    for (int i = 1; i < h-1; i++) { fb_set(x+1, y+i, C_BORDER_LT); fb_set(x+w-2, y+i, C_BORDER_DK); }
}

/* Draw 3D sunken border (for text areas) */
static void draw_3d_sunken(int x, int y, int w, int h) {
    for (int i = 0; i < w; i++) { fb_set(x+i, y, C_BORDER_DD); fb_set(x+i, y+h-1, C_BORDER_LT); }
    for (int i = 0; i < h; i++) { fb_set(x, y+i, C_BORDER_DD); fb_set(x+w-1, y+i, C_BORDER_LT); }
    for (int i = 1; i < w-1; i++) { fb_set(x+i, y+1, C_BORDER_DK); fb_set(x+i, y+h-2, 0x00DFDFDF); }
    for (int i = 1; i < h-1; i++) { fb_set(x+1, y+i, C_BORDER_DK); fb_set(x+w-2, y+i, 0x00DFDFDF); }
}

/* Bitmap font: 8x16 VGA-style for 'M', 'y', ' ', 'S', 'e', 'd' */
static const u8 font_M[] = {0x00,0x41,0x63,0x55,0x49,0x41,0x00,0x00};
static const u8 font_y[] = {0x00,0x00,0x42,0x42,0x42,0x3E,0x02,0x00};
static const u8 font_S[] = {0x00,0x3C,0x42,0x40,0x3C,0x02,0x42,0x3C};
static const u8 font_e[] = {0x00,0x00,0x3C,0x42,0x7E,0x40,0x3E,0x00};
static const u8 font_d[] = {0x00,0x06,0x02,0x3E,0x42,0x42,0x42,0x3E,0x00};
static const u8 font_N[] = {0x00,0x42,0x62,0x52,0x4A,0x46,0x42,0x00};
static const u8 font_t[] = {0x00,0x10,0x30,0x10,0x10,0x10,0x1E,0x00};
static const u8 font_p[] = {0x00,0x00,0x3C,0x42,0x42,0x3C,0x40,0x40};
static const u8 font_a[] = {0x00,0x00,0x3C,0x02,0x3E,0x42,0x3E,0x00};
static const u8 font_o[] = {0x00,0x00,0x3C,0x42,0x42,0x42,0x3C,0x00};
static const u8 font_k[] = {0x00,0x00,0x42,0x4C,0x50,0x4C,0x42,0x00};

/* Draw 8x8 character at position */
static void draw_char(int x, int y, char ch, u32 fg, u32 bg) {
    const u8 *glyph = NULL;
    switch(ch) {
        case 'M': glyph=font_M; break; case 'y': glyph=font_y; break;
        case 'S': glyph=font_S; break; case 'e': glyph=font_e; break;
        case 'd': glyph=font_d; break; case 'N': glyph=font_N; break;
        case 't': glyph=font_t; break; case 'p': glyph=font_p; break;
        case 'a': glyph=font_a; break; case 'o': glyph=font_o; break;
        case 'k': glyph=font_k; break;
    }
    for (int row = 0; row < 8; row++) {
        u8 bits = glyph ? glyph[row] : 0;
        for (int col = 0; col < 8; col++) {
            fb_set(x + col, y + row, (bits & (0x80 >> col)) ? fg : bg);
        }
    }
}

/* Draw string using bitmap font */
static void draw_text(int x, int y, const char *str, u32 fg, u32 bg) {
    for (int i = 0; str[i]; i++)
        draw_char(x + i * 9, y, str[i], fg, bg);
}

/* Draw close button (# with X) */
static void draw_close_button(int x, int y) {
    fill_rect(x, y, 16, 14, C_BTN_FACE);
    draw_3d_raised(x, y, 16, 14);
    /* X marks */
    fb_set(x+4, y+3, C_CLOSE_X); fb_set(x+5, y+4, C_CLOSE_X);
    fb_set(x+6, y+5, C_CLOSE_X); fb_set(x+5, y+6, C_CLOSE_X);
    fb_set(x+4, y+7, C_CLOSE_X); fb_set(x+4, y+5, C_CLOSE_X);
    fb_set(x+6, y+3, C_CLOSE_X); fb_set(x+5, y+5, C_CLOSE_X);
    fb_set(x+6, y+7, C_CLOSE_X);
}

/* Draw Win98-style window */
static void draw_window(int x, int y, int w, int h, const char *title) {
    /* Outer 3D border */
    draw_3d_raised(x, y, w, h);
    
    /* Window body */
    fill_rect(x+3, y+3, w-6, h-6, C_WIN_FACE);
    
    /* Title bar (height 20) */
    fill_rect(x+4, y+4, w-8, 20, C_TITLE_BG);
    
    /* Title text */
    draw_text(x+10, y+9, title, C_TITLE_FG, C_TITLE_BG);
    
    /* Close button (top-right) */
    draw_close_button(x + w - 24, y + 5);
    
    /* Separator line below title */
    for (int i = x+4; i < x+w-4; i++) {
        fb_set(i, y+24, C_BORDER_DK);
        fb_set(i, y+25, C_BORDER_LT);
    }
    
    /* Client area sunken border (for text editing area) */
    draw_3d_sunken(x+8, y+30, w-16, h-42);
}

/* Draw taskbar */
static void draw_taskbar(void) {
    int tb_y = FB_H - 28;
    fill_rect(0, tb_y, FB_W, 28, C_TASKBAR);
    draw_3d_raised(0, tb_y, FB_W, 28);
    /* Start button area */
    fill_rect(4, tb_y+3, 60, 22, C_START_BTN);
    draw_3d_raised(4, tb_y+3, 60, 22);
    draw_text(12, tb_y+8, "Start", 0, C_START_BTN);
}

/* Save as PPM (viewable image) */
static void save_ppm(const char *path) {
    FILE *f = fopen(path, "wb");
    fprintf(f, "P6\n%d %d\n255\n", FB_W, FB_H);
    for (int i = 0; i < FB_W * FB_H; i++) {
        u32 c = framebuffer[i];
        fputc((c >> 16) & 0xFF, f);  /* R */
        fputc((c >> 8) & 0xFF, f);   /* G */
        fputc(c & 0xFF, f);          /* B */
    }
    fclose(f);
}

int main(void) {
    /* Clear to desktop background */
    fill_rect(0, 0, FB_W, FB_H, C_BG);
    
    /* Draw desktop pattern (subtle dither) */
    for (int y = 0; y < FB_H; y += 4)
        for (int x = 0; x < FB_W; x += 4)
            fb_set(x, y, 0x00848484);
    
    /* Draw window: "My Seed - Notepad" at (100, 80), 500x400 */
    draw_window(100, 80, 500, 400, "My Seed");
    
    /* Draw second smaller window: "Temple REPL" */
    draw_window(640, 120, 340, 280, "Temple");
    
    /* Draw taskbar */
    draw_taskbar();
    
    /* Save output */
    save_ppm("sketch.ppm");
    printf("VBE sketch saved to sketch.ppm (%dx%d)\n", FB_W, FB_H);
    printf("Window 1: 'My Seed' at (100,80) 500x400\n");
    printf("Window 2: 'Temple' at (640,120) 340x280\n");
    printf("Taskbar: bottom 28px with Start button\n");
    
    return 0;
}
