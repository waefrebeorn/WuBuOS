/*
 * wubu_ns_kv.h -- the /n/kv control subtree (the AGI's KV cache namespace).
 *
 *   /n/kv/snapshot  -> JSON view of the KV mount table (read)
 *   /n/kv/world/    -> the training experience stream, one file per tick
 *
 * The tick files are placeholders: the Styx server routes reads of any
 * /n/kv/... path to the LIVE tensor (styxfs_server.c calls
 * wubu_kvfs_route_path/read), so a file only has to exist for a 9P client to
 * open it -- the bytes come from the KV cache, not from disk.
 *
 * C11, minimal includes.
 */
#ifndef WUBU_NS_KV_H
#define WUBU_NS_KV_H

/* Publish the subtree skeleton (/n/kv and /n/kv/world). Called at boot. */
int wubu_ns_publish_kv(void);

/* Bring up the kernel KV-FS with the canonical mount layout
 * (/kv/in, /kv/world, /kv/agent). Returns 0 on success. */
int wubu_kvfs_namespace_init(void);

/* Refresh /n/kv/snapshot from the live mount table. */
int wubu_ns_kv_refresh(void);

/* Create the entry for one experience tick. Called by the play loop right
 * after it writes /kv/world/tick_<N> into the tensor. No-op (-1) when the
 * KV-FS is not initialised. */
int wubu_ns_kv_touch(int tick);

#endif /* WUBU_NS_KV_H */
