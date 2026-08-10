/*
 * wubu_ns_steamrt.c -- the /n/steamrt control subtree (the sniper
 * runtime + Proton launch integration).
 *
 *   /n/steamrt/manifest   -> the runtime lib count + the critical list
 *   /n/steamrt/env         -> write "appid game_lib proton_dist" and
 *                             the FULL Proton launch env is written back
 *   /n/steamrt/verify      -> write a comma-separated lib list; the
 *                             missing count is written back
 *
 * Each file wraps the REAL wubu_steamrt API via ns_mkdir/ns_write.
 */
#include "wubu_ns_bridge_internal.h"
#include "wubu_steamrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int wubu_ns_publish_steamrt(void)
{
    char sub[128];
    if (ns_mkdir("steamrt") != 0) return -1;

    snprintf(sub, sizeof(sub), "steamrt/manifest");
    if (ns_write(sub, "0 libs\n") != 0) return -1;
    snprintf(sub, sizeof(sub), "steamrt/env");
    if (ns_write(sub, "not built\n") != 0) return -1;
    snprintf(sub, sizeof(sub), "steamrt/verify");
    if (ns_write(sub, "0 missing\n") != 0) return -1;
    return 0;
}

/* refresh /n/steamrt/manifest */
int wubu_ns_steamrt_refresh_manifest(void)
{
    char sub[128], buf[128];
    snprintf(sub, sizeof(sub), "steamrt/manifest");
    snprintf(buf, sizeof(buf), "%d critical libs (sniper)\n",
             wubu_steamrt_get_manifest_count());
    return ns_write(sub, buf);
}

/* `echo "<appid> <game_lib> <proton_dist>" > /n/steamrt/env`
 * builds the Proton launch env and writes the STEAM_COMPAT_* summary
 * back to /n/steamrt/env. */
int wubu_ns_steamrt_build_env(const char *args)
{
    if (!args) return -1;
    unsigned appid = 0;
    char game_lib[256] = "", proton_dist[256] = "";
    if (sscanf(args, "%u %255s %255s", &appid, game_lib, proton_dist) < 2)
        return -1;
    wubu_steamrt_env_t env[8];
    int n = wubu_steamrt_build_env((uint32_t)appid, game_lib,
                                   proton_dist[0] ? proton_dist : NULL,
                                   env, 8);
    if (n < 0) return -1;
    /* write a readable summary back */
    char buf[2048];
    size_t off = 0;
    for (int i = 0; i < n && off < sizeof(buf) - 2; i++) {
        int w = snprintf(buf + off, sizeof(buf) - off, "%s=%s\n",
                         env[i].key, env[i].val);
        if (w < 0) break;
        off += (size_t)w;
    }
    char sub[128];
    snprintf(sub, sizeof(sub), "steamrt/env");
    return ns_write(sub, buf);
}

/* `echo "lib1,lib2,..." > /n/steamrt/verify` — the missing count. */
int wubu_ns_steamrt_verify(const char *list)
{
    if (!list) return -1;
    /* split on commas */
    const char *libs[64];
    size_t n = 0;
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", list);
    char *tok = strtok(tmp, ",");
    while (tok && n < 64) {
        libs[n++] = tok;
        tok = strtok(NULL, ",");
    }
    if (n == 0) return -1;
    int missing = wubu_steamrt_verify(libs, n, 0);
    char sub[128], buf[64];
    snprintf(sub, sizeof(sub), "steamrt/verify");
    snprintf(buf, sizeof(buf), "%d missing\n", missing);
    return ns_write(sub, buf);
}
