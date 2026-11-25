/*
 * wubu_n9srv.h  --  the COM2 binding of the kernel's /n/kv 9P server.
 *
 * wubu_n9 owns the on-wire framing (u32 LE length + 9P message over COM2) and
 * uses it as a CLIENT (fetch /apps). wubu_9psrv is the SERVER codec. This leaf
 * is the glue: poll a request frame, hand it to the codec, send the reply --
 * so a host process can mount /n/kv off the running metal body.
 *
 * Deliberately not a daemon: the caller decides how many frames to answer
 * (`n9serve` on the console), because a boot must never block on a host that
 * is not there.
 */
#ifndef WUBU_N9SRV_H
#define WUBU_N9SRV_H

#include <stdint.h>

/* Answer up to `max_frames` requests from COM2 against the live KV
 * namespace. Returns the number of frames served; 0 means the host was quiet
 * (or the link failed), which is a normal, bounded outcome. */
uint32_t wubu_n9srv_serve(uint32_t max_frames);

#endif /* WUBU_N9SRV_H */
