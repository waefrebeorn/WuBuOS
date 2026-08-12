/*
 * wubu_dsptrace.h -- kernel-owned audio DSP trace/debug routing.
 */
#ifndef WUBU_DSPTRACE_H
#define WUBU_DSPTRACE_H

#include <stddef.h>

void wubu_dsptrace_probe(void);
int  wubu_dsptrace_present(void);
int  wubu_dsptrace_level(int level);
const char *wubu_dsptrace_evt(int code);
void wubu_dsptrace_summary(char *out, size_t cap);

#endif
