/*
 * wubu_writeback.h -- kernel-owned storage writeback routing.
 */
#ifndef WUBU_WRITEBACK_H
#define WUBU_WRITEBACK_H

#include <stddef.h>

void wubu_writeback_probe(void);
int  wubu_writeback_present(void);
int  wubu_writeback_dirty(void);
int  wubu_writeback_sync(void);
int  wubu_writeback_interval(void);
int  wubu_writeback_thread(void);
const char *wubu_writeback_driver(void);
const char *wubu_writeback_mode_for(const char *m);
const char *wubu_writeback_thread_for(const char *t);
int wubu_writeback_summary(char *out, size_t cap);

#endif
