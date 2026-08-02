/*
 * fw_block.h  --  WuBuFW block device abstraction.
 */

#ifndef WUBUFW_BLOCK_H
#define WUBUFW_BLOCK_H

#include <stdint.h>

typedef struct {
    const char *name;
    void       *ctx;
    uint64_t    sectors;
    uint32_t    sector_size;
    int (*read)(void *ctx, uint64_t lba, uint32_t count, void *buf);
    int (*write)(void *ctx, uint64_t lba, uint32_t count, const void *buf);
} fw_block_dev;

int           fw_block_register(const char *name, void *ctx, uint64_t sectors,
                                uint32_t sector_size,
                                int (*rd)(void *, uint64_t, uint32_t, void *),
                                int (*wr)(void *, uint64_t, uint32_t, const void *));
int           fw_block_count(void);
fw_block_dev *fw_block_get(int i);
int           fw_block_read(int i, uint64_t lba, uint32_t count, void *buf);
int           fw_block_write(int i, uint64_t lba, uint32_t count, const void *buf);

#endif
