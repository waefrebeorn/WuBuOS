/*
 * wubu_parental.h -- family limits: the hours and days this machine may be used (ParentalControls).
 *
 * Windows puts this behind a cloud account. WuBuOS is a machine, so the policy is a local file and the
 * DECISION is a pure function of (policy, clock) -- which is the only way it can be tested without
 * waiting for tomorrow morning.
 *
 * Every rule here can say WHY it refused. "Not allowed" with no reason is how a child (or an adult)
 * concludes the computer is broken; the screen shows the reason instead.
 *
 * $WUBU_PARENTAL_CONF overrides the file, so the gate runs on its own policy.
 *
 * C11.
 */
#ifndef WUBU_PARENTAL_H
#define WUBU_PARENTAL_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#define WUBU_PARENTAL_PATH_MAX 256

/* Monday is bit 0, Sunday bit 6 -- time.h's tm_wday has Sunday as 0, so the mapping lives in ONE place. */
#define WUBU_PARENTAL_ALL_DAYS 0x7Fu
#define WUBU_PARENTAL_WEEKDAYS 0x1Fu      /* Mon-Fri */
#define WUBU_PARENTAL_WEEKEND  0x60u      /* Sat, Sun */

typedef struct {
    bool     enabled;
    int      start_min;                   /* minutes after midnight, inclusive */
    int      end_min;                     /* minutes after midnight, exclusive; < start means overnight */
    unsigned days;                        /* bit 0 = Monday */
    char     source[WUBU_PARENTAL_PATH_MAX];  /* the file this came from ("" when there was none) */
    bool     loaded;                      /* a policy file was read (it may still be disabled) */
} WubuParentalPolicy;

const char *wubu_parental_conf_path(void);

/* Read the policy. Returns 0 when a policy file was read, -1 when there is none (the caller then has a
 * DISABLED policy, never an enabled one invented from nothing). */
int wubu_parental_load(WubuParentalPolicy *p);

/* The decision. `local` is a localtime() result for the moment in question. When use is not allowed,
 * `why` (if given) receives the sentence the screen shows. */
bool wubu_parental_allowed(const WubuParentalPolicy *p, const struct tm *local, char *why, size_t cap);

/* "08:00-20:00" for the panel/tests. */
void wubu_parental_window_str(const WubuParentalPolicy *p, char *out, size_t cap);
/* "Mon, Tue, Wed, Thu, Fri" for the panel/tests. */
void wubu_parental_days_str(const WubuParentalPolicy *p, char *out, size_t cap);

/* Write the policy back (used by the panel's editor). Returns 0 on success. */
int wubu_parental_save(const WubuParentalPolicy *p);

/* Clamp a policy into a sane shape: hours inside a day, at least one day, window not empty.
 * Returns true when it had to change something. */
bool wubu_parental_normalize(WubuParentalPolicy *p);

#endif
