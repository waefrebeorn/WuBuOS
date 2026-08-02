/*
 * fw_block.c  --  WuBuFW unified block device layer.
 *
 * fw_media.c used to read straight from ATA PIO, which meant a machine with
 * no legacy IDE (any q35 / real modern board) had no boot volume at all. All
 * storage backends now register here and the FAT/GPT layer reads through one
 * interface, so booting from SATA/AHCI or NVMe works exactly like IDE.
 */

#include "fw.h"
#include "fw_block.h"

static fw_block_dev g_blk[8];
static int g_nblk;

int fw_block_register(const char *name, void *ctx, uint64_t sectors,
                      uint32_t sector_size,
                      int (*rd)(void *, uint64_t, uint32_t, void *),
                      int (*wr)(void *, uint64_t, uint32_t, const void *)) {
    if (g_nblk >= 8 || !rd) return -1;
    fw_block_dev *b = &g_blk[g_nblk];
    b->name = name;
    b->ctx = ctx;
    b->sectors = sectors;
    b->sector_size = sector_size ? sector_size : 512;
    b->read = rd;
    b->write = wr;
    fw_printf("[blk] %d: %s  %lu sectors x %u\n", g_nblk, name, sectors, b->sector_size);
    return g_nblk++;
}

int            fw_block_count(void) { return g_nblk; }
fw_block_dev  *fw_block_get(int i) { return (i >= 0 && i < g_nblk) ? &g_blk[i] : NULL; }

int fw_block_read(int i, uint64_t lba, uint32_t count, void *buf) {
    fw_block_dev *b = fw_block_get(i);
    if (!b || !b->read) return -1;
    return b->read(b->ctx, lba, count, buf);
}

int fw_block_write(int i, uint64_t lba, uint32_t count, const void *buf) {
    fw_block_dev *b = fw_block_get(i);
    if (!b || !b->write) return -1;
    return b->write(b->ctx, lba, count, buf);
}
