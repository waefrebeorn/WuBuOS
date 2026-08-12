/*
 * wubu_backlightpwm.h -- kernel-owned display backlight PWM routing.
 */
#ifndef WUBU_BACKLIGHTPWM_H
#define WUBU_BACKLIGHTPWM_H

#include <stddef.h>

/* W1: probe the backlight topology. */
void wubu_backlightpwm_probe(void);

/* W2: accessors */
int  wubu_backlightpwm_present(void);
int  wubu_backlightpwm_pwm(void);
int  wubu_backlightpwm_sysfs(void);
int  wubu_backlightpwm_acpi(void);
int  wubu_backlightpwm_intel(void);
const char *wubu_backlightpwm_driver(void);

/* W3: backlight routing. */
const char *wubu_backlightpwm_type_for(const char *t);
const char *wubu_backlightpwm_brightness_for(const char *b);

/* W4: summary fragment. */
int wubu_backlightpwm_summary(char *out, size_t cap);

#endif /* WUBU_BACKLIGHTPWM_H */
