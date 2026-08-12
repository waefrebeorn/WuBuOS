/*
 * wubu_panel.h -- kernel-owned GPU panel routing.
 */
#ifndef WUBU_PANEL_H
#define WUBU_PANEL_H

#include <stddef.h>

void wubu_panel_probe(void);
int  wubu_panel_present(void);
int  wubu_panel_lvds(void);
int  wubu_panel_edp(void);
int  wubu_panel_hdmi(void);
int  wubu_panel_dp(void);
int  wubu_panel_vga(void);
const char *wubu_panel_driver(void);
const char *wubu_panel_type_for(const char *t);
const char *wubu_panel_status_for(const char *s);
int wubu_panel_summary(char *out, size_t cap);

#endif
