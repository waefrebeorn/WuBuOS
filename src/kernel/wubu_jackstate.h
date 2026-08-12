/*
 * wubu_jackstate.h -- kernel-owned audio jack state machine routing.
 */
#ifndef WUBU_JACKSTATE_H
#define WUBU_JACKSTATE_H

#include <stddef.h>

void wubu_jackstate_probe(void);
int  wubu_jackstate_present(void);
int  wubu_jackstate_plug(void);
int  wubu_jackstate_unplug(void);
int  wubu_jackstate_debounce(void);
int  wubu_jackstate_stable(void);
const char *wubu_jackstate_driver(void);
const char *wubu_jackstate_machine_for(const char *s);
const char *wubu_jackstate_event_for(const char *e);
int wubu_jackstate_summary(char *out, size_t cap);

#endif
