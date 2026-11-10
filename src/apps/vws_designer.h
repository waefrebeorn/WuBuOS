/*
 * vws_designer.h -- launch the VWS form designer.
 *
 * The designer is a .wet applet written in Lua; this launcher builds the same
 * stack it designs for (WET + WUA + VWS in one interpreter) and puts it in a
 * WuBuOS window.
 */
#ifndef VWS_DESIGNER_H
#define VWS_DESIGNER_H

#include "../gui/dosgui_wm.h"

#ifdef __cplusplus
extern "C" {
#endif

DosGuiWindow *dosgui_launch_vws_designer(void);

/* The last launch error, or NULL. */
const char *vws_designer_error(void);

#ifdef __cplusplus
}
#endif

#endif /* VWS_DESIGNER_H */
