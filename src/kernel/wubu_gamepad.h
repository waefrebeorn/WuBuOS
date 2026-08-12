/*
 * wubu_gamepad.h -- kernel-owned game controller + display DSC routing.
 */
#ifndef WUBU_GAMEPAD_H
#define WUBU_GAMEPAD_H

#include <stddef.h>

/* W1: probe the gamepad/DSC topology. */
void wubu_gamepad_probe(void);

/* W2: accessors */
int  wubu_gamepad_present(void);
int  wubu_gamepad_wheel(void);
int  wubu_gamepad_arcade(void);
int  wubu_gamepad_dsc(void);
const char *wubu_gamepad_driver(void);
const char *wubu_gamepad_dsc_driver(void);

/* W3: routing. */
const char *wubu_gamepad_controller_for(const char *dev);
const char *wubu_gamepad_dsc_for(const char *gpu);

/* W4: summary fragment. */
int wubu_gamepad_summary(char *out, size_t cap);

#endif /* WUBU_GAMEPAD_H */
