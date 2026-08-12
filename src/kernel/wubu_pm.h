/*
 * wubu_pm.h -- kernel-owned power mode (S0ix/deep sleep/runtime PM) routing.
 */
#ifndef WUBU_PM_H
#define WUBU_PM_H

#include <stddef.h>

/* W1: probe the power-mode topology. */
void wubu_pm_probe(void);

/* W2: accessors */
int  wubu_pm_s0ix(void);
int  wubu_pm_s3(void);
int  wubu_pm_s4(void);
int  wubu_pm_runtime(void);
int  wubu_pm_cpuidle(void);
const char *wubu_pm_driver(void);
const char *wubu_pm_idle_driver(void);

/* W3: sleep-state routing. */
const char *wubu_pm_sleep_state(int mask);
const char *wubu_pm_idle_for(const char *cpu);

/* W4: summary fragment. */
int wubu_pm_summary(char *out, size_t cap);

#endif /* WUBU_PM_H */
