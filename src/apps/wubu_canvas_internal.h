#ifndef WUBU_CANVAS_INTERNAL_H
#define WUBU_CANVAS_INTERNAL_H

/* Internal shared surface for the decomposed wubu_canvas_* modules.
 * No public API, no god header: every module includes only wubu_canvas.h
 * (for WubuCanvas/WubuLayer/WubuPlugin types, wubu_blend, and all public
 * wubu_cv_* declarations) and this tiny header for the one private hook
 * they all need.
 *
 * The undo/redo subsystem owns the snapshot stacks as static globals inside
 * wubu_canvas_undo.c. Drawing and transform operations must record a snapshot
 * before mutating pixels, but must not reach into those globals directly, so
 * they call wubu_cv__undo_push() -- the single internal seam between the
 * mutation modules and the undo module. */

#include "wubu_canvas.h"

/* Record a pre-mutation snapshot of the active layer into the undo stack.
 * Defined in wubu_canvas_undo.c; called by wubu_canvas_draw.c and
 * wubu_canvas_transform.c before they touch layer pixels. Clears the redo
 * stack, exactly as the former monolith did. */
void wubu_cv__undo_push(WubuCanvas *cv);

#endif /* WUBU_CANVAS_INTERNAL_H */
