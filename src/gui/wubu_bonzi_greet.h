/*
 * wubu_bonzi_greet.h -- the companion's summon greeting (Track B3).
 *
 * Turns a loaded memory index into a personalized summon greeting: a
 * first-meeting greeting when memory is empty, a "welcome back" naming the
 * most recent activity when it isn't. Pure + dependency-light; the caller
 * loads the memory and hands this module the count + recent action labels.
 */
#ifndef WUBU_BONZI_GREET_H
#define WUBU_BONZI_GREET_H

/* Format a summon greeting into buf. recent[] holds up to recent_n recent
 * action labels (most-recent first); mem_n is the total memory count.
 * Returns bytes written (snprintf semantics). */
int wubu_bonzi_greet_format(char *buf, int cap, int mem_n,
                            const char *const *recent, int recent_n);

#endif /* WUBU_BONZI_GREET_H */
