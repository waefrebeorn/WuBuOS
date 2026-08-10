/*
 * wubu_pressure_vessel.h -- the pressure-vessel preset (SteamOS).
 */
#ifndef WUBU_PRESSURE_VESSEL_H
#define WUBU_PRESSURE_VESSEL_H

#include "wubu_host_exec.h"
#include <stddef.h>

typedef struct WubuPv WubuPv;

/* PV1: create the pressure-vessel container. runtime_lib = the Steam
 * Linux Runtime library base; game_lib = the game's bundled lib dir
 * ("" = none); game_exe = the game binary ("" = set later). */
WubuPv *wubu_pv_create(const char *name, const char *runtime_lib,
                       const char *game_lib, const char *game_exe);

/* PV2: set the game command after creation. */
int wubu_pv_set_game(WubuPv *pv, const char *game_exe);

/* PV3: launch via the bwrap runtime (the curated LD_LIBRARY_PATH is
 * applied: game libs first, the runtime fills the gaps). */
int wubu_pv_launch(WubuPv *pv);

/* PV4: teardown. */
void wubu_pv_destroy(WubuPv *pv);

/* PV5: the launch command preview. */
void wubu_pv_describe(const WubuPv *pv, char *buf, size_t cap);

/* PV6: the test hook — the WubuCt for the assertions. */
WubuCt *wubu_pv_ct(WubuPv *pv);

#endif
