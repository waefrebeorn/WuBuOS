/*
 * wubu_accel.h -- kernel-owned NPU/accelerator driver routing.
 */
#ifndef WUBU_ACCEL_H
#define WUBU_ACCEL_H

#include <stddef.h>

/* W1: probe the accelerator topology. */
void wubu_accel_probe(void);

/* W2: accessors */
int  wubu_accel_present(void);
int  wubu_accel_has_npu(void);
int  wubu_accel_has_dsp(void);
int  wubu_accel_npu_vendor(void);   /* PCI vendor of the NPU */
const char *wubu_accel_driver(void);  /* ivpu|amdxdna|qaic|edgetpu|accel */
const char *wubu_accel_npu_name(void);

/* W3: NPU driver routing per vendor. */
const char *wubu_accel_npu_driver(int vendor);

/* W4: summary fragment. */
int wubu_accel_summary(char *out, size_t cap);

#endif /* WUBU_ACCEL_H */
