/*
 * wubu_bonzi_observe.c -- the companion's live-state observer (Track UX cleanup).
 *
 * ONE place that reads the live companion + WM state and pushes it to the
 * meta-layer consumers (the /n/bonzi namespace mirror + the ambient-context
 * module). The render loop used to inline a dozen weak no-ops and six
 * per-frame push calls directly; that smeared namespace/context knowledge
 * across the GUI renderer. Now the renderer calls this single function and
 * knows nothing about /n.
 *
 * Weak-linked: in test/capture builds that don't link the namespace +
 * context modules, this is a no-op. In the shipped build it drives both.
 */
#include "wubu_bonzi_observe.h"
#include "wubu_bonzi.h"
#include "wubu_bonzi_activity.h"
#include "wubu_bonzi_context.h"
#include "../runtime/wubu_ns_bonzi.h"
#include "../gui/dosgui_wm.h"

/* Push the live companion + focused-window state to the meta-layer
 * consumers. Safe to call every frame; no-op if the modules aren't linked. */
void wubu_bonzi_observe(void) {
    /* /n/bonzi namespace mirror */
    wubu_ns_bonzi_set_enabled(wubu_bonzi_is_enabled());
    wubu_ns_bonzi_set_pos(wubu_bonzi_x(), wubu_bonzi_y());
    wubu_ns_bonzi_set_size(wubu_bonzi_w(), wubu_bonzi_h());
    wubu_ns_bonzi_set_activity(wubu_bonzi_activity_name(wubu_bonzi_get_activity()));
    wubu_ns_bonzi_sync();
    /* ambient context (what the companion is attending to) */
    wubu_bonzi_context_set_window_count(dosgui_wm_window_count());
    DosGuiWindow *fw = dosgui_wm_get_focused();
    if (fw)
        wubu_bonzi_context_set_focus(fw->title, fw->x, fw->y, fw->w, fw->h);
}
