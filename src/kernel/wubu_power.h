/*
 * wubu_power.h -- kernel-owned CPU/power/thermal driver routing + tuning.
 */
#ifndef WUBU_POWER_H
#define WUBU_POWER_H

#include <stddef.h>

/* W1: probe the power/CPU topology. */
void wubu_power_probe(void);

/* W2: cpufreq driver + governor + C-state selection. */
const char *wubu_power_cpufreq_driver(void);  /* intel_pstate|amd_pstate|cpufreq-dt|acpi-cpufreq */
const char *wubu_power_governor(void);        /* schedutil|performance|powersave */
const char *wubu_power_cstate_cap(void);      /* C-state cap param or NULL */

/* W3: accessors */
int  wubu_power_cpu_vendor(void);   /* 1=intel 2=amd 3=arm 0=other */
int  wubu_power_has_battery(void);
int  wubu_power_has_thermal(void);
int  wubu_power_has_fan(void);
int  wubu_power_ncores(void);

/* W4: summary fragment. */
int wubu_power_summary(char *out, size_t cap);

#endif /* WUBU_POWER_H */
