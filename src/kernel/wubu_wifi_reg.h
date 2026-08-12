/*
 * wubu_wifi_reg.h -- kernel-owned WiFi regulatory/DFS routing.
 */
#ifndef WUBU_WIFI_REG_H
#define WUBU_WIFI_REG_H

#include <stddef.h>

/* W1: probe the WiFi-regulatory topology. */
void wubu_wifi_reg_probe(void);

/* W2: accessors */
int  wubu_wifi_reg_db(void);
int  wubu_wifi_reg_crda(void);
int  wubu_wifi_reg_dfs(void);
int  wubu_wifi_reg_radar(void);
int  wubu_wifi_reg_country(void);
const char *wubu_wifi_reg_driver(void);

/* W3: regulatory routing. */
const char *wubu_wifi_reg_country_for(const char *code);
const char *wubu_wifi_reg_dfs_for(const char *band);

/* W4: summary fragment. */
int wubu_wifi_reg_summary(char *out, size_t cap);

#endif /* WUBU_WIFI_REG_H */
