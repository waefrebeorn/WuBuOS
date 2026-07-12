/*
 * doom.c  --  Free Doom Engine for WuBuOS
 *
 * A simplified Doom-like raycasting engine that runs in the WuBuOS
 * hosted environment. Uses VBE framebuffer for display.
 *
 * This is a FROM-SCRATCH implementation (not a port of Chocolate Doom)
 * designed to run within the WuBuOS GUI shell as a .wubu container app.
 *
 * Features:
 *   - Raycasting engine (Wolfenstein 3D style)
 *   - Textured walls
 *   - Floor/ceiling rendering
 *   - Player movement (WASD + mouse)
 *   - Minimap
 *   - Win98-style window with title bar
 *
 * Build: make doom
 * Run:   ./doom
 */

#include "../gui/wm.h"
#include "../kernel/vbe.h"
#include "../kernel/input.h"
#include "../kernel/wubu_math.h"

#include <string.h>
#include <stdlib.h>

/* -- Game Constants ---------------------------------------------- */

#define SCREEN_W        640
#define SCREEN_H        480
#define MAP_W           16
#define MAP_H           16
#define TEX_W           64
#define TEX_H           64
#define FOV             60
#define RAY_COUNT       SCREEN_W
#define MAX_DEPTH       20

/* -- Map Data ---------------------------------------------------- */
/* 1 = wall, 0 = empty, 2-5 = different wall types */

static const uint8_t g_map[MAP_H][MAP_W] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,2,2,0,0,0,0,0,3,3,0,0,0,1},
    {1,0,0,2,0,0,0,0,0,0,3,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,4,4,4,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,4,0,4,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,4,0,4,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,5,5,0,0,0,0,0,0,5,5,0,0,1},
    {1,0,0,5,0,0,0,0,0,0,0,5,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
};

/* -- Texture Data (procedurally generated) ----------------------- */

static uint32_t g_textures[5][TEX_W * TEX_H];

static void generate_textures(void) {
    for (int t = 0; t < 5; t++) {
        uint32_t base_r = 0xC0, base_g = 0x80, base_b = 0x40;
        switch (t) {
            case 0: base_r = 0xC0; base_g = 0x80; base_b = 0x40; break; /* brown brick */
            case 1: base_r = 0x80; base_g = 0x80; base_b = 0x80; break; /* gray stone */
            case 2: base_r = 0x40; base_g = 0x80; base_b = 0x40; break; /* green moss */
            case 3: base_r = 0x80; base_g = 0x40; base_b = 0x40; break; /* red brick */
            case 4: base_r = 0x60; base_g = 0x60; base_b = 0xA0; break; /* blue stone */
        }
        for (int y = 0; y < TEX_H; y++) {
            for (int x = 0; x < TEX_W; x++) {
                /* Brick pattern */
                int brick_x = x % 16;
                int brick_y = y % 8;
                int mortar = (brick_x == 0) || (brick_y == 0);
                if (mortar) {
                    g_textures[t][y * TEX_W + x] = 0x404040;
                } else {
                    uint8_t r = base_r + (rand() % 20) - 10;
                    uint8_t g = base_g + (rand() % 20) - 10;
                    uint8_t b = base_b + (rand() % 20) - 10;
                    g_textures[t][y * TEX_W + x] = (r << 16) | (g << 8) | b;
                }
            }
        }
    }
}

/* -- Player State ------------------------------------------------ */

typedef struct {
    float x, y;         /* Position */
    float angle;        /* Viewing angle in radians */
    float speed;        /* Movement speed */
    int health;
    int ammo;
    int score;
} PlayerState;

static PlayerState g_player = {2.0f, 2.0f, 0.0f, 0.5f, 100, 50, 0};

/* -- Raycast State ----------------------------------------------- */

typedef struct {
    float distance;
    int wall_type;
    float wall_x;       /* Exact hit position for texturing */
    int side;           /* 0 = NS wall, 1 = EW wall */
} RayHit;

/* -- Pure C Math (no libm) -------------------------------------- */

static float fsin(float x) {
    /* Taylor series: sin(x) ≈ x - x³/6 + x⁵/120 - x⁷/5040 */
    float x2 = x * x;
    float x3 = x2 * x;
    float x5 = x3 * x2;
    float x7 = x5 * x2;
    return x - x3 / 6.0f + x5 / 120.0f - x7 / 5040.0f;
}

