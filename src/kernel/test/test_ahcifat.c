/* host repro: FAT32 create through the REAL ahci sim backend
 * (the metal's exact path: ahci hba + sim disk + fat32_boot_attach) */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "fat32.h"
#include "ahci.h"
#include "ahci.c"
#include "fat32_format.c"
#include "fat32_fat.c"
#include "fat32_dir.c"
#include "fat32_file.c"
#include "fat32_name.c"
#include "fat32_cluster.c"
#include "wubu_lfn.c"
#include "memory.c"

void datetime_to_dos(time_t t, uint16_t *dos_time, uint16_t *dos_date)
{ (void)t; (void)dos_time; (void)dos_date; }

int klog_printf(const char *f, ...) { (void)f; return 0; }

static int blk_read(void *ctx, uint64_t lba, uint32_t n, void *buf)
{ return (ahci_read((ahci_hba_t *)ctx, 0, lba, n, buf) == (int)n) ? 0 : -1; }
static int blk_write(void *ctx, uint64_t lba, uint32_t n, const void *buf)
{ return (ahci_write((ahci_hba_t *)ctx, 0, lba, n, buf) == (int)n) ? 0 : -1; }

int main(void)
{
    mem_init(64 * 1024 * 1024);
    ahci_hba_t *hba = (ahci_hba_t *)calloc(1, sizeof(ahci_hba_t));
    printf("hba=%p\n", (void *)hba);
    if (ahci_hba_init(hba) != 0) { printf("hba_init fail\n"); return 1; }
    if (ahci_enumerate_ports(hba) <= 0) { printf("enumerate fail\n"); return 1; }
    if (ahci_port_init(hba, 0) != 0) { printf("port_init fail\n"); return 1; }
    if (ahci_sim_disk_create(hba, 0, 8) != 0) { printf("disk fail\n"); return 1; }
    fat32_blk_ops ops = { .read = blk_read, .write = blk_write,
                          .ctx = hba, .n_sectors = 16384 };
    printf("format: %d\n", fat32_format(&ops, 16384, "WUBUOS"));
    printf("mount: %d\n", fat32_mount(fat32_boot_volume(), &ops));
    printf("attach: %d\n", fat32_boot_attach(&ops));
    fat32_volume *v = fat32_boot_volume();
    fat32_file_info fi;
    fat32_file f;
    int cr = fat32_create(v, 0, "AGI.CKP", 0, &fi);
    printf("create=%d\n", cr);
    if (cr == 0) {
        int op = fat32_open(v, 0, "AGI.CKP", "w", &f);
        int wr = fat32_write(&f, "hello-ckp", 9);
        fat32_close(&f);
        printf("open=%d write=%d\n", op, wr);
    }
    printf("DONE\n");
    return 0;
}
