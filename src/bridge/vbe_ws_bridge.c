/*
 * vbe_ws_bridge.c  --  VBE ↔ WorldSim Render Bridge Implementation
 *
 * Cell 070: Wires WorldSim software renderer to VBE framebuffer.
 * The bridge sets ws_render_ctx_t.fb to VBE's back-buffer,
 * so WorldSim renders directly into VBE. After rendering,
 * vbe_swap() flips the completed frame to the front buffer.
 *
 * All C11, no external deps beyond VBE + WorldSim.
 */

#include "vbe_ws_bridge.h"
#include <string.h>
#include <stdio.h>

/* -- Simple 8x16 VGA-style bitmap font for HUD text -------- */

/* Minimal digit + letter glyphs (5 wide, 7 tall, packed in uint8_t[5])
 * We use a simple 5x7 font stored as column bitmaps.
 * Each character is 5 bytes, each byte is a column, bits are rows top-to-bottom.
 */
static const uint8_t hud_font_digits[10][5] = {
    {0x3E,0x51,0x49,0x45,0x3E}, /* 0 */
    {0x00,0x42,0x7F,0x40,0x00}, /* 1 */
    {0x42,0x61,0x51,0x49,0x46}, /* 2 */
    {0x22,0x41,0x49,0x49,0x36}, /* 3 */
    {0x18,0x14,0x12,0x7F,0x10}, /* 4 */
    {0x27,0x45,0x45,0x45,0x39}, /* 5 */
    {0x3C,0x4A,0x49,0x49,0x31}, /* 6 */
    {0x41,0x20,0x10,0x08,0x07}, /* 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 8 */
    {0x06,0x49,0x49,0x29,0x1E}, /* 9 */
};

static const uint8_t hud_font_alpha[26][5] = {
    {0x3E,0x41,0x41,0x41,0x3E}, /* A */
    {0x7F,0x49,0x49,0x49,0x36}, /* B */
    {0x3E,0x41,0x41,0x41,0x22}, /* C */
    {0x7F,0x41,0x41,0x22,0x1C}, /* D */
    {0x7F,0x49,0x49,0x49,0x41}, /* E */
    {0x7F,0x09,0x09,0x09,0x01}, /* F */
    {0x3E,0x41,0x49,0x49,0x3A}, /* G */
    {0x7F,0x08,0x08,0x08,0x7F}, /* H */
    {0x00,0x41,0x7F,0x41,0x00}, /* I */
    {0x30,0x40,0x40,0x40,0x3F}, /* J */
    {0x7F,0x08,0x14,0x22,0x41}, /* K */
    {0x7F,0x40,0x40,0x40,0x40}, /* L */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* M */
    {0x7F,0x04,0x08,0x10,0x7F}, /* N */
    {0x3E,0x41,0x41,0x41,0x3E}, /* O */
    {0x7F,0x09,0x09,0x09,0x06}, /* P */
    {0x3E,0x51,0x51,0x51,0x2E}, /* Q */
    {0x7F,0x09,0x19,0x29,0x46}, /* R */
    {0x26,0x49,0x49,0x49,0x32}, /* S */
    {0x01,0x01,0x7F,0x01,0x01}, /* T */
    {0x3F,0x40,0x40,0x40,0x3F}, /* U */
    {0x0F,0x30,0x40,0x30,0x0F}, /* V */
    {0x7F,0x20,0x10,0x20,0x7F}, /* W */
    {0x63,0x14,0x08,0x14,0x63}, /* X */
    {0x07,0x08,0x70,0x08,0x07}, /* Y */
    {0x61,0x51,0x49,0x45,0x43}, /* Z */
};

/* Special chars */
static const uint8_t hud_font_colon[5]  = {0x00,0x36,0x00,0x36,0x00};
static const uint8_t hud_font_space[5]  = {0x00,0x00,0x00,0x00,0x00};
static const uint8_t hud_font_period[5] = {0x00,0x00,0x00,0x00,0x40};
static const uint8_t hud_font_dash[5]   = {0x00,0x00,0x7F,0x00,0x00};
static const uint8_t hud_font_lparen[5] = {0x0E,0x11,0x20,0x00,0x00};
static const uint8_t hud_font_rparen[5] = {0x00,0x20,0x11,0x0E,0x00};
static const uint8_t hud_font_slash[5]  = {0x01,0x02,0x04,0x08,0x10};
static const uint8_t hud_font_comma[5]  = {0x00,0x00,0x00,0x20,0x10};