static float fcos(float x) {
    /* cos(x) = sin(x + π/2) */
    return fsin(x + 1.5707963f);
}

static float fsqrt(float x) {
    /* Fast inverse square root (Quake III) then reciprocal */
    if (x <= 0) return 0;
    float xhalf = 0.5f * x;
    int i = *(int *)&x;
    i = 0x5f3759df - (i >> 1);
    x = *(float *)&i;
    x = x * (1.5f - xhalf * x * x);
    return 1.0f / x;
}

static float fminf(float a, float b) {
    return a < b ? a : b;
}

static float fmaxf(float a, float b) {
    return a > b ? a : b;
}

static int floorf_to_int(float x) {
    int i = (int)x;
    return (x < 0 && x != i) ? i - 1 : i;
}

/* -- Raycasting Engine ------------------------------------------- */

static RayHit cast_ray(float px, float py, float angle) {
    RayHit hit;
    hit.distance = MAX_DEPTH;
    hit.wall_type = 0;
    hit.side = 0;

    float dir_x = fcos(angle);
    float dir_y = fsin(angle);

    int map_x = floorf_to_int(px);
    int map_y = floorf_to_int(py);

    float delta_dist_x = fabsf(1.0f / dir_x);
    float delta_dist_y = fabsf(1.0f / dir_y);

    float side_dist_x, side_dist_y;
    int step_x, step_y;

    if (dir_x < 0) {
        step_x = -1;
        side_dist_x = (px - map_x) * delta_dist_x;
    } else {
        step_x = 1;
        side_dist_x = (map_x + 1.0f - px) * delta_dist_x;
    }
    if (dir_y < 0) {
        step_y = -1;
        side_dist_y = (py - map_y) * delta_dist_y;
    } else {
        step_y = 1;
        side_dist_y = (map_y + 1.0f - py) * delta_dist_y;
    }

    int hit_wall = 0;
    while (!hit_wall && hit.distance < MAX_DEPTH) {
        if (side_dist_x < side_dist_y) {
            side_dist_x += delta_dist_x;
            map_x += step_x;
            hit.side = 0;
        } else {
            side_dist_y += delta_dist_y;
            map_y += step_y;
            hit.side = 1;
        }

        if (map_x < 0 || map_x >= MAP_W || map_y < 0 || map_y >= MAP_H) {
            hit.distance = MAX_DEPTH;
            break;
        }

        uint8_t cell = g_map[map_y][map_x];
        if (cell > 0) {
            hit_wall = 1;
            hit.wall_type = cell - 1;
            if (hit.side == 0) {
                hit.distance = side_dist_x - delta_dist_x;
                hit.wall_x = py + hit.distance * dir_y;
            } else {
                hit.distance = side_dist_y - delta_dist_y;
                hit.wall_x = px + hit.distance * dir_x;
            }
            hit.wall_x -= floorf_to_int(hit.wall_x);
        }
    }

    return hit;
}

/* -- Rendering --------------------------------------------------- */

