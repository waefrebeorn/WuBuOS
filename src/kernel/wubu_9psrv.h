/*
 * wubu_9psrv.h  --  KERNEL-CLEAN Styx/9P2000 SERVER for the /n/kv namespace.
 *
 * The twin of wubu_9pc (the kernel's 9P CLIENT): same wire vocabulary
 * (wubu_9pwire.h), opposite role. This is the metal half of "the KV cache IS
 * the OS's namespace": a 9P client walks /n/kv, opens a tick file, and reads
 * bytes served straight out of the LIVE ring-0 tensor (g_wubu_kv_base via
 * wubu_kvfs_route_read) -- never from disk.
 *
 * Transport-free on purpose: the caller hands over one request message and
 * takes one reply message, so COM2 framing (wubu_n9), a hosted test and a
 * future UDP wire all drive the same codec.
 *
 *   /            dir   children = first segment of every live mount (kv)
 *   /kv          dir   children = mount table entries under it
 *   /kv/world    dir   a mount point; its children are the tick files
 *   /kv/world/tick_3   FILE: block_size floats at the mount+hash offset
 *
 * Read-only by design: the Body owns the tensor, the Brain reads it. A write
 * is answered with a 9P error, not silently ignored.
 *
 * C11, opaque struct, minimal includes, no allocator.
 */
#ifndef WUBU_9PSRV_H
#define WUBU_9PSRV_H

#include <stdint.h>
#include <stddef.h>

/* Largest single reply frame this codec builds (Rread payload included). */
#define WUBU_9PSRV_MSIZE 8192u

/* Forget every fid (a fresh client / a reboot of the transport). */
void wubu_9psrv_reset(void);

/* Serve one request. `req` is a complete 9P2000 message WITHOUT the u32
 * length prefix (the transport owns framing). The reply is written into
 * `rsp` (at most `rsp_cap` bytes) and its length stored in *rsplen.
 * Returns 0 when a reply was produced (including a 9P Rerror), -1 when the
 * frame was too short/malformed to answer at all. */
int wubu_9psrv_serve(const uint8_t *req, uint32_t reqlen,
                     uint8_t *rsp, uint32_t rsp_cap, uint32_t *rsplen);

#endif /* WUBU_9PSRV_H */
