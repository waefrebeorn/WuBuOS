/*
 * wubu_swarm_wire.c -- the fixed on-wire frame.  See the header for the layout.
 */
#include "wubu_swarm_wire.h"

#include <ctype.h>
#include <string.h>

static int is_hex(const char *s, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (!isxdigit((unsigned char)s[i])) return 0;
    return 1;
}

int wubu_swarm_wire_encode(const char *peer, const char *kind,
                           uint8_t out[WUBU_SWARM_WIRE_LEN])
{
    if (!out) return -1;
    if (!peer || strlen(peer) != WUBU_SWARM_WIRE_PEER) return -1;
    if (!is_hex(peer, WUBU_SWARM_WIRE_PEER)) return -1;
    size_t klen = kind ? strlen(kind) : 0;
    if (klen == 0 || klen > WUBU_SWARM_WIRE_KIND) return -1;

    memset(out, ' ', WUBU_SWARM_WIRE_LEN);       /* space-pad the body */
    memcpy(out, "WMSW", 4);                      /* magic */
    memcpy(out + 4, peer, WUBU_SWARM_WIRE_PEER); /* peer id */
    memcpy(out + 20, kind, klen);                /* kind, padded */
    return 0;
}

int wubu_swarm_wire_decode(const uint8_t in[WUBU_SWARM_WIRE_LEN],
                           char peer[WUBU_SWARM_WIRE_PEER + 1],
                           char kind[WUBU_SWARM_WIRE_KIND + 1])
{
    if (!in || !peer || !kind) return -1;
    if (memcmp(in, "WMSW", 4) != 0) return -1;        /* wrong magic */

    const char *ps = (const char *)(in + 4);
    if (!is_hex(ps, WUBU_SWARM_WIRE_PEER)) return -1; /* peer must be 16 hex */

    /* kind: 24 bytes, space-padded.  Trim trailing spaces; must not be empty. */
    int kend = WUBU_SWARM_WIRE_KIND - 1;
    const char *ks = (const char *)(in + 20);
    while (kend >= 0 && ks[kend] == ' ') kend--;
    if (kend < 0) return -1;                           /* no kind at all */

    memcpy(peer, ps, WUBU_SWARM_WIRE_PEER);
    peer[WUBU_SWARM_WIRE_PEER] = '\0';
    memcpy(kind, ks, (size_t)kend + 1);
    kind[kend + 1] = '\0';
    return 0;
}