/* Draw a single 5x7 glyph at (x,y) */
static void draw_glyph(VBEState *vbe, int x, int y,
                        const uint8_t glyph[5], uint32_t color) {
    if (!vbe || !vbe->back) return;
    for (int col = 0; col < 5; col++) {
        uint8_t bits = glyph[col];
        for (int row = 0; row < 7; row++) {
            if (bits & (1 << row)) {
                int px = x + col;
                int py = y + row;
                if (px >= 0 && px < vbe->width && py >= 0 && py < vbe->height)
                    vbe->back[py * vbe->width + px] = color;
            }
        }
    }
}

/* Get glyph for a character */
static const uint8_t *char_glyph(char c) {
    if (c >= '0' && c <= '9') return hud_font_digits[c - '0'];
    if (c >= 'A' && c <= 'Z') return hud_font_alpha[c - 'A'];
    if (c >= 'a' && c <= 'z') return hud_font_alpha[c - 'a']; /* lowercase→uppercase */
    switch (c) {
        case ':':  return hud_font_colon;
        case ' ':  return hud_font_space;
        case '.':  return hud_font_period;
        case '-':  return hud_font_dash;
        case '(':  return hud_font_lparen;
        case ')':  return hud_font_rparen;
        case '/':  return hud_font_slash;
        case ',':  return hud_font_comma;
        default:   return hud_font_space;
    }
}

/* -- Lifecycle ----------------------------------------------- */

void vbe_ws_bridge_init(vbe_ws_bridge_t *br) {
    memset(br, 0, sizeof(*br));
    br->state = BRIDGE_STOPPED;
    br->wired = 0;
    br->view_zoom = 1.0f;
    br->sim_speed = 1.0f;
    br->show_hud = 1;
    br->show_minimap = 1;
    br->show_fps = 1;
    br->show_entity_count = 1;
    br->minimap_size = 128;
}

int vbe_ws_bridge_wire(vbe_ws_bridge_t *br, ws_simulation_t *sim) {
    VBEState *vbe = vbe_state();
    if (!vbe || !vbe->back || !sim) return -1;

    br->vbe = vbe;
    br->sim = sim;
    br->wired = 1;

    /* Wire WorldSim render context to VBE back-buffer */
    sim->render.fb = vbe->back;
    sim->render.fb_w = vbe->width;
    sim->render.fb_h = vbe->height;

    /* Sync viewport from render context */
    br->view_x = sim->render.cam_x;
    br->view_y = sim->render.cam_y;
    br->view_zoom = sim->render.cam_z;
    /* Default zoom if not set by sim_init */
    if (br->view_zoom == 0.0f) {
        br->view_zoom = 1.0f;
        sim->render.cam_z = 1.0f;
    }
    br->view_w = vbe->width;
    br->view_h = vbe->height;

    /* Default minimap position: top-right */
    br->minimap_x = vbe->width - br->minimap_size - 6;
    br->minimap_y = 4;

    return 0;
}

void vbe_ws_bridge_unwire(vbe_ws_bridge_t *br) {
    if (!br->wired) return;
    if (br->sim) {
        br->sim->render.fb = NULL;
        br->sim->render.fb_w = 0;
        br->sim->render.fb_h = 0;
    }
    br->wired = 0;
    br->vbe = NULL;
    br->sim = NULL;
    br->state = BRIDGE_STOPPED;
}

void vbe_ws_bridge_start(vbe_ws_bridge_t *br) {
    if (!br->wired) return;
    br->state = BRIDGE_RUNNING;
}

void vbe_ws_bridge_pause(vbe_ws_bridge_t *br) {
    if (br->state == BRIDGE_RUNNING)
        br->state = BRIDGE_PAUSED;
}

void vbe_ws_bridge_resume(vbe_ws_bridge_t *br) {
    if (br->state == BRIDGE_PAUSED)
        br->state = BRIDGE_RUNNING;
}

void vbe_ws_bridge_stop(vbe_ws_bridge_t *br) {
    br->state = BRIDGE_STOPPED;
}

/* -- Camera / Viewport --------------------------------------- */

void vbe_ws_bridge_pan(vbe_ws_bridge_t *br, int dx, int dy) {
    br->view_x += dx;
    br->view_y += dy;
    if (br->sim) {
        br->sim->render.cam_x = br->view_x;
        br->sim->render.cam_y = br->view_y;
    }
}

