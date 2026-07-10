#include "worldsim.h"
#include <string.h>

/* Biome colors (XRGB8888) */
static const uint32_t biome_colors[] = {
    0x004488FF, /* water    --  deep blue */
    0x44AA22FF, /* plains   --  green */
    0x227711FF, /* forest   --  dark green */
    0x888888FF, /* mountain --  gray */
    0xDDDDDDFF, /* snow     --  white */
};

static const uint32_t biome_colors_shaded[] = {
    0x003366FF, /* water shadow */
    0x338811FF, /* plains shadow */
    0x1A5500FF, /* forest shadow */
    0x666666FF, /* mountain shadow */
    0xAAAAAAAA, /* snow shadow */
};

void ws_render_terrain(const ws_terrain_t *t, ws_render_ctx_t *ctx) {
    if (!ctx->fb || !t) return;
    
    /* Top-down biome map render */
    for (int y = 0; y < ctx->fb_h && y < t->height; y++) {
        for (int x = 0; x < ctx->fb_w && x < t->width; x++) {
            int tx = x + ctx->cam_x;
            int ty = y + ctx->cam_y;
            if (tx >= t->width || ty >= t->height) {
                ctx->fb[y * ctx->fb_w + x] = 0x000000FF; /* black */
                continue;
            }
            uint8_t biome = t->biome[ty * t->width + tx];
            uint8_t h = t->heightmap[ty * t->width + tx];
            
            /* Shade by height within biome */
            uint32_t base = biome_colors[biome];
            int shade = h / 4; /* 0-63 intensity shift */
            uint32_t r = (base >> 24) & 0xFF;
            uint32_t g = (base >> 16) & 0xFF;
            uint32_t b = (base >> 8) & 0xFF;
            
            /* Height shading: brighter = higher */
            r = (r + shade > 255) ? 255 : r + shade;
            g = (g + shade > 255) ? 255 : g + shade;
            b = (b + shade > 255) ? 255 : b + shade;
            
            ctx->fb[y * ctx->fb_w + x] = (r << 24) | (g << 16) | (b << 8) | 0xFF;
        }
    }
}

void ws_render_entities(const ws_world_t *w, ws_render_ctx_t *ctx) {
    if (!ctx->fb) return;
    
    for (int i = 0; i < WS_MAX_ENTITIES; i++) {
        ws_entity_t *e = &w->entities[i];
        if (!e->active) continue;
        if (!(e->component_mask & (1 << WS_COMP_POSITION))) continue;
        if (!(e->component_mask & (1 << WS_COMP_SPRITE))) continue;
        
        ws_position_t *pos = (ws_position_t *)&e->components[WS_COMP_POSITION];
        ws_sprite_t *spr = (ws_sprite_t *)&e->components[WS_COMP_SPRITE];
        
        int sx = (int)pos->x - ctx->cam_x;
        int sy = (int)pos->y - ctx->cam_y;
        
        /* Blit sprite */
        if (spr->pixels) {
            for (int py = 0; py < spr->h; py++) {
                for (int px = 0; px < spr->w; px++) {
                    int dx = sx + px, dy = sy + py;
                    if (dx >= 0 && dx < ctx->fb_w && dy >= 0 && dy < ctx->fb_h) {
                        uint8_t a = spr->pixels[(py * spr->w + px) * 4 + 3];
                        if (a > 128) /* simple alpha test */
                            ctx->fb[dy * ctx->fb_w + dx] = 
                                *(uint32_t *)&spr->pixels[(py * spr->w + px) * 4];
                    }
                }
            }
        } else {
            /* No pixels  --  draw rect placeholder */
            for (int py = 0; py < spr->h && sy + py < ctx->fb_h; py++) {
                for (int px = 0; px < spr->w && sx + px < ctx->fb_w; px++) {
                    int dx = sx + px, dy = sy + py;
                    if (dx >= 0 && dy >= 0)
                        ctx->fb[dy * ctx->fb_w + dx] = 0xFF00FFFF; /* magenta */
                }
            }
        }
    }
}

void ws_render_minimap(const ws_terrain_t *t, ws_render_ctx_t *ctx, int mx, int my, int size) {
    if (!ctx->fb || !t) return;
    
    /* Render scaled-down terrain to a minimap box */
    float scale_x = (float)t->width / size;
    float scale_y = (float)t->height / size;
    
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int tx = (int)(x * scale_x);
            int ty = (int)(y * scale_y);
            uint8_t biome = t->biome[ty * t->width + tx];
            uint32_t color = biome_colors[biome];
            
            int dx = mx + x, dy = my + y;
            if (dx >= 0 && dx < ctx->fb_w && dy >= 0 && dy < ctx->fb_h)
                ctx->fb[dy * ctx->fb_w + dx] = color;
        }
    }
    
    /* Border */
    for (int i = 0; i < size; i++) {
        if (my + i < ctx->fb_h) {
            if (mx < ctx->fb_w) ctx->fb[(my+i) * ctx->fb_w + mx] = 0xFFFFFFFF;
            if (mx+size < ctx->fb_w) ctx->fb[(my+i) * ctx->fb_w + mx+size] = 0xFFFFFFFF;
        }
        if (mx + i < ctx->fb_w) {
            if (my < ctx->fb_h) ctx->fb[my * ctx->fb_w + mx+i] = 0xFFFFFFFF;
            if (my+size < ctx->fb_h) ctx->fb[(my+size) * ctx->fb_w + mx+i] = 0xFFFFFFFF;
        }
    }
}
