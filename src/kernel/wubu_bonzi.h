/*
 * wubu_bonzi.h -- Bonzi Buddy: bare-metal AGI agent persona (ring-0 task).
 *
 * The same persona as the hosted WuBuFX Bonzi window (src/apps/bonzi/), but
 * freestanding and running ON METAL as a kernel task inside the AGI
 * supervisor. Bonzi is the human interface of the bare-metal AGI:
 *
 *   - draws the gorilla + chat console on the framebuffer (vbe primitives),
 *   - polls the PS/2 keyboard (input_key_poll) for a command line,
 *   - dispatches REAL ring-0 actions: attestation dump, freeze/unfreeze of
 *     the self-improve loop, promote (run a DA-3 cycle), trace inspection,
 *   - emits an AGENT trace span for every human interaction, so the
 *     supervisor's self-improve loop scores the human loop itself,
 *   - heartbeats every ~1s (proof the loop is alive on the serial console).
 *
 * No speech-bubble theater: every command performs a genuine kernel call.
 * C11, opaque struct, freestanding (no malloc / pthreads).
 */
#ifndef WUBU_BONZI_H
#define WUBU_BONZI_H

struct wubu_agi_kernel;
typedef struct wubu_bonzi wubu_bonzi_t;

/* Kernel task entry (spawned by wubu_agi_kernel_run). arg = the AGI kernel. */
void wubu_bonzi_task(void *arg);

/* Init: draw the gorilla + console, greet. Returns the static instance. */
wubu_bonzi_t *wubu_bonzi_init(struct wubu_agi_kernel *k);

/* Cooperative tick: poll keys, dispatch lines, paced heartbeat + redraw.
 * Returns actions performed this tick. */
int wubu_bonzi_tick(wubu_bonzi_t *b);

/* Handle one human line through the real ring-0 dispatch. Returns actions. */
int wubu_bonzi_handle_line(wubu_bonzi_t *b, const char *line);

/* Last reply text (static buffer, valid until the next dispatch). */
const char *wubu_bonzi_last_reply(const wubu_bonzi_t *b);
int         wubu_bonzi_action_count(const wubu_bonzi_t *b);

#endif /* WUBU_BONZI_H */