void vbe_ws_bridge_set_view(vbe_ws_bridge_t *br, int x, int y) {
    br->view_x = x;
    br->view_y = y;
    if (br->sim) {
        br->sim->render.cam_x = x;
        br->sim->render.cam_y = y;
    }
}

void vbe_ws_bridge_zoom(vbe_ws_bridge_t *br, float zoom_delta) {
    br->view_zoom += zoom_delta;
    if (br->view_zoom < 0.25f) br->view_zoom = 0.25f;
    if (br->view_zoom > 4.0f)  br->view_zoom = 4.0f;
    if (br->sim) {
        br->sim->render.cam_z = br->view_zoom;
    }
}

void vbe_ws_bridge_center_on(vbe_ws_bridge_t *br, int wx, int wy) {
    br->view_x = wx - br->view_w / 2;
    br->view_y = wy - br->view_h / 2;
    if (br->sim) {
        br->sim->render.cam_x = br->view_x;
        br->sim->render.cam_y = br->view_y;
    }
}

/* -- HUD Drawing --------------------------------------------- */

int vbe_ws_bridge_text(vbe_ws_bridge_t *br, int x, int y,
                        const char *text, uint32_t color) {
    if (!br || !br->vbe) return x;
    VBEState *vbe = br->vbe;
    int cx = x;
    int len = (int)strlen(text);
    for (int i = 0; i < len; i++) {
        const uint8_t *g = char_glyph(text[i]);
        draw_glyph(vbe, cx, y, g, color);
        cx += 6; /* 5 pixels wide + 1 spacing */
    }
    return cx;
}

int vbe_ws_bridge_text_int(vbe_ws_bridge_t *br, int x, int y,
                            int value, uint32_t color) {
    char buf[16];
    if (value < 0) {
        buf[0] = '-';
        snprintf(buf + 1, sizeof(buf) - 1, "%d", -value);
    } else {
        snprintf(buf, sizeof(buf), "%d", value);
    }
    return vbe_ws_bridge_text(br, x, y, buf, color);
}

/* Draw a filled rect directly to VBE back-buffer (fast path) */
static void hud_fill_rect(VBEState *vbe, int x, int y, int w, int h, uint32_t color) {
    if (!vbe || !vbe->back) return;
    for (int dy = 0; dy < h; dy++) {
        for (int dx = 0; dx < w; dx++) {
            int px = x + dx, py = y + dy;
            if (px >= 0 && px < vbe->width && py >= 0 && py < vbe->height)
                vbe->back[py * vbe->width + px] = color;
        }
    }
}

/* Draw a rect outline to VBE back-buffer */
static void hud_rect_outline(VBEState *vbe, int x, int y, int w, int h, uint32_t color) {
    if (!vbe || !vbe->back) return;
    for (int dx = 0; dx < w; dx++) {
        if (x + dx >= 0 && x + dx < vbe->width) {
            if (y >= 0 && y < vbe->height)
                vbe->back[y * vbe->width + x + dx] = color;
            if (y + h - 1 >= 0 && y + h - 1 < vbe->height)
                vbe->back[(y + h - 1) * vbe->width + x + dx] = color;
        }
    }
    for (int dy = 0; dy < h; dy++) {
        if (y + dy >= 0 && y + dy < vbe->height) {
            if (x >= 0 && x < vbe->width)
                vbe->back[(y + dy) * vbe->width + x] = color;
            if (x + w - 1 >= 0 && x + w - 1 < vbe->width)
                vbe->back[(y + dy) * vbe->width + x + w - 1] = color;
        }
    }
}

