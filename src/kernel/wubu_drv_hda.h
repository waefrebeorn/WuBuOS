/*
 * wubu_drv_hda.h -- the HD Audio driver.
 */
#ifndef WUBU_DRV_HDA_H
#define WUBU_DRV_HDA_H

#include <stdint.h>

/* the driver (registered by the registry) */
extern const struct wubu_drv wubu_drv_hda;

/* the test hooks */
void wubu_hda_set_present(int present);
int wubu_hda_codec_present(void);
uint32_t wubu_hda_last_verb(void);
uint32_t wubu_hda_last_response(void);

#endif
