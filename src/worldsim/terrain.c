#include "worldsim.h"
#include "wubu_math.h"

#include <string.h>

static uint64_t rng_state;

/* -- RNG -- */

uint64_t ws_rng_next(uint64_t *state) {
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

float ws_rng_float(uint64_t *state) {
    return (float)(ws_rng_next(state) & 0xFFFFFF) / (float)0xFFFFFF;
}

int ws_rng_int(uint64_t *state, int lo, int hi) {
    return lo + (int)(ws_rng_float(state) * (hi - lo));
}

static float noise2d(uint64_t *rng, int x, int y, int freq) {
    /* Hash-based value noise  --  good enough for terrain */
    uint64_t h = (uint64_t)x * 374761393ULL + (uint64_t)y * 668265263ULL + (uint64_t)freq * 1274126177ULL;
    h ^= h >> 13; h *= 1274126177ULL; h ^= h >> 16;
    *rng = h;
    return ws_rng_float(rng);
}

static float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

static float smooth_noise2d(uint64_t *rng, float x, float y, int freq) {
    int ix = (int)x, iy = (int)y;
    float fx = x - ix, fy = y - iy;
    /* Smoothstep */
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    float v00 = noise2d(rng, ix, iy, freq);
    float v10 = noise2d(rng, ix+1, iy, freq);
    float v01 = noise2d(rng, ix, iy+1, freq);
    float v11 = noise2d(rng, ix+1, iy+1, freq);
    return lerp(lerp(v00, v10, fx), lerp(v01, v11, fx), fy);
}

void ws_terrain_generate(ws_terrain_t *t, uint32_t seed) {
    t->seed = seed;
    t->width = WS_TERRAIN_W;
    t->height = WS_TERRAIN_H;
    
    /* Multi-octave noise */
    for (int y = 0; y < WS_TERRAIN_H; y++) {
        for (int x = 0; x < WS_TERRAIN_W; x++) {
            uint64_t local_rng = seed;
            float val = 0.0f;
            float amp = 1.0f;
            float freq = 1.0f;
            float max_amp = 0.0f;
            
            for (int oct = 0; oct < 6; oct++) {
                val += smooth_noise2d(&local_rng, x * freq / 64.0f, y * freq / 64.0f, (int)freq) * amp;
                max_amp += amp;
                amp *= 0.5f;
                freq *= 2.0f;
            }
            val /= max_amp;
            
            /* Island falloff  --  distance from center */
            float cx = (float)x / WS_TERRAIN_W - 0.5f;
            float cy = (float)y / WS_TERRAIN_H - 0.5f;
            float dist = (float)wubu_sqrt((double)(cx*cx + cy*cy)) * 2.0f;
            float falloff = 1.0f - dist * dist;
            if (falloff < 0.0f) falloff = 0.0f;
            val *= falloff;
            
            int h = (int)(val * WS_HEIGHT_SCALE);
            if (h < 0) h = 0;
            if (h > 255) h = 255;
            t->heightmap[y * WS_TERRAIN_W + x] = (uint8_t)h;
            
            /* Biome from height */
            uint8_t biome;
            if (h < 40) biome = 0;       /* water */
            else if (h < 70) biome = 1;  /* plains */
            else if (h < 120) biome = 2; /* forest */
            else if (h < 180) biome = 3; /* mountain */
            else biome = 4;              /* snow */
            t->biome[y * WS_TERRAIN_W + x] = biome;
        }
    }
}

void ws_terrain_erode(ws_terrain_t *t, int iterations) {
    /* Simple thermal erosion  --  smooth steep slopes */
    for (int iter = 0; iter < iterations; iter++) {
        for (int y = 1; y < WS_TERRAIN_H - 1; y++) {
            for (int x = 1; x < WS_TERRAIN_W - 1; x++) {
                int idx = y * WS_TERRAIN_W + x;
                int h = t->heightmap[idx];
                int hn = t->heightmap[idx - WS_TERRAIN_W];
                int hs = t->heightmap[idx + WS_TERRAIN_W];
                int hw = t->heightmap[idx - 1];
                int he = t->heightmap[idx + 1];
                
                int max_diff = 0;
                int max_neighbor = idx;
                
                int d;
                d = h - hn; if (d > max_diff) { max_diff = d; max_neighbor = idx - WS_TERRAIN_W; }
                d = h - hs; if (d > max_diff) { max_diff = d; max_neighbor = idx + WS_TERRAIN_W; }
                d = h - hw; if (d > max_diff) { max_diff = d; max_neighbor = idx - 1; }
                d = h - he; if (d > max_diff) { max_diff = d; max_neighbor = idx + 1; }
                
                if (max_diff > 2) { /* talus angle threshold */
                    int transfer = max_diff / 4;
                    t->heightmap[idx] -= transfer;
                    t->heightmap[max_neighbor] += transfer;
                }
            }
        }
    }
}

uint8_t ws_terrain_height(const ws_terrain_t *t, int x, int y) {
    if (x < 0) x = 0; if (x >= t->width) x = t->width - 1;
    if (y < 0) y = 0; if (y >= t->height) y = t->height - 1;
    return t->heightmap[y * t->width + x];
}

uint8_t ws_terrain_biome(const ws_terrain_t *t, int x, int y) {
    if (x < 0) x = 0; if (x >= t->width) x = t->width - 1;
    if (y < 0) y = 0; if (y >= t->height) y = t->height - 1;
    return t->biome[y * t->width + x];
}
