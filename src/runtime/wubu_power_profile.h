/*
 * wubu_power_profile.h -- power profiles as data (power-saver / balanced / performance).
 *
 * The reference desktop ships power-profiles-daemon. Here the profiles are a table, the machine
 * state is read from sysfs, and every set is verified by reading the value back.
 */
#ifndef WUBU_POWER_PROFILE_H
#define WUBU_POWER_PROFILE_H

#include <stddef.h>

typedef enum {
    WUBU_POWER_UNKNOWN = 0,
    WUBU_POWER_SAVER   = 1,
    WUBU_POWER_BALANCED = 2,
    WUBU_POWER_PERF    = 3
} WubuPowerProfile;

typedef enum {
    WUBU_POWER_OK = 0,      /* written and read back */
    WUBU_POWER_DENIED,      /* no privilege */
    WUBU_POWER_REFUSED,     /* hardware said no, or nothing to touch */
    WUBU_POWER_ABSENT       /* this machine has no such control */
} WubuPowerSetResult;

typedef struct {
    int         id;
    const char *name;
    const char *platform;   /* /sys/firmware/acpi/platform_profile value */
    const char *governor;   /* cpufreq scaling_governor value */
    const char *why;
} WubuPowerProfileDef;

int  wubu_power_profile_count(void);
const WubuPowerProfileDef *wubu_power_profile_def(int i);

int  wubu_power_profile_current(void);
WubuPowerSetResult wubu_power_set(int id);
const char *wubu_power_result_str(WubuPowerSetResult r);

int  wubu_power_read_platform(char *out, size_t cap);
int  wubu_power_read_governor(int cpu, char *out, size_t cap);
int  wubu_power_describe(char *out, size_t cap);

#endif /* WUBU_POWER_PROFILE_H */
