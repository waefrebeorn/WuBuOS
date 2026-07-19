/*
 * wubu_hc_eval_stub.c -- Minimal definition of hc_eval for runtime test
 * binaries that link wubu_exec.c (which calls hc_eval for HolyC JIT exec)
 * but do not build the real HolyC compiler. Returns -1 (no-op), matching the
 * "runtime without HolyC" contract. Real HolyC paths use the compiler module.
 */

#include <stdint.h>

int64_t hc_eval(const char *source) {
    (void)source;
    return -1;
}
