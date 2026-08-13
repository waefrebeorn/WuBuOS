/* quick host check of fat32_boot_attach on the fat32_test RAM disk */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "fat32.h"

/* the datetime_to_dos helper is a metal/CMOS module: stub it for the
 * host test (the format path doesn't need real time) */
uint32_t datetime_to_dos(int y, int m, int d, int hh, int mm, int ss)
{
    (void)y; (void)m; (void)d; (void)hh; (void)mm; (void)ss;
    return 0;
}

#define RAM_DISK_SECTORS 16384
static uint8_t *g_ram_disk;

static int ram_read(void *ctx, uint64_t lba, uint32_t n, void *buf)
{
    (void)ctx;
    if (lba + n > RAM_DISK_SECTORS) return -1;
    memcpy(buf, g_ram_disk + lba * 512, (size_t)n * 512);
    return 0;
}
static int ram_write(void *ctx, uint64_t lba, uint32_t n, const void *buf)
{
    (void)ctx;
    if (lba + n > RAM_DISK_SECTORS) return -1;
    memcpy(g_ram_disk + lba * 512, buf, (size_t)n * 512);
    return 0;
}

int main(void)
{
    g_ram_disk = (uint8_t *)calloc(RAM_DISK_SECTORS, 512);
    fat32_blk_ops ops = {
        .read = ram_read, .write = ram_write,
        .ctx = NULL, .n_sectors = RAM_DISK_SECTORS,
    };
    /* fresh zeroed disk: boot_attach must FORMAT + mount */
    int rc1 = fat32_boot_attach(&ops);
    printf("attach-fresh: %d (expect 0)\n", rc1);
    /* attach again: already mounted, no reformat */
    printf("attach-again: %d (expect 0)\n", fat32_boot_attach(&ops));
    /* the media now carries a real FAT32 volume: a file roundtrip */
    fat32_volume *v = fat32_boot_volume();
    fat32_file_info fi;
    fat32_file f;
    int cr = fat32_create(v, 0, "AGI.CKP", 0, &fi);
    int op = fat32_open(v, 0, "AGI.CKP", "w", &f);
    int wr = fat32_write(&f, "hello-ckp", 9);
    fat32_close(&f);
    int fl = fat32_flush(v);
    printf("create=%d open=%d write=%d flush=%d (all expect 0/9)\n",
           cr, op, wr, fl);
    printf("boot_attach host check DONE\n");
    return 0;
}
