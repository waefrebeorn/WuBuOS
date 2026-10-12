/*
 * wubu_bonzi_greet.c -- the companion's summon greeting (Track B3).
 *
 * When you summon the companion (Ctrl+/), it should recognize you and
 * recall what you were working on -- the persistent-memory payoff. This
 * module turns a loaded memory index into a personalized greeting string:
 * a first-meeting greeting when memory is empty, a "welcome back" that
 * names the most recent activity when it isn't.
 *
 * It is deliberately pure + dependency-light: the caller loads the memory
 * (wubu_bonzi_mem_load) and hands this module the count + the recent
 * action labels; this module formats the greeting. That keeps bonzi.c
 * from hard-depending on the memory store, and makes the greeting logic
 * unit-testable with no filesystem.
 *
 * C11, minimal includes.
 */

#include "wubu_bonzi_greet.h"
#include <stdio.h>
#include <string.h>

/* Format a summon greeting into buf. recent[] holds up to recent_n recent
 * action labels (most-recent first). mem_n is the total memory count.
 * Returns bytes written (snprintf semantics). */
int wubu_bonzi_greet_format(char *buf, int cap, int mem_n,
                            const char *const *recent, int recent_n) {
    if (!buf || cap <= 0) return 0;
    if (mem_n <= 0) {
        return snprintf(buf, cap,
                        "Hello! I'm WuBu. I don't have any memories of you "
                        "yet -- let's make some.");
    }
    int off = 0;
    off += snprintf(buf + off, cap - off,
                    "Welcome back! I remember %d thing%s about you.",
                    mem_n, mem_n == 1 ? "" : "s");
    if (recent && recent_n > 0 && recent[0] && recent[0][0]) {
        off += snprintf(buf + off, cap - off,
                        " Last time: %s", recent[0]);
        if (recent_n > 1 && recent[1] && recent[1][0])
            off += snprintf(buf + off, cap - off, ", and before that %s", recent[1]);
        off += snprintf(buf + off, cap - off, ".");
    }
    return off;
}
