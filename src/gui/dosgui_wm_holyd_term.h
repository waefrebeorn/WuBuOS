/*
 * dosgui_wm_holyd_term.h -- HolyD Terminal subsystem (public API).
 *
 * Self-contained: declares only what other GUI modules need to spawn/use the
 * terminal. The terminal evaluates HolyD through an injected evaluator
 * (dependency injection) so this GUI module never reaches into the runtime
 * AGI/EDR layer or into the compiler internals directly -- no god headers.
 *
 * Default evaluator: a direct, in-process JIT compile+run via the public
 * HolyD compiler API (hd_eval). At the composition root the hosted binary
 * injects the richer wubu_holyd_agi path (holyd daemon + EDR disclosure),
 * but the terminal module itself stays decoupled from both.
 */

#ifndef DOSGUI_WM_HOLYC_TERM_H
#define DOSGUI_WM_HOLYC_TERM_H

#include <stddef.h>

struct DosGuiWindow;

/*
 * HolyD evaluator signature.
 *   src       -- HolyD source to compile + run
 *   out       -- result text buffer (e.g. "6" on success, message on error)
 *   out_size  -- capacity of `out`
 *   returns   0 on success, non-zero on lex/parse/codegen/JIT error.
 */
typedef int (*holyd_term_eval_fn)(const char *src, char *out, size_t out_size);

/* Inject the evaluator used by the terminal. Pass NULL to restore the
 * built-in default (direct JIT). Call before the first eval. */
void holyd_term_set_eval(holyd_term_eval_fn fn);

/* Spawn a HolyD Terminal window at (x,y,w,h). Returns the window or NULL. */
struct DosGuiWindow *dosgui_wm_spawn_holyd_term(int x, int y, int w, int h);

#endif /* DOSGUI_WM_HOLYC_TERM_H */