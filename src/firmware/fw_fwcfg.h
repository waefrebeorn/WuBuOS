/*
 * fw_fwcfg.h  --  QEMU fw_cfg access.
 */

#ifndef WUBUFW_FWCFG_H
#define WUBUFW_FWCFG_H

#include <stdint.h>

int      fw_cfg_init(void);
int      fw_cfg_present(void);
int      fw_cfg_read_file(const char *name, void *buf, uint32_t max, uint32_t *out_len);
uint32_t fw_cfg_file_size(const char *name);

/* Executes etc/table-loader; returns the built RSDP or NULL. */
void    *fw_acpi_load_from_fwcfg(void);

#endif
