/*
 * fw_nvme.c  --  WuBuFW NVMe driver.
 *
 * Admin queue + one I/O queue pair, polled completions. Enough to identify
 * the first namespace and do LBA reads/writes, which is what a firmware boot
 * path needs from an NVMe SSD.
 */

#include "fw.h"
#include "fw_pci.h"
#include "fw_block.h"

#define NVME_REG_CAP    0x00
#define NVME_REG_CC     0x14
#define NVME_REG_CSTS   0x1C
#define NVME_REG_AQA    0x24
#define NVME_REG_ASQ    0x28
#define NVME_REG_ACQ    0x30

#define QDEPTH 8

typedef struct {
    uint32_t cdw0;
    uint32_t nsid;
    uint64_t rsv;
    uint64_t mptr;
    uint64_t prp1;
    uint64_t prp2;
    uint32_t cdw10, cdw11, cdw12, cdw13, cdw14, cdw15;
} __attribute__((packed)) nvme_sqe;

typedef struct {
    uint32_t dw0, dw1;
    uint16_t sqhd, sqid;
    uint16_t cid;
    uint16_t status;
} __attribute__((packed)) nvme_cqe;

typedef struct {
    nvme_sqe *sq;
    nvme_cqe *cq;
    volatile uint32_t *sq_db, *cq_db;
    uint16_t sq_tail, cq_head;
    uint8_t  phase;
} nvme_queue;

static volatile uint8_t *g_bar;
static nvme_queue g_admin, g_io;
static uint64_t   g_nsze;
static uint32_t   g_lba_shift = 9;
static int        g_ready;
static uint16_t   g_cid;

static uint32_t reg32(uint32_t off) { return *(volatile uint32_t *)(g_bar + off); }
static void wreg32(uint32_t off, uint32_t v) { *(volatile uint32_t *)(g_bar + off) = v; }
static void wreg64(uint32_t off, uint64_t v) { *(volatile uint64_t *)(g_bar + off) = v; }

/* Submit one command and poll its completion. Returns the status field. */
static int nvme_submit(nvme_queue *q, nvme_sqe *cmd) {
    uint16_t cid = ++g_cid;
    cmd->cdw0 = (cmd->cdw0 & 0xFFFF) | ((uint32_t)cid << 16);

    q->sq[q->sq_tail] = *cmd;
    q->sq_tail = (uint16_t)((q->sq_tail + 1) % QDEPTH);
    *q->sq_db = q->sq_tail;

    for (int i = 0; i < 3000000; i++) {
        nvme_cqe *c = &q->cq[q->cq_head];
        if ((c->status & 1) == q->phase) {
            uint16_t sc = (uint16_t)(c->status >> 1);
            q->cq_head = (uint16_t)((q->cq_head + 1) % QDEPTH);
            if (q->cq_head == 0) q->phase ^= 1;
            *q->cq_db = q->cq_head;
            return sc == 0 ? 0 : -1;
        }
        fw_stall_us(10);
    }
    return -1;
}

int fw_nvme_read(uint64_t lba, uint32_t count, void *buf) {
    if (!g_ready || !count) return -1;
    nvme_sqe c;
    fw_memset(&c, 0, sizeof(c));
    c.cdw0 = 0x02;                          /* opcode: Read */
    c.nsid = 1;
    c.prp1 = (uint64_t)(uintptr_t)buf;
    /* A transfer crossing a page boundary needs PRP2 pointing at the next. */
    uint64_t off = (uint64_t)(uintptr_t)buf & 0xFFF;
    if (off + (uint64_t)count * 512 > 4096)
        c.prp2 = (((uint64_t)(uintptr_t)buf) & ~0xFFFull) + 4096;
    c.cdw10 = (uint32_t)lba;
    c.cdw11 = (uint32_t)(lba >> 32);
    c.cdw12 = count - 1;
    return nvme_submit(&g_io, &c);
}

int fw_nvme_write(uint64_t lba, uint32_t count, const void *buf) {
    if (!g_ready || !count) return -1;
    nvme_sqe c;
    fw_memset(&c, 0, sizeof(c));
    c.cdw0 = 0x01;                          /* opcode: Write */
    c.nsid = 1;
    c.prp1 = (uint64_t)(uintptr_t)buf;
    uint64_t off = (uint64_t)(uintptr_t)buf & 0xFFF;
    if (off + (uint64_t)count * 512 > 4096)
        c.prp2 = (((uint64_t)(uintptr_t)buf) & ~0xFFFull) + 4096;
    c.cdw10 = (uint32_t)lba;
    c.cdw11 = (uint32_t)(lba >> 32);
    c.cdw12 = count - 1;
    return nvme_submit(&g_io, &c);
}

uint64_t fw_nvme_sectors(void) { return g_nsze; }
int      fw_nvme_present(void) { return g_ready; }

