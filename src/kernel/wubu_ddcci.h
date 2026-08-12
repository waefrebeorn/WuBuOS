/*
 * wubu_ddcci.h -- kernel-owned display panel DDC/CI control routing.
 */
#ifndef WUBU_DDCCI_H
#define WUBU_DDCCI_H

#include <stddef.h>

void wubu_ddcci_probe(void);
int  wubu_ddcci_present(void);
int  wubu_ddcci_i2c(void);
int  wubu_ddcci_cec(void);
int  wubu_ddcci_edid(void);
int  wubu_ddcci_ctrl(void);
const char *wubu_ddcci_driver(void);
const char *wubu_ddcci_cmd_for(const char *c);
const char *wubu_ddcci_bus_for(const char *b);
int wubu_ddcci_summary(char *out, size_t cap);

#endif