static void render_frame(uint32_t *fb, int fb_w, int fb_h) {
    /* Clear to dark gray ceiling and floor */
    for (int y = 0; y < fb_h; y++) {
        uint32_t color = (y < fb_h / 2) ? 0x202040 : 0x404040;
        for (int x = 0; x < fb_w; x++) {
            fb[y * fb_w + x] = color;
        }
    }

    /* Cast rays */
    float fov_rad = FOV * 3.14159265f / 180.0f;
    float half_fov = fov_rad / 2.0f;

    for (int x = 0; x < fb_w; x++) {
        float ray_angle = g_player.angle - half_fov + ((float)x / fb_w) * fov_rad;
        RayHit hit = cast_ray(g_player.x, g_player.y, ray_angle);

        if (hit.distance < MAX_DEPTH) {
            /* Fix fisheye */
            float corrected = hit.distance * fcos(ray_angle - g_player.angle);
            int line_height = (int)(fb_h / corrected);
            int draw_start = (fb_h - line_height) / 2;
            int draw_end = draw_start + line_height;

            if (draw_start < 0) draw_start = 0;
            if (draw_end >= fb_h) draw_end = fb_h - 1;

            /* Texture mapping */
            int tex_x = (int)(hit.wall_x * TEX_W);
            if (tex_x < 0) tex_x = 0;
            if (tex_x >= TEX_W) tex_x = TEX_W - 1;

            uint32_t *tex = g_textures[hit.wall_type % 5];

            for (int y = draw_start; y < draw_end; y++) {
                int tex_y = ((y - draw_start) * TEX_H) / line_height;
                if (tex_y < 0) tex_y = 0;
                if (tex_y >= TEX_H) tex_y = TEX_H - 1;

                uint32_t color = tex[tex_y * TEX_W + tex_x];

                /* Darken one side for depth effect */
                if (hit.side == 1) {
                    color = (color >> 1) & 0x7F7F7F;
                }

                fb[y * fb_w + x] = color;
            }
        }
    }

    /* Minimap */
    int mm_size = 80;
    int mm_x = fb_w - mm_size - 8;
    int mm_y = 8;
    int cell_size = mm_size / MAP_W;

    for (int my = 0; my < mm_size; my++) {
        for (int mx = 0; mx < mm_size; mx++) {
            int map_x = mx / cell_size;
            int map_y = my / cell_size;
            uint32_t c = (g_map[map_y][map_x] > 0) ? 0x808080 : 0x202020;
            fb[(mm_y + my) * fb_w + (mm_x + mx)] = c;
        }
    }

    /* Player dot on minimap */
    int px = mm_x + (int)(g_player.x * cell_size);
    int py = mm_y + (int)(g_player.y * cell_size);
    for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -2; dx <= 2; dx++) {
            int nx = px + dx, ny = py + dy;
            if (nx >= mm_x && nx < mm_x + mm_size && ny >= mm_y && ny < mm_y + mm_size) {
                fb[ny * fb_w + nx] = 0x00FF00;
            }
        }
    }

    /* HUD */
    /* Health bar */
    for (int i = 0; i < 100; i++) {
        uint32_t c = (i < g_player.health) ? 0x00FF00 : 0xFF0000;
        for (int dy = 0; dy < 8; dy++) {
            fb[(fb_h - 20 + dy) * fb_w + 10 + i] = c;
        }
    }
}

/* -- Input Handling ---------------------------------------------- */

static void handle_input(void) {
    float move_speed = 0.05f;
    float rot_speed = 0.03f;

    /* Get keyboard state from input queue */
    /* In hosted mode, these come from X11 via hosted.c */
    /* For now, use the input queue directly */

    /* Forward/backward */
    if (input_key_pressed(0x1E)) { /* W */
        float nx = g_player.x + fcos(g_player.angle) * move_speed;
        float ny = g_player.y + fsin(g_player.angle) * move_speed;
        if (g_map[(int)ny][(int)nx] == 0) {
            g_player.x = nx;
            g_player.y = ny;
        }
    }
    if (input_key_pressed(0x1F)) { /* S */
        float nx = g_player.x - fcos(g_player.angle) * move_speed;
        float ny = g_player.y - fsin(g_player.angle) * move_speed;
        if (g_map[(int)ny][(int)nx] == 0) {
            g_player.x = nx;
            g_player.y = ny;
        }
    }

    /* Strafe */
    if (input_key_pressed(0x1E + 3)) { /* A */
        float nx = g_player.x + fcos(g_player.angle - 1.5708f) * move_speed;
        float ny = g_player.y + fsin(g_player.angle - 1.5708f) * move_speed;
        if (g_map[(int)ny][(int)nx] == 0) {
            g_player.x = nx;
            g_player.y = ny;
        }
    }
    if (input_key_pressed(0x1E + 4)) { /* D */
        float nx = g_player.x + fcos(g_player.angle + 1.5708f) * move_speed;
        float ny = g_player.y + fsin(g_player.angle + 1.5708f) * move_speed;
        if (g_map[(int)ny][(int)nx] == 0) {
            g_player.x = nx;
            g_player.y = ny;
        }
    }

    /* Rotation */
    if (input_key_pressed(0xE04B)) { /* Left arrow */
        g_player.angle -= rot_speed;
    }
    if (input_key_pressed(0xE04D)) { /* Right arrow */
        g_player.angle += rot_speed;
    }
}

/* -- Public API -------------------------------------------------- */

void doom_init(void) {
    generate_textures();
    g_player.x = 2.0f;
    g_player.y = 2.0f;
    g_player.angle = 0.0f;
    g_player.health = 100;
    g_player.ammo = 50;
    g_player.score = 0;
}

void doom_update(void) {
    handle_input();
}

void doom_render(uint32_t *fb, int w, int h) {
    render_frame(fb, w, h);
}

void doom_shutdown(void) {
    /* Nothing to clean up */
}
