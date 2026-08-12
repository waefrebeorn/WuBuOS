/*
 * wubu_storage.h -- kernel-owned storage driver routing + tuning interface.
 */
#ifndef WUBU_STORAGE_H
#define WUBU_STORAGE_H

#include <stddef.h>

/* W1: probe the storage topology (NVMe/SATA/IDE, Intel RST lockout). */
void wubu_storage_probe(void);

/* W2: accessors */
int          wubu_storage_has_nvme(void);
int          wubu_storage_has_sata(void);
int          wubu_storage_has_ide(void);
int          wubu_storage_has_raid_rst(void);   /* Intel RST lockout */
int          wubu_storage_queue_depth(void);
const char *wubu_storage_path(void);

/* W3: kernel cmdline tuning (APST off, queue depth, RST). */
const char *wubu_storage_kernel_params(void);

/* W4: TRIM/discard fstab fragment. */
const char *wubu_storage_trim_config(void);

/* W5: Intel RST lockout warning (NULL if not needed). */
const char *wubu_storage_rst_warning(void);

/* W6: summary fragment. */
int wubu_storage_summary(char *out, size_t cap);

#endif /* WUBU_STORAGE_H */
