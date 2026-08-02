/*
 * fw_ata.c  --  WuBuFW ATA PIO block driver (28/48-bit LBA).
 *
 * QEMU's default disk on -drive if=ide is an IDE/ATA device on the legacy
 * ports, which is the one storage path guaranteed present without PCI
 * enumeration. Polled PIO only: firmware I/O is low volume and interrupt-free
 * keeps the whole boot path reentrancy-safe.
 */

#include "fw.h"
#include "fw_block.h"

#define ATA_MAX_DEV 4

/* Register offsets from the command block base. */
#define R_DATA      0
#define R_ERROR     1
#define R_FEATURES  1
#define R_SECCOUNT  2
#define R_LBA0      3
#define R_LBA1      4
#define R_LBA2      5
#define R_DRIVE     6
#define R_STATUS    7
#define R_COMMAND   7

#define ST_ERR  0x01
#define ST_DRQ  0x08
#define ST_SRV  0x10
#define ST_DF   0x20
#define ST_RDY  0x40
#define ST_BSY  0x80

#define CMD_READ_PIO      0x20
#define CMD_READ_PIO_EXT  0x24
#define CMD_WRITE_PIO     0x30
#define CMD_WRITE_PIO_EXT 0x34
#define CMD_CACHE_FLUSH   0xE7
#define CMD_IDENTIFY      0xEC

struct fw_ata_dev {
    uint16_t io;         /* command block base   */
    uint16_t ctrl;       /* control block base   */
    uint8_t  slave;      /* 0 master, 1 slave    */
    uint8_t  lba48;
    uint8_t  present;
    uint64_t sectors;
    char     model[41];
};

static struct fw_ata_dev g_dev[ATA_MAX_DEV];
static int g_ndev;

static void ata_delay400(struct fw_ata_dev *d) {
    for (int i = 0; i < 4; i++) (void)inb(d->ctrl);
}

static int ata_wait(struct fw_ata_dev *d, uint8_t mask, uint8_t want, uint64_t us) {
    uint64_t spins = us * 4;             /* ~250ns per poll on any real CPU */
    for (uint64_t i = 0; i < spins; i++) {
        uint8_t st = inb(d->io + R_STATUS);
        if (st == 0xFF) return -1;                 /* floating bus */
        if ((st & (ST_ERR | ST_DF)) && !(st & ST_BSY)) return -1;
        if ((st & mask) == want) return 0;
        __asm__ volatile("pause");
    }
    return -1;
}

static int ata_identify(struct fw_ata_dev *d) {
    outb(d->io + R_DRIVE, (uint8_t)(0xA0 | (d->slave << 4)));
    ata_delay400(d);
    outb(d->io + R_SECCOUNT, 0);
    outb(d->io + R_LBA0, 0);
    outb(d->io + R_LBA1, 0);
    outb(d->io + R_LBA2, 0);
    outb(d->io + R_COMMAND, CMD_IDENTIFY);
    ata_delay400(d);

    uint8_t st = inb(d->io + R_STATUS);
    if (st == 0 || st == 0xFF) return -1;           /* no device */
    if (ata_wait(d, ST_BSY, 0, 1000000) != 0) return -1;
    /* ATAPI/SATA signature check: non-zero LBA1/2 means not plain ATA. */
    if (inb(d->io + R_LBA1) != 0 || inb(d->io + R_LBA2) != 0) return -1;
    if (ata_wait(d, ST_DRQ, ST_DRQ, 1000000) != 0) return -1;

    uint16_t id[256];
    for (int i = 0; i < 256; i++) id[i] = inw(d->io + R_DATA);

    for (int i = 0; i < 20; i++) {
        d->model[i * 2]     = (char)(id[27 + i] >> 8);
        d->model[i * 2 + 1] = (char)(id[27 + i] & 0xFF);
    }
    d->model[40] = 0;
    for (int i = 39; i >= 0 && d->model[i] == ' '; i--) d->model[i] = 0;

    if (id[83] & (1 << 10)) {
        d->lba48   = 1;
        d->sectors = (uint64_t)id[100] | ((uint64_t)id[101] << 16) |
                     ((uint64_t)id[102] << 32) | ((uint64_t)id[103] << 48);
    } else {
        d->lba48   = 0;
        d->sectors = (uint64_t)id[60] | ((uint64_t)id[61] << 16);
    }
    if (d->sectors == 0) return -1;
    d->present = 1;
    return 0;
}

/* Block-layer adapters so the FAT/GPT layer is storage-agnostic. */
static int ata_blk_read(void *ctx, uint64_t lba, uint32_t n, void *buf) {
    return fw_ata_read((fw_ata_dev *)ctx, lba, n, buf);
}
static int ata_blk_write(void *ctx, uint64_t lba, uint32_t n, const void *buf) {
    return fw_ata_write((fw_ata_dev *)ctx, lba, n, (void *)buf);
}

