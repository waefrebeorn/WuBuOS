/*
 * wubu_chanmap.h -- kernel-owned audio channel map routing.
 */
#ifndef WUBU_CHANMAP_H
#define WUBU_CHANMAP_H

#include <stddef.h>

void wubu_chanmap_probe(void);
int  wubu_chanmap_present(void);
int  wubu_chanmap_stereo(void);
int  wubu_chanmap_51(void);
int  wubu_chanmap_71(void);
int  wubu_chanmap_chmap(void);
const char *wubu_chanmap_driver(void);
const char *wubu_chanmap_pos_for(const char *p);
const char *wubu_chanmap_layout_for(const char *l);
int wubu_chanmap_summary(char *out, size_t cap);

#endif
