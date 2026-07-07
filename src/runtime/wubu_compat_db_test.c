/*
 * wubu_compat_db_test.c -- WuBuOS per-app compat DB tests (SteamOS ProtonDB
 * + shader-cache lesson). Real on-disk round-trip assertions.
 */

#define _POSIX_C_SOURCE 200809L
#include "wubu_compat_db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int g_run = 0, g_pass = 0;
#define T(cond, msg) do { \
    g_run++; \
    if (cond) { g_pass++; printf("  ✅ %s\n", msg); } \
    else { printf("  ❌ %s (line %d)\n", msg, __LINE__); } \
} while (0)

int main(void) {
    /* Isolate under /tmp. */
    char store[256];
    snprintf(store, sizeof(store), "/tmp/wubu-compat-test-%d", (int)getpid());
    setenv("HOME", store, 1);

    printf("=== WuBuOS Compat DB Test Suite ===\n\n");

    T(wubu_compat_db_init() == 0, "db init (store dir created)");

    /* Normalize title: mixed case + punctuation -> lowercase key. */
    char key[WUBU_COMPAT_TITLE_MAX];
    wubu_compat_normalize_title("Half-Life 2 (Demo)", key, sizeof(key));
    T(strcmp(key, "half_life_2_demo") == 0, "title normalized");

    /* Set an entry (ProtonDB-style profile). */
    WubuCompatEntry e;
    memset(&e, 0, sizeof(e));
    strncpy(e.title, "Half-Life 2", sizeof(e.title) - 1);
    strncpy(e.proton_ver, "GE-Proton7-43", sizeof(e.proton_ver) - 1);
    strncpy(e.proton_flags, "dxvk_async,fsync", sizeof(e.proton_flags) - 1);
    strncpy(e.env_overrides, "WINEESYNC=1\nDXVK_ASYNC=1", sizeof(e.env_overrides) - 1);
    strncpy(e.dll_overrides, "d3d11=n;dxgi=n", sizeof(e.dll_overrides) - 1);
    e.rating = 5;
    e.cache_enabled = true;
    T(wubu_compat_db_set(&e) == 0, "set entry");

    /* Get it back -- verify round-trip. */
    WubuCompatEntry g;
    T(wubu_compat_db_get("Half-Life 2", &g) == 0, "get entry");
    T(strcmp(g.proton_ver, "GE-Proton7-43") == 0, "proton_ver round-trip");
    T(strcmp(g.proton_flags, "dxvk_async,fsync") == 0, "proton_flags round-trip");
    T(strcmp(g.dll_overrides, "d3d11=n;dxgi=n") == 0, "dll_overrides round-trip");
    T(g.rating == 5, "rating round-trip");
    T(g.cache_enabled == true, "cache_enabled round-trip");

    /* Missing entry -> -1. */
    WubuCompatEntry miss;
    T(wubu_compat_db_get("Nonexistent Game", &miss) == -1, "get missing -> -1");

    /* Per-title cache dir created. */
    char cpath[512];
    T(wubu_compat_cache_dir("Half-Life 2", cpath, sizeof(cpath)) == 0, "cache dir created");
    T(access(cpath, F_OK) == 0, "cache dir exists on disk");

    /* Delete. */
    T(wubu_compat_db_del("Half-Life 2") == 0, "delete entry");
    T(wubu_compat_db_get("Half-Life 2", &g) == -1, "get after delete -> -1");

    printf("\n=== Results: %d/%d passed ===\n", g_pass, g_run);
    return (g_pass == g_run) ? 0 : 1;
}
