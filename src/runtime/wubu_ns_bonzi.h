/*
 * wubu_ns_bonzi.h -- the Bonzi companion's /n control plane.
 *
 * The AGI face is a first-class OS citizen: its live state is mirrored into
 * the /n namespace so `cat /n/bonzi/*` shows what the agent IS, and the
 * shell (or another agent) can drive it. Same doctrine as /n/kernel and
 * /n/dram. See wubu_ns_bonzi.c.
 */
#ifndef WUBU_NS_BONZI_H
#define WUBU_NS_BONZI_H

/* Push the live companion state into the namespace mirror. The renderer
 * calls these each tick with the real values, then wubu_ns_bonzi_sync()
 * flushes them to /n/bonzi/*. */
void wubu_ns_bonzi_set_enabled(int on);
void wubu_ns_bonzi_set_pos(int x, int y);
void wubu_ns_bonzi_set_size(int w, int h);
void wubu_ns_bonzi_set_agi(int open);
void wubu_ns_bonzi_set_bubble(const char *text);
void wubu_ns_bonzi_set_activity(const char *activity);

/* Read back the mirror (the single source of truth for /n/bonzi). */
int wubu_ns_bonzi_enabled(void);
int wubu_ns_bonzi_agi_open(void);

/* Flush the mirror into /n/bonzi/*. Returns 0 on success, -1 if the
 * namespace root isn't set. Call after the setters, from the render tick
 * or once after init. */
int wubu_ns_bonzi_sync(void);

#endif /* WUBU_NS_BONZI_H */
