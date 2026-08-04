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
#include <sys/stat.h>
#include "wubu_runtime.h"

static int failures = 0;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } else { printf("  ok: %s\n", m); } } while (0)

static int count_cb(const wubu_rt_space_t *sp, void *user)
{
    (void)sp;
    (*(size_t *)user)++;
    return 0;
}

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

    /* --- Wave 3 (the gap filler): personalities --- */
    {
        printf("[wave 3] personalities (the gap filler)\n");
        uint64_t jvm = wubu_runtime_create(rt, "java-jvm-21", "java",
                                           "holyc-0.1.0", "java-21",
                                           "wubu-abi-v1", "/n/java/");
        CHECK(jvm != 0, "re-create java-jvm-21 (after ring recycle)");

        /* W1: attach a personality */
        CHECK(wubu_runtime_set_personality(rt, jvm, "posix") == 0,
              "attach posix personality");
        wubu_rt_space_t *sp = wubu_runtime_find(rt, jvm);
        CHECK(sp && sp->personality &&
              !strcmp(sp->personality->name, "posix"),
              "personality attached");
        CHECK(sp && sp->state == WUBU_RT_WARM,
              "attaching a personality warms the space");

        /* unknown personality refused */
        CHECK(wubu_runtime_set_personality(rt, jvm, "lisp") == -1,
              "unknown personality refused");

        /* W2: dispatch syscalls through the personality */
        void *mem = (void *)(uintptr_t)wubu_runtime_call(
            rt, jvm, WUBU_RT_SYS_HEAP_ALLOC, 64, 0, 0);
        CHECK(mem != NULL, "heap alloc via posix personality");
        wubu_runtime_call(rt, jvm, WUBU_RT_SYS_HEAP_FREE,
                          (int64_t)(uintptr_t)mem, 0, 0);

        /* write to a real fd (stdout) via the personality — the JVM's
         * "syscall" maps to the OS-native substrate, not a re-implemented
         * one */
        int64_t w = wubu_runtime_call(rt, jvm, WUBU_RT_SYS_WRITE,
                                      1, (int64_t)(uintptr_t)"JVM-says-hi\n",
                                      13);
        CHECK(w == 13, "write via posix personality (native)");

        /* WASI sandbox: open outside /n/ refused */
        uint64_t wasm = wubu_runtime_create(rt, "wasm-instance-2", "wasm",
                                            "holyc-0.1.0", "wasi-p2",
                                            "wubu-abi-v1", "/n/wasm2/");
        CHECK(wasm != 0, "create wasm space");
        CHECK(wubu_runtime_set_personality(rt, wasm, "wasi") == 0,
              "attach wasi personality");
        int64_t fd = wubu_runtime_call(rt, wasm, WUBU_RT_SYS_OPEN,
                                       (int64_t)(uintptr_t)"/etc/passwd",
                                       0, 0);
        CHECK(fd == WUBU_RT_SANDBOX_REFUSED,
              "wasi sandbox REFUSES /etc/passwd (outside /n/, by policy)");
        /* inside the namespace root is admitted by policy (the actual
         * resolution is the OS's business — in real WuBuOS /n/ is the
         * styx namespace, not host root) */
        fd = wubu_runtime_call(rt, wasm, WUBU_RT_SYS_OPEN,
                               (int64_t)(uintptr_t)"/n/../tmp/wubu_rt_tape",
                               4 /* O_CREAT */, 0);
        CHECK(fd != WUBU_RT_SANDBOX_REFUSED, "wasi ADMITS /n/ paths");
        if (fd >= 0) wubu_runtime_call(rt, wasm, WUBU_RT_SYS_CLOSE, fd, 0, 0);
        remove("/tmp/wubu_rt_tape");

        /* no personality attached yet -> dispatch refused */
        uint64_t cold = wubu_runtime_create(rt, "cold-space", "c++",
                                            "holyc-0.1.0", "abi-v1",
                                            "wubu-abi-v1", "/n/cxx/");
        CHECK(wubu_runtime_call(rt, cold, WUBU_RT_SYS_READ, 0, 0, 0) == -1,
              "dispatch refused on a personality-less (cold) space");

        /* W3: enumeration includes personalities */
        size_t count = 0;
        wubu_runtime_list(rt, count_cb, &count);
        CHECK(count == wubu_runtime_count(rt),
              "list enumerates every live space");
    }

    wubu_runtime_free(rt);
    wubu_hive_destroy(hive);

    printf("\n%s (%d failures)\n",
           failures == 0 ? "=== test_runtime PASSED ===" : "=== test_runtime FAILED ===",
           failures);
    return failures == 0 ? 0 : 1;
}
