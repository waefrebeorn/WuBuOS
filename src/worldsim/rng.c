/*
 * rng.c -- PCG-XSH-RR 32-bit generator for the world simulation (WORLDSIM #564).
 *
 * The legacy ws_rng_* xorshift in terrain.c stays for existing callers; this is
 * the specified generator: a 64-bit LCG state with a 32-bit XSH-RR output, all
 * integer arithmetic, so the stream is identical on x86-64 and aarch64.
 *
 * Reference construction (O'Neill, PCG32):
 *   state = state * 6364136223846793005 + inc
 *   xorshifted = (uint32_t)(((old >> 18) ^ old) >> 27)
 *   rot = old >> 59
 *   out = (xorshifted >> rot) | (xorshifted << ((-rot) & 31))
 */
#include "worldsim.h"

#define WS_PCG_MULT 6364136223846793005ULL

static uint32_t pcg_output(uint64_t old)
{
    uint32_t xorshifted = (uint32_t)(((old >> 18u) ^ old) >> 27u);
    uint32_t rot = (uint32_t)(old >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31u));
}

void ws_pcg_seed(ws_pcg32_t *r, uint64_t seed, uint64_t seq)
{
    if (!r) return;
    r->state = 0u;
    r->inc = (seq << 1u) | 1u;
    (void)ws_pcg_next(r);
    r->state += seed;
    (void)ws_pcg_next(r);
}

uint32_t ws_pcg_next(ws_pcg32_t *r)
{
    if (!r) return 0u;
    uint64_t old = r->state;
    r->state = old * WS_PCG_MULT + r->inc;
    return pcg_output(old);
}

uint32_t ws_pcg_bounded(ws_pcg32_t *r, uint32_t bound)
{
    if (!r || bound == 0u) return 0u;
    /* Debiased rejection: drop the incomplete tail of the 2^32 range. */
    uint32_t threshold = (uint32_t)(-bound) % bound;
    for (;;) {
        uint32_t v = ws_pcg_next(r);
        if (v >= threshold) return v % bound;
    }
}

float ws_pcg_float(ws_pcg32_t *r)
{
    /* 24-bit mantissa, [0,1) */
    return (float)(ws_pcg_next(r) >> 8) * (1.0f / 16777216.0f);
}