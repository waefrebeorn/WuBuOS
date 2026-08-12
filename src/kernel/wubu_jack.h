/*
 * wubu_jack.h -- kernel-owned audio jack detection routing.
 */
#ifndef WUBU_JACK_H
#define WUBU_JACK_H

#include <stddef.h>

void wubu_jack_probe(void);
int  wubu_jack_present(void);
int  wubu_jack_headphone(void);
int  wubu_jack_mic(void);
int  wubu_jack_spdif(void);
int  wubu_jack_impedance(void);
const char *wubu_jack_driver(void);
const char *wubu_jack_type_for(const char *t);
const char *wubu_jack_state_for(const char *s);
int wubu_jack_summary(char *out, size_t cap);

#endif
