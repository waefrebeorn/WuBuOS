/*
 * standalone_hosted_shim.c -- standalone app shim for hosted-only symbols.
 *
 * The WuBuOS GUI references two symbols that are normally defined inside the
 * hosted binary (src/hosted/hosted.o, which carries main() and is therefore
 * not linkable into a standalone app binary like paint/doom):
 *
 *   - dosgui_wm_get_hosted_state()  (Play action in dosgui_wm_ctxmenu.c)
 *   - g_primary_selection_manager   (wayland primary-selection in wubu_clipboard.c)
 *
 * The hosted binary provides real implementations. Standalone apps have no
 * hosted runtime, so we supply the same NULL-returning shim that the unit-test
 * harness (dosgui_wm_test_stub.c) already uses. This keeps the from-scratch
 * GUI apps buildable without dragging hosted.o (and its main()) into the link.
 *
 * This is NOT a stub of real work: the symbols genuinely have no meaning
 * outside the hosted compositor, and returning NULL is the correct, defined
 * behaviour (ctx_action_play treats NULL as "no launch"; clipboard treats a
 * NULL manager as "primary selection unavailable").
 */

#include "dosgui_wm.h"
#include "../hosted/hosted.h"   /* hosted_state_t */

/* Mirrors hosted.c: g_primary_selection_manager is a wayland protocol global
 * that only exists once the hosted compositor connects to the display server.
 * Standalone apps never connect, so it is NULL. */
struct zwp_primary_selection_device_manager_v1 *g_primary_selection_manager = NULL;

/* Returns NULL when there is no hosted runtime (e.g. a standalone paint/doom
 * binary). Callers must already handle NULL. */
hosted_state_t *dosgui_wm_get_hosted_state(void) {
    return NULL;
}
