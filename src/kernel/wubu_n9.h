/*
 * wubu_n9.h  --  A3: booted-image 9P proof (docs/GAP_ROADMAP.md, GAP A3).
 *
 * The on-metal completion of DA#3: the BOOTED WuBuOS AGI kernel opens a 9P
 * session to a HOST-exported /n server (carried over a dedicated COM2 UART),
 * walks /apps/<mod>, reads the module bytes, runs them through the loader
 * (wubu_zx_run, mapping into a high-half exec VA), and reports the entry's
 * result back on the COM1 klog. That printed `RESULT: PASS -- <result>` is the
 * hardware-gated proof none of the hosted tests can produce.
 *
 * Freestanding, C11, no malloc beyond the caller-supplied buffer.
 */
#ifndef WUBU_N9_H
#define WUBU_N9_H

#include <stdint.h>

/* Run the whole A3 sequence once at boot. Returns 0 on success (module ran),
 * nonzero on a transport/load failure (logged on COM1). Safe to call exactly
 * once; never blocks the COM1 debug channel. */
int wubu_n9_boot(void);

#endif /* WUBU_N9_H */
