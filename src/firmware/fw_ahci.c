/*
 * fw_ahci.c  --  WuBuFW AHCI (SATA) driver.
 *
 * Real port init + command list / FIS / PRDT DMA. AHCI is how every modern
 * SATA disk is reached; the ATA PIO path in fw_ata.c only covers legacy IDE.
 * Read/write use the 48-bit DMA commands through a single command slot,
 * which is all firmware needs (no queueing, no NCQ).
 */

#include "fw.h"
#include "fw_pci.h"
#include "fw_block.h"

#define HBA_PORT_CMD_ST   0x0001
#define HBA_PORT_CMD_FRE  0x0010
#define HBA_PORT_CMD_FR   0x4000
#define HBA_PORT_CMD_CR   0x8000

#define ATA_CMD_READ_DMA_EX  0x25
#define ATA_CMD_WRITE_DMA_EX 0x35
#define ATA_CMD_IDENTIFY     0xEC

typedef volatile struct {
    uint32_t clb, clbu, fb, fbu, is, ie, cmd, rsv0;
    uint32_t tfd, sig, ssts, sctl, serr, sact, ci, sntf;
    uint32_t fbs, rsv1[11], vendor[4];
} hba_port;

typedef volatile struct {
    uint32_t cap, ghc, is, pi, vs, ccc_ctl, ccc_pts, em_loc, em_ctl, cap2, bohc;
    uint8_t  rsv[0xA0 - 0x2C];
    uint8_t  vendor[0x100 - 0xA0];
    hba_port ports[32];
} hba_mem;

typedef struct {
    uint8_t  cfl_a_w_p;
    uint8_t  r_b_c;
    uint16_t prdtl;
    volatile uint32_t prdbc;
    uint32_t ctba, ctbau;
    uint32_t rsv[4];
} __attribute__((packed)) hba_cmd_header;

typedef struct {
    uint32_t dba, dbau, rsv;
    uint32_t dbc_i;                 /* bit31 = interrupt, bits21:0 = count-1 */
} __attribute__((packed)) hba_prdt;

typedef struct {
    uint8_t  cfis[64];
    uint8_t  acmd[16];
    uint8_t  rsv[48];
    hba_prdt prdt[8];
} __attribute__((packed)) hba_cmd_table;

typedef struct {
    hba_port *port;
    hba_cmd_header *clist;          /* 32 entries, 1KB aligned  */
    void           *fis;            /* 256B, 256B aligned       */
    hba_cmd_table  *ctbl;           /* 256B aligned             */
    uint64_t        sectors;
    int             active;
} ahci_port_ctx;

static ahci_port_ctx g_ports[8];
static int g_nports;

static void port_stop(hba_port *p) {
    p->cmd &= ~HBA_PORT_CMD_ST;
    p->cmd &= ~HBA_PORT_CMD_FRE;
    for (int i = 0; i < 100000; i++) {
        if (!(p->cmd & (HBA_PORT_CMD_FR | HBA_PORT_CMD_CR))) break;
        fw_stall_us(10);
    }
}

static void port_start(hba_port *p) {
    while (p->cmd & HBA_PORT_CMD_CR) fw_stall_us(10);
    p->cmd |= HBA_PORT_CMD_FRE;
    p->cmd |= HBA_PORT_CMD_ST;
}

/* Issue one command on slot 0 and wait for completion. */
static int ahci_run(ahci_port_ctx *c, const uint8_t fis[20], void *buf,
                    uint32_t bytes, int write) {
    hba_port *p = c->port;
    p->is = (uint32_t)-1;

    hba_cmd_header *h = &c->clist[0];
    fw_memset(h, 0, sizeof(*h));
    h->cfl_a_w_p = (uint8_t)((20 / 4) | (write ? 0x40 : 0));
    h->prdtl = bytes ? 1 : 0;
    h->prdbc = 0;
    h->ctba  = (uint32_t)(uintptr_t)c->ctbl;
    h->ctbau = (uint32_t)((uint64_t)(uintptr_t)c->ctbl >> 32);

    fw_memset(c->ctbl, 0, sizeof(*c->ctbl));
    fw_memcpy(c->ctbl->cfis, fis, 20);
    if (bytes) {
        c->ctbl->prdt[0].dba  = (uint32_t)(uintptr_t)buf;
        c->ctbl->prdt[0].dbau = (uint32_t)((uint64_t)(uintptr_t)buf >> 32);
        c->ctbl->prdt[0].dbc_i = (bytes - 1) & 0x3FFFFF;
    }

    /* Wait for the device to leave BSY/DRQ. */
    for (int i = 0; i < 1000000; i++) {
        if (!(p->tfd & 0x88)) break;
        fw_stall_us(10);
    }
    if (p->tfd & 0x88) return -1;

    p->ci = 1;
    for (int i = 0; i < 3000000; i++) {
        if (!(p->ci & 1)) break;
        if (p->is & (1u << 30)) return -1;       /* task file error */
        fw_stall_us(10);
    }
    if (p->ci & 1) return -1;
    if (p->is & (1u << 30)) return -1;
    if (p->tfd & 0x01) return -1;                /* ERR */
    return 0;
}

