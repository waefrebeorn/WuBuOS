/*
 * wubu_holyc_agi.h -- Live ring-0 compiler AGI layer (TempleOS "God compiler")
 *
 * WuBuOS already has a working HolyC -> x86-64 JIT compiler (src/compiler) and
 * a persistent-repl daemon (wubu_holyd). What was missing was the CONNECTIVE
 * TISSUE: a human at the HolyC Terminal, and an AGI, both need to author HolyC
 * source and have it compile + run LIVE in the same process, with the result
 * shown and the action disclosed to EDR.
 *
 * This module is that tissue. It owns one in-process holyd daemon with a
 * persistent "default" session (so functions/variables accumulate across evals
 * -- true TempleOS REPL semantics) and exposes two entry points:
 *
 *   wubu_holyc_eval(src, out, n)        -- compile + run, return result text.
 *   wubu_holyc_agent_eval(src, out, n)  -- same, but logged to EDR as an AGI
 *                                          action (the transparency edict).
 *
 * There is no sandbox and no separate build step: source in, native code out,
 * executed where it stands -- that is the ring-0 God-compiler magic, and the
 * AGI uses the exact same path a human does.
 */

#ifndef WUBU_HOLYC_AGI_H
#define WUBU_HOLYC_AGI_H

#include <stddef.h>

/* Lazily initialize the in-process holyd daemon + default session.
 * Idempotent; safe to call before the first eval. Returns 0 on success. */
int  wubu_holyc_agi_init(void);

/* Compile + execute HolyC source LIVE. On success `out` holds the integer
 * result (e.g. "6"); on lex/parse/codegen/JIT error `out` holds the message.
 * Returns 0 on success, non-zero on failure. */
int  wubu_holyc_eval(const char *src, char *out, size_t out_size);

/* AGI variant: same as wubu_holyc_eval, but records the action to EDR
 * (EDR_EV_AGENT_ACTION, detail "holyc: <src>") so a watching human can see
 * exactly what the operating system compiled and ran on the agent's behalf. */
int  wubu_holyc_agent_eval(const char *src, char *out, size_t out_size);

/* Tear down the daemon (used at shutdown). */
void wubu_holyc_agi_shutdown(void);

#endif /* WUBU_HOLYC_AGI_H */
