/*
 * wubu_fpga.h -- kernel-owned FPGA driver routing.
 */
#ifndef WUBU_FPGA_H
#define WUBU_FPGA_H

#include <stddef.h>

/* W1: probe the FPGA topology. */
void wubu_fpga_probe(void);

/* W2: accessors */
int  wubu_fpga_present(void);
int  wubu_fpga_has_mgr(void);
int  wubu_fpga_has_region(void);
int  wubu_fpga_has_bridge(void);
const char *wubu_fpga_driver(void);

/* W3: FPGA manager driver routing. */
const char *wubu_fpga_mgr_driver(const char *vendor);

/* W4: summary fragment. */
int wubu_fpga_summary(char *out, size_t cap);

#endif /* WUBU_FPGA_H */
