/*
 * wubu_rescue_test.c -- WuBuOS emergency 16-bit rescue layer test.
 *
 * Verifies the on-disk contract of the unified boot disk:
 *   - boot sector (LBA 0) carries the 0x55AA signature and a rescue path,
 *   - the rescue shim is embedded at RESCUE_LBA (64) with its own signature,
 *   - the FreeDOS/WuBuDOS FAT image begins at LBA 65 with a valid BPB +
 *     0x55AA boot signature and KERNEL.SYS present at the root directory.
 *
 * This is the boot-time contract that wubu_exec_dos (the VSL-side detector)
 * and boot.S (the CPUID/key trigger) both depend on. It does NOT need KVM.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define RESCUE_LBA 64

static int g_run = 0, g_pass = 0;
#define T(c, m) do { g_run++; if (c) { g_pass++; printf("  ✅ %s\n", m); } \
                     else { printf("  ❌ %s\n", m); } } while (0)

static uint8_t *g_disk = NULL;
static size_t   g_disk_len = 0;

static int load_disk(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    g_disk = malloc(n);
    if (fread(g_disk, 1, n, f) != (size_t)n) { fclose(f); return -1; }
    g_disk_len = n;
    fclose(f);
    return 0;
}

static const uint8_t *sector(uint32_t lba) {
    size_t off = (size_t)lba * 512;
    if (off + 512 > g_disk_len) return NULL;
    return g_disk + off;
}

static bool fat_has_kernel_sys(const uint8_t *fat, size_t len) {
    /* Walk root dir (after reserved + FATs). We only need KERNEL.SYS present. */
    /* Decode BPB from the boot sector at LBA 65. */
    const uint8_t *bs = fat;
    if (bs[510] != 0x55 || bs[511] != 0xAA) return false;
    uint16_t bps = *(const uint16_t *)(bs + 11);
    uint8_t  spc = bs[13];
    uint16_t res = *(const uint16_t *)(bs + 14);
    uint8_t  fats = bs[16];
    uint16_t root = *(const uint16_t *)(bs + 17);
    (void)spc;
    uint32_t root_bytes = (uint32_t)root * 32;
    uint32_t fat_sectors = *(const uint16_t *)(bs + 22);
    uint32_t data_start = (uint32_t)(res + (uint32_t)fats * fat_sectors) * bps + root_bytes;
    /* KERNEL.SYS is 11 bytes "KERNEL  SYS". Scan the root dir region. */
    const uint8_t *rd = fat + (uint32_t)(res + (uint32_t)fats * fat_sectors) * bps;
    for (uint32_t i = 0; i + 11 <= root_bytes; i += 32) {
        if (memcmp(rd + i, "KERNEL  SYS", 11) == 0) return true;
    }
    (void)data_start;
    return false;
}

int main(int argc, char **argv) {
    const char *disk = (argc > 1) ? argv[1] : "vendor/freedos/wubu_rescue_disk.img";
    printf("=== WuBuOS Emergency 16-bit Rescue Layer Test ===\n\n");
    printf("[Rescue Disk Layout: %s]\n", disk);

    T(load_disk(disk) == 0, "rescue disk opens");
    if (g_run == g_pass) { /* only if loaded */
        const uint8_t *boot = sector(0);
        T(boot && boot[510] == 0x55 && boot[511] == 0xAA, "boot sector (LBA0) signature 55AA");

        const uint8_t *shim = sector(RESCUE_LBA);
        T(shim && shim[510] == 0x55 && shim[511] == 0xAA, "rescue shim (LBA64) signature 55AA");

        const uint8_t *rescue = sector(RESCUE_LBA + 1);
        T(rescue && rescue[510] == 0x55 && rescue[511] == 0xAA, "rescue FAT (LBA65) boot signature 55AA");
        T(rescue && fat_has_kernel_sys(rescue, g_disk_len), "rescue FAT contains KERNEL.SYS");
    }

    printf("\n=== Results: %d/%d passed ===\n", g_pass, g_run);
    free(g_disk);
    return (g_pass == g_run) ? 0 : 1;
}