void vbe_ws_bridge_draw_hud(vbe_ws_bridge_t *br) {
    if (!br || !br->wired || !br->vbe || !br->sim) return;
    VBEState *vbe = br->vbe;

    /* HUD background: semi-transparent dark strip at top */
    uint32_t hud_bg   = 0x80000000; /* semi-transparent black (XRGB) */
    uint32_t hud_fg   = 0x00FFFFFF; /* white text */
    uint32_t hud_fg2  = 0x00FFFF00; /* yellow for values */

    int hud_h = 14; /* HUD strip height */
    /* Dark strip background */
    hud_fill_rect(vbe, 0, 0, vbe->width, hud_h, 0x40000000);

    int cx = 4; /* current x position */
    int cy = 3; /* y position (top of text) */

    /* FPS display */
    if (br->show_fps) {
        cx = vbe_ws_bridge_text(br, cx, cy, "FPS:", hud_fg);
        cx = vbe_ws_bridge_text_int(br, cx, cy, (int)br->fps, hud_fg2);
        cx += 8; /* spacing */
    }

    /* Frame count */
    cx = vbe_ws_bridge_text(br, cx, cy, "F:", hud_fg);
    cx = vbe_ws_bridge_text_int(br, cx, cy, (int)(br->frame_count % 10000), hud_fg2);
    cx += 8;

    /* Tick display */
    cx = vbe_ws_bridge_text(br, cx, cy, "T:", hud_fg);
    cx = vbe_ws_bridge_text_int(br, cx, cy, (int)(br->sim->tick % 10000), hud_fg2);
    cx += 8;

    /* Entity count */
    if (br->show_entity_count) {
        cx = vbe_ws_bridge_text(br, cx, cy, "E:", hud_fg);
        cx = vbe_ws_bridge_text_int(br, cx, cy, br->sim->world.count, hud_fg2);
        cx += 8;
    }

    /* State indicator */
    if (br->state == BRIDGE_PAUSED) {
        cx = vbe_ws_bridge_text(br, cx, cy, "PAUSED", 0x00FF4444);
    } else if (br->state == BRIDGE_RUNNING) {
        cx = vbe_ws_bridge_text(br, cx, cy, "RUN", 0x0044FF44);
    }

    /* Camera position in bottom-left */
    int by = vbe->height - 12;
    cx = 4;
    cx = vbe_ws_bridge_text(br, cx, by, "CAM:", hud_fg);
    cx = vbe_ws_bridge_text_int(br, cx, by, br->view_x, hud_fg2);
    cx = vbe_ws_bridge_text(br, cx, by, ",", hud_fg);
    cx = vbe_ws_bridge_text_int(br, cx, by, br->view_y, hud_fg2);

    /* Zoom */
    cx += 8;
    cx = vbe_ws_bridge_text(br, cx, by, "Z:", hud_fg);
    /* Print zoom as integer (zoom*10) for simplicity with our font */
    cx = vbe_ws_bridge_text_int(br, cx, by, (int)(br->view_zoom * 10.0f), hud_fg2);

    /* Minimap */
    if (br->show_minimap && br->sim) {
        /* Render minimap via WorldSim */
        ws_render_minimap(&br->sim->terrain, &br->sim->render,
                          br->minimap_x, br->minimap_y, br->minimap_size);
    }
}

/* -- Per-Frame Operations ------------------------------------ */

void vbe_ws_bridge_frame(vbe_ws_bridge_t *br) {
    if (!br || !br->wired || br->state == BRIDGE_STOPPED) return;

    /* Advance simulation if running (not paused) */
    if (br->state == BRIDGE_RUNNING) {
        /* Apply simulation speed: at speed 2.0, step twice per frame */
        int steps = (int)br->sim_speed;
        if (steps < 1) steps = 1;
        for (int i = 0; i < steps; i++) {
            ws_sim_step(br->sim);
        }
    }

    /* Render: terrain → entities → minimap → HUD → swap */
    vbe_ws_bridge_render_only(br);

    br->frame_count++;
}

void vbe_ws_bridge_render_only(vbe_ws_bridge_t *br) {
    if (!br || !br->wired) return;

    /* Clear back-buffer to black */
    vbe_clear(0x000000FF);

    /* Re-wire render context (in case vbe_swap or resize changed pointers) */
    if (br->sim && br->vbe) {
        br->sim->render.fb = br->vbe->back;
        br->sim->render.fb_w = br->vbe->width;
        br->sim->render.fb_h = br->vbe->height;
    }

    /* Render WorldSim: terrain + entities */
    if (br->sim) {
        ws_render_terrain(&br->sim->terrain, &br->sim->render);
        ws_render_entities(&br->sim->world, &br->sim->render);
    }

    /* Draw HUD overlay */
    if (br->show_hud) {
        vbe_ws_bridge_draw_hud(br);
    }

    /* Flip: copy back-buffer to front-buffer */
    vbe_swap();
}

/* -- Query ---------------------------------------------------- */

int vbe_ws_bridge_is_active(const vbe_ws_bridge_t *br) {
    return br && br->wired && br->state == BRIDGE_RUNNING;
}

uint64_t vbe_ws_bridge_frame_count(const vbe_ws_bridge_t *br) {
    return br ? br->frame_count : 0;
}

float vbe_ws_bridge_fps(const vbe_ws_bridge_t *br) {
    return br ? br->fps : 0.0f;
}
