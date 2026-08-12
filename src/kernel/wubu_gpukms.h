/*
 * wubu_gpukms.h -- kernel-owned GPU KMS modeset routing.
 */
#ifndef WUBU_GPUKMS_H
#define WUBU_GPUKMS_H

#include <stddef.h>

void wubu_gpukms_probe(void);
int  wubu_gpukms_present(void);
int  wubu_gpukms_valid_mode(int width, int height, int refresh);
int  wubu_gpukms_is_active(int crtc_active, int connector_connected);
void wubu_gpukms_summary(char *out, size_t cap);

#endif