int fw_ata_init(void) {
    static const uint16_t bases[2] = { 0x1F0, 0x170 };
    static const uint16_t ctrls[2] = { 0x3F6, 0x376 };
    g_ndev = 0;
    for (int ch = 0; ch < 2; ch++) {
        for (int sl = 0; sl < 2; sl++) {
            struct fw_ata_dev *d = &g_dev[g_ndev];
            fw_memset(d, 0, sizeof(*d));
            d->io = bases[ch];
            d->ctrl = ctrls[ch];
            d->slave = (uint8_t)sl;
            outb(d->ctrl, 0x02);              /* nIEN: no interrupts */
            if (ata_identify(d) == 0) {
                fw_printf("[ata] %d: %s  %lu sectors (%lu MB)%s\n",
                          g_ndev, d->model, d->sectors,
                          d->sectors / 2048, d->lba48 ? " lba48" : "");
                fw_block_register("ide", d, d->sectors, 512,
                                  ata_blk_read, ata_blk_write);
                g_ndev++;
            }
        }
    }
    return g_ndev;
}

int fw_ata_count(void) { return g_ndev; }

fw_ata_dev *fw_ata_get(int i) {
    if (i < 0 || i >= g_ndev) return NULL;
    return &g_dev[i];
}

uint64_t fw_ata_sectors(fw_ata_dev *d) { return d ? d->sectors : 0; }

static int ata_xfer(fw_ata_dev *d, uint64_t lba, uint32_t count, void *buf, int write) {
    if (!d || !d->present || !count) return -1;
    if (lba + count > d->sectors) return -1;

    uint8_t *p = (uint8_t *)buf;
    while (count) {
        uint32_t n = count;
        if (d->lba48) { if (n > 65536) n = 65536; }
        else          { if (n > 256)   n = 256;   }
        if (!d->lba48 && lba + n > 0x10000000ULL) return -1;

        if (ata_wait(d, ST_BSY, 0, 1000000) != 0) return -1;

        if (d->lba48) {
            outb(d->io + R_DRIVE, (uint8_t)(0x40 | (d->slave << 4)));
            outb(d->io + R_SECCOUNT, (uint8_t)((n >> 8) & 0xFF));
            outb(d->io + R_LBA0, (uint8_t)((lba >> 24) & 0xFF));
            outb(d->io + R_LBA1, (uint8_t)((lba >> 32) & 0xFF));
            outb(d->io + R_LBA2, (uint8_t)((lba >> 40) & 0xFF));
            outb(d->io + R_SECCOUNT, (uint8_t)(n & 0xFF));
            outb(d->io + R_LBA0, (uint8_t)(lba & 0xFF));
            outb(d->io + R_LBA1, (uint8_t)((lba >> 8) & 0xFF));
            outb(d->io + R_LBA2, (uint8_t)((lba >> 16) & 0xFF));
            outb(d->io + R_COMMAND, write ? CMD_WRITE_PIO_EXT : CMD_READ_PIO_EXT);
        } else {
            outb(d->io + R_DRIVE,
                 (uint8_t)(0xE0 | (d->slave << 4) | ((lba >> 24) & 0x0F)));
            outb(d->io + R_SECCOUNT, (uint8_t)(n & 0xFF));   /* 256 -> 0 */
            outb(d->io + R_LBA0, (uint8_t)(lba & 0xFF));
            outb(d->io + R_LBA1, (uint8_t)((lba >> 8) & 0xFF));
            outb(d->io + R_LBA2, (uint8_t)((lba >> 16) & 0xFF));
            outb(d->io + R_COMMAND, write ? CMD_WRITE_PIO : CMD_READ_PIO);
        }
        ata_delay400(d);

        for (uint32_t s = 0; s < n; s++) {
            if (ata_wait(d, ST_BSY | ST_DRQ, ST_DRQ, 3000000) != 0) return -1;
            if (write) {
                for (int w = 0; w < 256; w++) {
                    uint16_t v = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
                    outw(d->io + R_DATA, v);
                    p += 2;
                }
                ata_delay400(d);
            } else {
                for (int w = 0; w < 256; w++) {
                    uint16_t v = inw(d->io + R_DATA);
                    p[0] = (uint8_t)(v & 0xFF);
                    p[1] = (uint8_t)(v >> 8);
                    p += 2;
                }
            }
        }
        if (write) {
            outb(d->io + R_COMMAND, CMD_CACHE_FLUSH);
            if (ata_wait(d, ST_BSY, 0, 3000000) != 0) return -1;
        }
        lba   += n;
        count -= n;
    }
    return 0;
}

int fw_ata_read(fw_ata_dev *d, uint64_t lba, uint32_t count, void *buf) {
    return ata_xfer(d, lba, count, buf, 0);
}

int fw_ata_write(fw_ata_dev *d, uint64_t lba, uint32_t count, const void *buf) {
    return ata_xfer(d, lba, count, (void *)(uintptr_t)buf, 1);
}
