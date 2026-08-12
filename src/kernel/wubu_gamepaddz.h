/*
 * wubu_gamepaddz.h -- kernel-owned gamepad deadzone routing.
 */
#ifndef WUBU_GAMEPADDZ_H
#define WUBU_GAMEPADDZ_H

#include <stddef.h>

void wubu_gamepaddz_probe(void);
int  wubu_gamepaddz_present(void);
int  wubu_gamepaddz_filter(int value, int deadzone);
int  wubu_gamepaddz_is_drift(int value, int deadzone);
void wubu_gamepaddz_summary(char *out, size_t cap);

#endif
