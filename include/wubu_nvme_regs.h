/*
 * wubu_nvme_regs.h -- NVMe controller register offsets. ONE owner.
 *
 * Shared by the firmware boot driver (src/firmware/fw_nvme.c) and the
 * in-kernel driver (src/kernel/wubu_drv_nvme.c). These are the NVMe
 * specification's offsets, not ours to tune. Created by the org-wide
 * hardcoding census (2026-11-19): both layers kept private tables.
 */
#ifndef WUBU_NVME_REGS_H
#define WUBU_NVME_REGS_H

#define NVME_REG_CAP     0x0000   /* controller capabilities */
#define NVME_REG_VS      0x0008   /* version */
#define NVME_REG_INTMS   0x000C   /* interrupt mask set */
#define NVME_REG_INTMC   0x0010   /* interrupt mask clear */
#define NVME_REG_CC      0x0014   /* controller config */
#define NVME_REG_CSTS    0x001C   /* controller status */
#define NVME_REG_AQA     0x0024   /* admin queue attributes */
#define NVME_REG_ASQ     0x0028   /* admin SQ base */
#define NVME_REG_ACQ     0x0030   /* admin CQ base */
#define NVME_REG_DBS     0x1000   /* doorbells */

#endif /* WUBU_NVME_REGS_H */