static int create_io_queues(uint32_t dstrd) {
    void *page = fw_alloc_pages_aligned(2, 4096);
    if (!page) return -1;
    fw_memset(page, 0, 8192);
    g_io.sq = (nvme_sqe *)page;
    g_io.cq = (nvme_cqe *)((uint8_t *)page + 4096);
    g_io.sq_tail = 0; g_io.cq_head = 0; g_io.phase = 1;
    g_io.sq_db = (volatile uint32_t *)(g_bar + 0x1000 + (2 * 1) * (4u << dstrd));
    g_io.cq_db = (volatile uint32_t *)(g_bar + 0x1000 + (2 * 1 + 1) * (4u << dstrd));

    nvme_sqe c;
    fw_memset(&c, 0, sizeof(c));
    c.cdw0  = 0x05;                          /* Create I/O CQ */
    c.prp1  = (uint64_t)(uintptr_t)g_io.cq;
    c.cdw10 = (QDEPTH - 1) << 16 | 1;        /* qsize | qid   */
    c.cdw11 = 1;                             /* physically contiguous */
    if (nvme_submit(&g_admin, &c) != 0) return -1;

    fw_memset(&c, 0, sizeof(c));
    c.cdw0  = 0x01;                          /* Create I/O SQ */
    c.prp1  = (uint64_t)(uintptr_t)g_io.sq;
    c.cdw10 = (QDEPTH - 1) << 16 | 1;
    c.cdw11 = (1 << 16) | 1;                 /* cqid | contiguous */
    if (nvme_submit(&g_admin, &c) != 0) return -1;
    return 0;
}

static int identify_ns(void) {
    static uint8_t idbuf[4096] __attribute__((aligned(4096)));
    nvme_sqe c;
    fw_memset(&c, 0, sizeof(c));
    c.cdw0  = 0x06;                          /* Identify */
    c.nsid  = 1;
    c.prp1  = (uint64_t)(uintptr_t)idbuf;
    c.cdw10 = 0;                             /* CNS 0: namespace */
    if (nvme_submit(&g_admin, &c) != 0) return -1;

    fw_memcpy(&g_nsze, idbuf, 8);
    uint8_t flbas = idbuf[26] & 0xF;
    uint8_t lbads = idbuf[128 + flbas * 4 + 2];
    if (lbads >= 9 && lbads <= 16) g_lba_shift = lbads;
    return 0;
}

static int nvme_blk_read(void *ctx, uint64_t lba, uint32_t n, void *buf) {
    (void)ctx; return fw_nvme_read(lba, n, buf);
}
static int nvme_blk_write(void *ctx, uint64_t lba, uint32_t n, const void *buf) {
    (void)ctx; return fw_nvme_write(lba, n, buf);
}

int fw_nvme_init(fw_pci_dev *d) {
    if (!d || !d->bar[0].addr) return -1;
    g_bar = (volatile uint8_t *)(uintptr_t)d->bar[0].addr;

    uint64_t cap = *(volatile uint64_t *)(g_bar + NVME_REG_CAP);
    uint32_t dstrd = (uint32_t)((cap >> 32) & 0xF);
    uint32_t mpsmin = (uint32_t)((cap >> 48) & 0xF);
    if (mpsmin > 0) return -1;               /* we only map 4KB pages */

    /* Disable, wait for CSTS.RDY=0, then program the admin queues. */
    wreg32(NVME_REG_CC, 0);
    for (int i = 0; i < 500000; i++) {
        if (!(reg32(NVME_REG_CSTS) & 1)) break;
        fw_stall_us(10);
    }

    void *page = fw_alloc_pages_aligned(2, 4096);
    if (!page) return -1;
    fw_memset(page, 0, 8192);
    g_admin.sq = (nvme_sqe *)page;
    g_admin.cq = (nvme_cqe *)((uint8_t *)page + 4096);
    g_admin.sq_tail = 0; g_admin.cq_head = 0; g_admin.phase = 1;
    g_admin.sq_db = (volatile uint32_t *)(g_bar + 0x1000);
    g_admin.cq_db = (volatile uint32_t *)(g_bar + 0x1000 + (4u << dstrd));

    wreg32(NVME_REG_AQA, ((QDEPTH - 1) << 16) | (QDEPTH - 1));
    wreg64(NVME_REG_ASQ, (uint64_t)(uintptr_t)g_admin.sq);
    wreg64(NVME_REG_ACQ, (uint64_t)(uintptr_t)g_admin.cq);

    /* CC: EN | IOSQES=6 (64B) | IOCQES=4 (16B) */
    wreg32(NVME_REG_CC, 1 | (6 << 16) | (4 << 20));
    for (int i = 0; i < 500000; i++) {
        if (reg32(NVME_REG_CSTS) & 1) break;
        if (reg32(NVME_REG_CSTS) & 2) return -1;      /* CFS: fatal */
        fw_stall_us(10);
    }
    if (!(reg32(NVME_REG_CSTS) & 1)) return -1;

    if (create_io_queues(dstrd) != 0) return -1;
    g_ready = 1;
    if (identify_ns() != 0) { g_ready = 0; return -1; }

    fw_printf("[nvme] namespace 1: %lu blocks of %u bytes\n",
              g_nsze, 1u << g_lba_shift);
    fw_block_register("nvme", NULL, g_nsze, 1u << g_lba_shift,
                      nvme_blk_read, nvme_blk_write);
    return 0;
}
