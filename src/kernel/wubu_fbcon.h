/*
 * wubu_fbcon.h -- kernel-owned GPU framebuffer console routing.
 */
#ifndef WUBU_FBCON_H
#define WUBU_FBCON_H

#include <stddef.h>

void wubu_fbcon_probe(void);
int  wubu_fbcon_present(void);
int  wubu_fbcon_drm(void);
int  wubu_fbcon_rotate(void);
int  wubu_fbcon_virtual(void);
int  wubu_fbcon_mode(void);
const char *wubu_fbcon_driver(void);
const char *wubu_fbcon_rotate_for(const char *r);
const char *wubu_fbcon_mode_for(const char *m);
int wubu_fbcon_summary(char *out, size_t cap);

#endif
