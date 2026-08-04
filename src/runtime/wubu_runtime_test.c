/*
 * test_runtime.c -- WUBURUNTIME test (research/063, Wave 1).
 *
 * The DA oracles:
 *   1. create/read round-trip: a space's snapshot is intact (the
 *      compiler_ver / language_ver / created fields survive)
 *   2. coexistence: jvm-21 and clr-9 are separate spaces, no overlap
 *   3. the snapshot guarantee: compiler_ver + language_ver + created
 *      are recorded (the "not left in the dust" promise)
 *   4. ring-bounded: the registry caps at max_spaces, recycles the
 *      oldest
 *   5. namespace: each space maps to its own 9P path
 *   6. the state machine: cold -> warm -> live -> frozen
 *   7. heap accounting: touch_heap respects the cap (refuses over)
 *   8. destroy: a space is removed, count drops
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "wubu_runtime.h"

static int failures = 0;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } else { printf("  ok: %s\n", m); } } while (0)

int main(void)
{
    printf("=== test_runtime (wuburuntime, Wave 1) ===\n");

    wubu_hive_t *hive = wubu_hive_new(0, malloc, free);
    CHECK(hive != NULL, "hive init");
    wubu_rt_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.max_spaces = 4;
    cfg.default_heap_cap = 1ull << 20;
    wubu_runtime_t *rt = wubu_runtime_init(hive, &cfg);
    CHECK(rt != NULL, "init");

    /* --- oracle 3: the snapshot guarantee --- */
    {
        printf("[oracle 3] the snapshot (nothing left in the dust)\n");
        uint64_t jvm = wubu_runtime_create(rt, "java-jvm-21", "java",
                                           "holyc-0.1.0", "java-21",
                                           "wubu-abi-v1", "/n/java/");
        CHECK(jvm != 0, "create java-jvm-21 space");
        wubu_rt_space_t *sp = wubu_runtime_find(rt, jvm);
        CHECK(sp != NULL, "find by id");
        if (sp) {
            CHECK(!strcmp(sp->compiler_ver, "holyc-0.1.0"),
                  "compiler_ver recorded (holyc-0.1.0)");
            CHECK(!strcmp(sp->language_ver, "java-21"),
                  "language_ver recorded (java-21)");
            CHECK(strlen(sp->created) == 10 && sp->created[4] == '-',
                  "created date recorded (YYYY-MM-DD)");
            CHECK(!strcmp(sp->abi_snapshot, "wubu-abi-v1"),
                  "abi snapshot recorded");
            CHECK(!strcmp(sp->language, "java"), "language recorded");
        }
    }

    /* --- oracle 2 + 5: coexistence + namespaces --- */
    {
        printf("[oracle 2+5] two runtimes coexist, own namespaces\n");
        uint64_t clr = wubu_runtime_create(rt, "dotnet-clr-9", "csharp",
                                           "holyc-0.1.0", "clr-9",
                                           "wubu-abi-v1", "/n/dotnet/");
        CHECK(clr != 0, "create dotnet-clr-9 space");
        wubu_rt_space_t *a = wubu_runtime_find_name(rt, "java-jvm-21");
        wubu_rt_space_t *b = wubu_runtime_find_name(rt, "dotnet-clr-9");
        CHECK(a && b, "both found by name");
        if (a && b) {
            CHECK(a->id != b->id, "distinct ids (no overlap)");
            CHECK(!strcmp(a->namespace_path, "/n/java/") &&
                  !strcmp(b->namespace_path, "/n/dotnet/"),
                  "each space owns its 9P namespace");
        }
        CHECK(wubu_runtime_count(rt) == 2, "count = 2");
    }

    /* --- oracle 6: the state machine --- */
    {
        printf("[oracle 6] cold -> warm -> live -> frozen\n");
        wubu_rt_space_t *sp = wubu_runtime_find_name(rt, "java-jvm-21");
        CHECK(sp && sp->state == WUBU_RT_COLD, "created cold");
        uint64_t id = sp ? sp->id : 0;
        CHECK(wubu_runtime_set_state(rt, id, WUBU_RT_WARM) == 0, "-> warm");
        CHECK(wubu_runtime_set_state(rt, id, WUBU_RT_LIVE) == 0, "-> live");
        CHECK(wubu_runtime_set_state(rt, id, WUBU_RT_FROZEN) == 0, "-> frozen");
        CHECK(wubu_runtime_set_state(rt, 999999, WUBU_RT_LIVE) == -1,
              "unknown id refused");
    }

    /* --- oracle 7: heap accounting (ring-bounded) --- */
    {
        printf("[oracle 7] heap accounting respects the cap\n");
        wubu_rt_space_t *sp = wubu_runtime_find_name(rt, "java-jvm-21");
        uint64_t id = sp ? sp->id : 0;
        CHECK(wubu_runtime_touch_heap(rt, id, 1000) == 0, "touch +1000");
        CHECK(sp && sp->heap_used == 1000, "heap_used tracked");
        CHECK(wubu_runtime_touch_heap(rt, id, (int64_t)(1ull << 20)) == -1,
              "over the cap refused");
        CHECK(wubu_runtime_touch_heap(rt, id, -500) == 0, "release -500");
        CHECK(sp && sp->heap_used == 500, "heap_used released");
    }

    /* --- oracle 4: ring-bounded registry --- */
    {
        printf("[oracle 4] registry caps and recycles the oldest\n");
        uint64_t v8 = wubu_runtime_create(rt, "js-v8", "javascript",
                                          "holyc-0.1.0", "v8",
                                          "wubu-abi-v1", "/n/js/");
        uint64_t wasm = wubu_runtime_create(rt, "wasm-instance-1", "wasm",
                                            "holyc-0.1.0", "wasi-p2",
                                            "wubu-abi-v1", "/n/wasm/");
        CHECK(v8 && wasm, "created two more (registry now at cap 4)");
        CHECK(wubu_runtime_count(rt) == 4, "count capped at 4");
        /* one more: the oldest (java-jvm-21, seq 1) is recycled */
        uint64_t rust = wubu_runtime_create(rt, "rust-tokio", "rust",
                                            "holyc-0.1.0", "rust-2024",
                                            "wubu-abi-v1", "/n/rust/");
        CHECK(rust != 0, "created the 5th (recycles the oldest)");
        CHECK(wubu_runtime_count(rt) == 4, "count stays at 4 (ring)");
        CHECK(wubu_runtime_find_name(rt, "java-jvm-21") == NULL,
              "the oldest (java-jvm-21) was recycled");
        CHECK(wubu_runtime_find_name(rt, "rust-tokio") != NULL,
              "the newest (rust-tokio) is present");
    }

    /* --- oracle 1 + 8: round-trip + destroy --- */
    {
        printf("[oracle 1+8] round-trip + destroy\n");
        wubu_rt_space_t *sp = wubu_runtime_find_name(rt, "rust-tokio");
        uint64_t id = sp ? sp->id : 0;
        CHECK(wubu_runtime_destroy(rt, id) == 0, "destroy rust-tokio");
        CHECK(wubu_runtime_find(rt, id) == NULL, "gone after destroy");
        CHECK(wubu_runtime_count(rt) == 3, "count = 3 after destroy");
        CHECK(wubu_runtime_destroy(rt, 999999) == -1, "destroy unknown fails");
    }

    wubu_runtime_free(rt);
    wubu_hive_destroy(hive);

    printf("\n%s (%d failures)\n",
           failures == 0 ? "=== test_runtime PASSED ===" : "=== test_runtime FAILED ===",
           failures);
    return failures == 0 ? 0 : 1;
}