static int ahci_identify(ahci_port_ctx *c) {
    static uint16_t id[256] __attribute__((aligned(4096)));
    uint8_t fis[20];
    fw_memset(fis, 0, sizeof(fis));
    fis[0] = 0x27;                 /* Register H2D */
    fis[1] = 0x80;                 /* command      */
    fis[2] = ATA_CMD_IDENTIFY;
    if (ahci_run(c, fis, id, 512, 0) != 0) return -1;

    if (id[83] & (1 << 10)) {
        c->sectors = ((uint64_t)id[103] << 48) | ((uint64_t)id[102] << 32) |
                     ((uint64_t)id[101] << 16) | id[100];
    } else {
        c->sectors = ((uint32_t)id[61] << 16) | id[60];
    }
    return 0;
}

int fw_ahci_read(int idx, uint64_t lba, uint32_t count, void *buf) {
    if (idx < 0 || idx >= g_nports || !g_ports[idx].active) return -1;
    uint8_t fis[20];
    fw_memset(fis, 0, sizeof(fis));
    fis[0] = 0x27; fis[1] = 0x80;
    fis[2] = ATA_CMD_READ_DMA_EX;
    fis[4] = (uint8_t)lba; fis[5] = (uint8_t)(lba >> 8); fis[6] = (uint8_t)(lba >> 16);
    fis[7] = 0x40;                                     /* LBA mode */
    fis[8] = (uint8_t)(lba >> 24); fis[9] = (uint8_t)(lba >> 32); fis[10] = (uint8_t)(lba >> 40);
    fis[12] = (uint8_t)count; fis[13] = (uint8_t)(count >> 8);
    return ahci_run(&g_ports[idx], fis, buf, count * 512, 0);
}

int fw_ahci_write(int idx, uint64_t lba, uint32_t count, const void *buf) {
    if (idx < 0 || idx >= g_nports || !g_ports[idx].active) return -1;
    uint8_t fis[20];
    fw_memset(fis, 0, sizeof(fis));
    fis[0] = 0x27; fis[1] = 0x80;
    fis[2] = ATA_CMD_WRITE_DMA_EX;
    fis[4] = (uint8_t)lba; fis[5] = (uint8_t)(lba >> 8); fis[6] = (uint8_t)(lba >> 16);
    fis[7] = 0x40;
    fis[8] = (uint8_t)(lba >> 24); fis[9] = (uint8_t)(lba >> 32); fis[10] = (uint8_t)(lba >> 40);
    fis[12] = (uint8_t)count; fis[13] = (uint8_t)(count >> 8);
    return ahci_run(&g_ports[idx], fis, (void *)buf, count * 512, 1);
}

uint64_t fw_ahci_sectors(int idx) {
    if (idx < 0 || idx >= g_nports) return 0;
    return g_ports[idx].sectors;
}
int fw_ahci_count(void) { return g_nports; }

/* Block-layer adapters: ctx is the port context, so FAT/GPT can read from
 * SATA exactly as it does from IDE. */
static int ahci_blk_read(void *ctx, uint64_t lba, uint32_t n, void *buf) {
    ahci_port_ctx *c = ctx;
    uint8_t fis[20];
    fw_memset(fis, 0, sizeof(fis));
    fis[0] = 0x27; fis[1] = 0x80; fis[2] = ATA_CMD_READ_DMA_EX;
    fis[4] = (uint8_t)lba; fis[5] = (uint8_t)(lba >> 8); fis[6] = (uint8_t)(lba >> 16);
    fis[7] = 0x40;
    fis[8] = (uint8_t)(lba >> 24); fis[9] = (uint8_t)(lba >> 32); fis[10] = (uint8_t)(lba >> 40);
    fis[12] = (uint8_t)n; fis[13] = (uint8_t)(n >> 8);
    return ahci_run(c, fis, buf, n * 512, 0);
}

