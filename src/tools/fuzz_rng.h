/*
 * fuzz_rng.h -- the PRNG shared by the deterministic fuzz targets.
 *
 * WHY A HEADER
 *   Two fuzzers were about to carry two copies of splitmix64.  A shared seed -> same
 *   sequence property only holds if they share ONE implementation; copies drift as
 *   soon as somebody "improves" one of them.  This is deliberately header-only and
 *   dependency-free (stdint only) so any test target can take it without pulling in
 *   the world, and so each TU gets its own private state.
 *
 * WHY NOT rand()
 *   rand() has an unspecified algorithm and global state: two runs of the same seed
 *   can differ by libc, and a mutation elsewhere in the process shifts the sequence.
 *   A finding you cannot reproduce is not a finding.
 */
#ifndef WUBU_FUZZ_RNG_H
#define WUBU_FUZZ_RNG_H

#include <stdint.h>

typedef struct {
    uint64_t state;
} fuzz_rng;

static inline void fuzz_rng_seed(fuzz_rng *r, uint64_t seed)
{
    r->state = seed;
}

/* splitmix64: good enough distribution for picking byte offsets, cheap, stable. */
static inline uint64_t fuzz_rng_next(fuzz_rng *r)
{
    uint64_t z = (r->state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

static inline uint32_t fuzz_rng_below(fuzz_rng *r, uint32_t n)
{
    return n ? (uint32_t)(fuzz_rng_next(r) % n) : 0;
}

#endif /* WUBU_FUZZ_RNG_H */
