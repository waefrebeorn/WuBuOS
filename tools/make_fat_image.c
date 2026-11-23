#include <stdio.h>
#include <string.h>
#include <stdint.h>
#define main fat32_format_main
#include "fat32.h"
#undef main

static unsigned char ram[1024 * 1024];

static int rd(void *ctx, uint64_t lba, uint32_t n, void *buf) {
    (void)ctx;
    if ((lba + n) * 512 > sizeof ram) return -1;
    memcpy(buf, ram + lba * 512, n * 512);
    return 0;
}
static int wr(void *ctx, uint64_t lba, uint32_t n, const void *buf) {
    (void)ctx;
    if ((lba + n) * 512 > sizeof ram) return -1;
    memcpy(ram + lba * 512, buf, n * 512);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    fat32_blk_ops ops = { rd, wr, NULL, 2048 };
    if (fat32_format(&ops, 2048, "WUBUFS") != 0) return 3;
    FILE *f = fopen(argv[1], "wb");
    if (!f) return 4;
    if (fwrite(ram, 1, sizeof ram, f) != sizeof ram) return 5;
    fclose(f);
    return 0;
}
