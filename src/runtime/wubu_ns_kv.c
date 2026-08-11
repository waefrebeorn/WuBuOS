/*
 * wubu_ns_kv.c -- the /n/kv control subtree (the AGI's KV cache namespace).
 *
 *   /n/kv/snapshot  -> JSON view of the KV mount table (read)
 *   /n/kv/world     -> the training experience stream (readable by the Brain)
 *
 * The KV-FS is the kernel-owned address space (wubu_kvfs.c). The Brain
 * (wubuwizard) mounts /n/kv/ over 9P and reads the world-state deltas
 * that wubu_agi_play_learn() writes every tick. This is THE AGI LOOP:
 *
 *   1. world drivers → wubu_world_sample()
 *   2. policy → action → input events → game
 *   3. wubu_agi_play_learn() → /kv/world/tick_<N> (4 floats)
 *   4. Brain reads /n/kv/world/tick_* → training experience
 *
 * C11, minimal includes — no god headers.
 */
#include "wubu_ns_bridge_internal.h"
#include "../kernel/wubu_kvfs.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Publish the /n/kv subtree skeleton. Called once at namespace init. */
int wubu_ns_publish_kv(void)
{
    if (ns_mkdir("kv") != 0) return -1;

    /* /n/kv/snapshot — the KV mount table as JSON (read-only metadata) */
    char sub[128];
    if (!g_wubu_kvfs) {
        snprintf(sub, sizeof(sub), "kv/snapshot");
        ns_write(sub, "{\"status\":\"kvfs not initialized\"}\n");
        return 0;
    }

    size_t jlen = 0;
    char *json = wubu_kvfs_snapshot_json(g_wubu_kvfs, &jlen);
    snprintf(sub, sizeof(sub), "kv/snapshot");
    if (json) {
        ns_write(sub, json);
        free(json);
    } else {
        ns_write(sub, "{\"status\":\"kvfs snapshot failed\"}\n");
    }

    /* /n/kv/world — directory marker (the tick files are written
     * dynamically by wubu_agi_play_learn into the KV tensor). */
    snprintf(sub, sizeof(sub), "kv/world");
    ns_write(sub, "kv namespace: world-state experience stream\n");

    return 0;
}

/* Refresh /n/kv/snapshot from the live KV mount table. */
int wubu_ns_kv_refresh(void)
{
    if (!g_wubu_kvfs) return -1;
    char sub[128];
    size_t jlen = 0;
    char *json = wubu_kvfs_snapshot_json(g_wubu_kvfs, &jlen);
    if (!json) return -1;
    snprintf(sub, sizeof(sub), "kv/snapshot");
    int rc = ns_write(sub, json);
    free(json);
    return rc;
}

/* Initialize the kernel KV-FS with the canonical mount layout.
 *   /kv/in     — input experience (downloads, sensor data, corpus)
 *   /kv/world  — the AGI play-loop training stream
 *   /kv/agent  — the model's own activations (read-back)
 * Returns 0 on success, -1 if already initialized or oom. */
int wubu_kvfs_namespace_init(void)
{
    if (g_wubu_kvfs) return -1; /* already live */

    /* 4096 blocks × 256 floats = 4M floats = 16 MB KV tensor.
     * Plenty for the training stream on this host. */
    int rc = wubu_kvfs_kernel_init(256, 4096);
    if (rc != 0) return -1;

    /* Mount the canonical training regions. Each gets 1M floats (4 MB). */
    rc  = wubu_kvfs_mount(g_wubu_kvfs, "/kv/in",     0,   1024);
    rc |= wubu_kvfs_mount(g_wubu_kvfs, "/kv/world", 1024, 1024);
    rc |= wubu_kvfs_mount(g_wubu_kvfs, "/kv/agent", 2048, 2048);
    if (rc != 0) return -1;

    return 0;
}
