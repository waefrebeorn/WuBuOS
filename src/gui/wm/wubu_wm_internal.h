/*
 * wubu_wm_internal.h  --  WuBuOS Window Manager (Internal)
 *
 * Shared internal header for WM sub-modules. Exposes the global WM state
 * so that desktop/input/render modules can access it without going
 * through function calls. External consumers should use wubu_wm.h only.
 */
#ifndef WUBU_WM_INTERNAL_H
#define WUBU_WM_INTERNAL_H

#include "wubu_wm.h"

/* Global WM state (defined in wubu_wm.c, used by sub-modules) */
extern WubuWM g_wm;

#endif /* WUBU_WM_INTERNAL_H */
