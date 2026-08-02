/*
 * bonzi.h -- Bonzi Buddy: desktop AGI agent persona (WuBuOS human interface).
 *
 * Bonzi is the friendly purple gorilla that sits on the desktop and actually
 * DOES things for the human. It is the human-facing front of the AGI operator:
 * the human types a request, Bonzi parses intent (launch app / run tool /
 * answer question) and routes it to the real OS plumbing (WuBuFX namespace
 * launch, dosgui app launch, or the wubuwizard substrate). No speech-bubble
 * theater — every action dispatches a genuine engine call.
 *
 * C11, opaque struct, minimal includes. The character is drawn from primitives
 * (vbe_*), no asset pipeline.
 */
#ifndef WUBU_BONZI_H
#define WUBU_BONZI_H

#include <stdint.h>
#include "../gui/dosgui_wm.h"   /* DosGuiWindow */

/* Public API: launch the Bonzi desktop agent window. */
DosGuiWindow *bonzi_launch(void);

/* Parse a human line and dispatch to real OS plumbing. Returns:
 *   a non-NULL reply string (static buffer, valid until next call) describing
 *   what Bonzi did. The actual side-effect (app launch / tool run) happens here. */
const char *bonzi_handle_line(const char *line);

/* Count of actions Bonzi has performed this session (plumbing proof). */
int bonzi_action_count(void);

#endif /* WUBU_BONZI_H */
