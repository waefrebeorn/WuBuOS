#ifndef WUBU_STYX_INTERNAL_H
#define WUBU_STYX_INTERNAL_H

/* Internal shared surface for the decomposed styx_* modules.
 * No public API, no god header: every module includes only styx.h
 * (for the wire types + inline pack/unpack helpers) and this tiny
 * header for the one helper they all need. */

#include "styx.h"

/* Pack the 4-byte size + 1-byte type + 2-byte tag header that every
 * 9P2000/Styx message begins with. Shared by the request encoders
 * (styx_enc.c), the response builders (styx_serve.c) and the error
 * builder. Inlined so each module stays self-contained (no extra .o
 * needed for a one-liner). */
static inline void styx_write_header(uint8_t *buf, uint32_t total_size,
                                     uint8_t type, uint16_t tag) {
    styx_put32(buf, total_size);
    buf[4] = type;
    styx_put16(buf + 5, tag);
}

#endif /* WUBU_STYX_INTERNAL_H */
