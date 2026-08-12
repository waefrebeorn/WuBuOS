/*
 * wubu_fw.h -- kernel-owned storage controller firmware routing.
 */
#ifndef WUBU_FW_H
#define WUBU_FW_H

#include <stddef.h>

/* W1: probe the FW topology. */
void wubu_fw_probe(void);

/* W2: accessors */
int  wubu_fw_loader(void);
int  wubu_fw_lib(void);
int  wubu_fw_raid(void);
int  wubu_fw_hba(void);
int  wubu_fw_update(void);
const char *wubu_fw_driver(void);

/* W3: firmware routing. */
const char *wubu_fw_controller_for(const char *ctrl);
const char *wubu_fw_stage_for(const char *stage);

/* W4: summary fragment. */
int wubu_fw_summary(char *out, size_t cap);

#endif /* WUBU_FW_H */
