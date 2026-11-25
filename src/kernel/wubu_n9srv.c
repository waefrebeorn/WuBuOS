/*
 * wubu_n9srv.c  --  serve /n/kv over the COM2 9P link.
 *
 * One concern: glue the framed transport (wubu_n9) to the server codec
 * (wubu_9psrv). No framing code here, no 9P code here -- if either side needs
 * a change it changes in its owner.
 *
 * Bounded by construction: the caller passes how many frames to answer, every
 * receive is a bounded poll, and a quiet host simply ends the loop. A boot can
 * never hang waiting for a client that never comes.
 */
#include "wubu_n9srv.h"
#include "wubu_n9.h"        /* framed COM2 transport (u32 LE length + 9P) */
#include "wubu_9psrv.h"
#include "wubu_9pwire.h"
#include "klog.h"

#include <string.h>

static uint8_t s_in[WUBU_9PSRV_MSIZE];
static uint8_t s_out[WUBU_9PSRV_MSIZE];

uint32_t wubu_n9srv_serve(uint32_t max_frames)
{
    uint32_t served = 0;
    for (uint32_t i = 0; i < max_frames; i++) {
        uint32_t len = 0;
        if (wubu_n9_recv_frame(s_in, sizeof s_in, &len) != 0) break;
        if (len < W9_HDR + 1) break;              /* truncated frame: stop */

        uint32_t rl = 0;
        if (wubu_9psrv_serve(s_in, len, s_out, sizeof s_out, &rl) != 0) {
            /* malformed beyond answering: drop the frame, keep serving */
            klog_printf("n9srv: malformed frame (%u bytes) dropped\n",
                        (unsigned)len);
            continue;
        }
        if (wubu_n9_send_frame(s_out, rl) != 0) break;
        served++;
    }
    return served;
}
