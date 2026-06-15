/*
 * bear_arena.c  --  PufferC/BearRL Arena Allocator Implementation
 */

#include "bear_arena.h"

/* ===================================================================
 * Global Arena Instances
 * =================================================================== */

BearArena g_bear_global_arena;
BearArena g_bear_rollout_arena;

/* ===================================================================
 * Arena Initialization / Cleanup
 * =================================================================== */

int bear_arena_global_init(size_t global_cap, size_t rollout_cap) {
    if (bear_arena_create(&g_bear_global_arena, global_cap) != 0) return -1;
    if (bear_arena_create(&g_bear_rollout_arena, rollout_cap) != 0) {
        bear_arena_destroy(&g_bear_global_arena);
        return -1;
    }
    return 0;
}

void bear_arena_global_shutdown(void) {
    bear_arena_destroy(&g_bear_global_arena);
    bear_arena_destroy(&g_bear_rollout_arena);
}

/* Reset rollout arena for new epoch */
void bear_rollout_reset(void) {
    bear_arena_reset(&g_bear_rollout_arena);
}

/* ===================================================================
 * Tensor Debug Helpers
 * =================================================================== */

#include <stdio.h>

void bear_tensor_print(const BearTensor* t, int max_elems) {
    if (!t || !t->data) {
        printf("Tensor (null)\n");
        return;
    }
    
    printf("Tensor '%s' [%s] shape=[", t->name,
           t->dtype == BEAR_DTYPE_F32 ? "f32" :
           t->dtype == BEAR_DTYPE_I32 ? "i32" :
           t->dtype == BEAR_DTYPE_U8 ? "u8" : "i64");
    for (int i = 0; i < t->ndim; ++i) {
        printf("%ld%s", t->shape[i], (i == t->ndim - 1) ? "]" : ", ");
    }
    printf(" strides=[");
    for (int i = 0; i < t->ndim; ++i) {
        printf("%ld%s", t->stride[i], (i == t->ndim - 1) ? "]" : ", ");
    }
    printf(" numel=%ld\n", bear_tensor_numel(t));
    
    if (t->dtype != BEAR_DTYPE_F32 || max_elems <= 0) return;
    
    int64_t n = bear_tensor_numel(t);
    int limit = max_elems < 0 ? n : (n < max_elems ? n : max_elems);
    float* p = (float*)t->data;
    for (int64_t i = 0; i < limit; ++i) {
        printf("%.4f ", p[i]);
        if ((i + 1) % 8 == 0) printf("\n");
    }
    if (limit < n) printf("... (%ld more)", n - limit);
    printf("\n");
}

void bear_arena_stats(const BearArena* a, const char* label) {
    printf("Arena %s: used=%.2f MiB / cap=%.2f MiB (%.1f%%), peak=%.2f MiB\n",
           label ? label : "",
           a->used / (1024.0 * 1024.0),
           a->cap / (1024.0 * 1024.0),
           100.0 * a->used / a->cap,
           a->peak_used / (1024.0 * 1024.0));
}