static int ahci_blk_write(void *ctx, uint64_t lba, uint32_t n, const void *buf) {
    ahci_port_ctx *c = ctx;
    uint8_t fis[20];
    fw_memset(fis, 0, sizeof(fis));
    fis[0] = 0x27; fis[1] = 0x80; fis[2] = ATA_CMD_WRITE_DMA_EX;
    fis[4] = (uint8_t)lba; fis[5] = (uint8_t)(lba >> 8); fis[6] = (uint8_t)(lba >> 16);
    fis[7] = 0x40;
    fis[8] = (uint8_t)(lba >> 24); fis[9] = (uint8_t)(lba >> 32); fis[10] = (uint8_t)(lba >> 40);
    fis[12] = (uint8_t)n; fis[13] = (uint8_t)(n >> 8);
    return ahci_run(c, fis, (void *)buf, n * 512, 1);
}

int fw_ahci_init(fw_pci_dev *d) {
    if (!d) return -1;
    if (!d->bar[5].addr) return -1;
    hba_mem *hba = (hba_mem *)(uintptr_t)d->bar[5].addr;

    /* Take ownership from any BIOS/SMM handoff before touching ports. */
    if (hba->cap2 & 1) {
        hba->bohc |= 2;                               /* OS ownership request */
        for (int i = 0; i < 100000; i++) {
            if (!(hba->bohc & 1)) break;
            fw_stall_us(10);
        }
    }

    hba->ghc |= (1u << 31);                           /* AHCI enable */

    uint32_t pi = hba->pi;
    int found = 0;
    for (int i = 0; i < 32 && g_nports < 8; i++) {
        if (!(pi & (1u << i))) continue;
        hba_port *p = &hba->ports[i];

        uint32_t det = p->ssts & 0xF;
        uint32_t ipm = (p->ssts >> 8) & 0xF;
        if (det != 3 || ipm != 1) continue;           /* no device present */

        ahci_port_ctx *c = &g_ports[g_nports];
        fw_memset(c, 0, sizeof(*c));
        c->port = p;

        /* One 4KB page holds the command list (1KB) + FIS (256B) + one
         * command table, all satisfying their alignment requirements. */
        void *page = fw_alloc_pages_aligned(2, 4096);
        if (!page) break;
        fw_memset(page, 0, 8192);
        c->clist = (hba_cmd_header *)page;
        c->fis   = (uint8_t *)page + 1024;
        c->ctbl  = (hba_cmd_table *)((uint8_t *)page + 4096);

        port_stop(p);
        p->clb  = (uint32_t)(uintptr_t)c->clist;
        p->clbu = (uint32_t)((uint64_t)(uintptr_t)c->clist >> 32);
        p->fb   = (uint32_t)(uintptr_t)c->fis;
        p->fbu  = (uint32_t)((uint64_t)(uintptr_t)c->fis >> 32);
        p->serr = (uint32_t)-1;
        p->is   = (uint32_t)-1;
        p->ie   = 0;
        port_start(p);

        /*
         * PxSIG only becomes valid after the port is running and the device
         * has delivered its first D2H FIS; reading it before ST/FRE (the
         * obvious ordering) returns 0xFFFFFFFF and rejects every real disk.
         */
        uint32_t sig = 0xFFFFFFFFu;
        for (int t = 0; t < 100000; t++) {
            sig = p->sig;
            if (sig != 0xFFFFFFFFu && !(p->tfd & 0x88)) break;
            fw_stall_us(10);
        }
        if (sig != 0x00000101) {                      /* SATA disk only */
            fw_printf("[ahci] port %d: sig %x is not a SATA disk\n", i, sig);
            port_stop(p);
            continue;
        }

        c->active = 1;
        if (ahci_identify(c) != 0) { c->active = 0; port_stop(p); continue; }

        fw_printf("[ahci] port %d: %lu sectors (%lu MB)\n",
                  i, c->sectors, c->sectors / 2048);
        fw_block_register("ahci", c, c->sectors, 512, ahci_blk_read, ahci_blk_write);
        g_nports++;
        found = 1;
    }
    return found ? 0 : -1;
}
