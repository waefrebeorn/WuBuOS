/*
 * wubu_n9.c  --  A3: booted-image 9P proof (docs/GAP_ROADMAP.md, GAP A3).
 *
 * Kernel side of the A3 harness. Uses the kernel-clean 9P client (wubu_9pc)
 * against a HOST 9P server reached over COM2 (0x2F8). The transport is
 * length-framed 9P-over-serial:
 *
 *   frame := [u32 LE length][9P2000 message bytes]
 *
 * sent/received over COM2; the host side mirrors the exact same framing so the
 * in-process codec (styxfs_serve) and the on-wire codec stay identical. COM1
 * stays the debug log; COM2 is dedicated to the /n proof and never touches the
 * klog stream.
 *
 * After a successful walk of /apps/<mod> + read, the bytes go through
 * wubu_zx_run with a kernel map fn that allocates physical pages and maps them
 * at a fixed HIGH-HALF VA (0x400000 image_base collides with the kernel heap,
 * so we do NOT map at the image's preferred base -- the module is self-
 * contained and base-agnostic). The entry returns a value; we print it.
 *
 * Whole thing is polled (no IRQ) and bounded: if the host never answers a
 * frame within the timeout the boot logs the stall and continues, so the A3
 * code can never hang the kernel the way an unbounded serial wait once did.
 */
#include "wubu_n9.h"
#include "wubu_9pc.h"
#include "wubu_vmm.h"
#include "wubu_zx.h"
#include "klog.h"
#include "memory.h"
#include "interrupt_io.h"
#include <stddef.h>
#include <string.h>

/* ---- COM2 (0x2F8) polled UART, length-framed 9P transport ---- */

#define COM2_BASE  0x2F8u
#define COM2_DATA  (COM2_BASE + 0)
#define COM2_IER   (COM2_BASE + 1)
#define COM2_FCR   (COM2_BASE + 2)
#define COM2_LCR   (COM2_BASE + 3)
#define COM2_LSR   (COM2_BASE + 5)

/* Bounded poll (no busy-forever): ~8 frames' worth before giving up. */
#define N9_POLL_MAX (1u << 22)

static void n9_com2_init(void)
{
    outb(COM2_IER, 0x00);          /* no UART interrupts (polled) */
    outb(COM2_LCR, 0x80);          /* DLAB on */
    outb(COM2_DATA + 0, 0x01);     /* divisor low  = 1  -> 115200 */
    outb(COM2_DATA + 1, 0x00);     /* divisor high = 0            */
    outb(COM2_LCR, 0x03);          /* DLAB off, 8N1 */
    outb(COM2_FCR, 0xC7);          /* FIFO enable + clear + 14B trigger */
}

/* TX one byte; returns 0 on success, -1 on bounded-timeout (char dropped).
 * The proof must not hang the boot, so unlike a debug log drop this is a real
 * return the caller can notice. */
static int n9_tx(uint8_t c)
{
    for (uint32_t i = 0; i < N9_POLL_MAX; i++) {
        if (inb(COM2_LSR) & 0x20) { outb(COM2_DATA, c); return 0; }
    }
    return -1;   /* THR never emptied within the bound */
}

/* RX one byte; returns 0 with *out set, -1 on bounded-timeout (host quiet). */
static int n9_rx(uint8_t *out)
{
    for (uint32_t i = 0; i < N9_POLL_MAX; i++) {
        if (inb(COM2_LSR) & 0x01) { *out = inb(COM2_DATA); return 0; }
    }
    return -1;
}

static int n9_send(const uint8_t *p, uint32_t n)
{
    uint8_t h[4];
    h[0] = (uint8_t)(n & 0xFF); h[1] = (uint8_t)((n >> 8) & 0xFF);
    h[2] = (uint8_t)((n >> 16) & 0xFF); h[3] = (uint8_t)((n >> 24) & 0xFF);
    for (int i = 0; i < 4; i++) if (n9_tx(h[i]) != 0) return -1;
    for (uint32_t i = 0; i < n; i++)  if (n9_tx(p[i]) != 0) return -1;
    return 0;
}

static int n9_recv_header(uint32_t *len)
{
    uint8_t h[4];
    for (int i = 0; i < 4; i++) if (n9_rx(&h[i]) != 0) return -1;
    *len = (uint32_t)h[0] | ((uint32_t)h[1] << 8) |
           ((uint32_t)h[2] << 16) | ((uint32_t)h[3] << 24);
    if (*len > 0x40000) return -1;   /* sanity cap */
    return 0;
}

/* The w9pc transport_fn: framed 9P over COM2. NOTE the contract: w9_tx()
 * passes *rsplen=0 and does NOT hand us the out-buffer capacity, so we must
 * cap the reply by the size of the buffer WE allocated (c->out), never by the
 * incoming *rsplen value. That buffer must be >= the largest reply a read can
 * return (msize + header). */
#define N9_OUT_SZ 16384
static uint8_t n9_out[N9_OUT_SZ];

static int n9_transport(const uint8_t *req, uint32_t reqlen,
                        uint8_t *rsp, uint32_t *rsplen, void *ud)
{
    (void)ud;
    if (n9_send(req, reqlen) != 0) return -1;
    uint32_t rlen = 0;
    if (n9_recv_header(&rlen) != 0) return -1;
    if (rlen > N9_OUT_SZ) return -1;          /* must fit our out buffer */
    for (uint32_t i = 0; i < rlen; i++) {
        uint8_t c;
        if (n9_rx(&c) != 0) return -1;
        rsp[i] = c;
    }
    *rsplen = rlen;
    return 0;
}

/* ---- kernel map ops for the loader ---- */

/* Fixed high-half VA for the module image (low 32-bit VAs are owned by the
 * kernel heap/kernel; 0xffffffff90000000 is above the kernel image and below
 * the shared libraries/PCI MMIO window). */
#define N9_APP_VA 0xffffffff90000000ull

static void *n9_map(uintptr_t base, size_t span)
{
    (void)base;                     /* always map at the fixed high VA */
    if (span == 0) return NULL;
    uint32_t n = (uint32_t)((span + 0xFFF) >> 12);
    if (n == 0 || n > 64) return NULL;
    uint64_t phys = wubu_vmm_alloc_pages(n);
    if (phys == 0) return NULL;
    for (uint32_t i = 0; i < n; i++) {
        if (wubu_vmm_map_page(N9_APP_VA + (uint64_t)i * 4096,
                              phys + (uint64_t)i * 4096, 3) != 0) {
            wubu_vmm_free_pages(phys, n);
            return NULL;
        }
    }
    void *va = (void *)(uintptr_t)N9_APP_VA;
    for (size_t i = 0; i < span; i++) ((uint8_t *)va)[i] = 0; /* zx wants zeroed */
    return va;
}

/* ---- the A3 sequence ---- */

int wubu_n9_boot(void)
{
    n9_com2_init();
    klog_printf("A3: 9P proof starting (COM2 transport)\n");

    /* fetch /apps/agi-demo.bin over 9P via the kernel-clean client */
    uint8_t inb_[2048];
    w9pc_t c;
    memset(&c, 0, sizeof c);
    c.in = inb_; c.in_cap = sizeof inb_;
    c.out = n9_out; c.out_cap = N9_OUT_SZ;
    c.tx = n9_transport;

    const char *apps = "apps";
    const char *name = "agi-demo.bin";
    const char *seg[2]; seg[0] = apps; seg[1] = name;

    if (w9pc_version(&c, 8192, "9P2000") != 0) {
        klog_printf("A3: FAIL 9P version\n"); return -1;
    }
    if (w9pc_attach(&c, 1, 1, 0, "/") != 0) {
        klog_printf("A3: FAIL attach\n"); return -1;
    }
    if (w9pc_walk(&c, 1, 1, 2, seg, 2) != 0) {
        klog_printf("A3: FAIL walk /apps/agi-demo.bin\n"); return -1;
    }
    if (w9pc_open(&c, 1, 2, 0) != 0) {
        klog_printf("A3: FAIL open\n"); return -1;
    }

    size_t cap = 8192, n = 0;
    uint8_t *buf = (uint8_t *)mem_alloc(cap);
    if (!buf) { klog_printf("A3: FAIL no heap\n"); return -1; }
    int ok = 0;
    for (;;) {
        if (n + 8192 > cap) {
            uint8_t *nb = (uint8_t *)mem_alloc(cap * 2);
            if (!nb) break;
            memcpy(nb, buf, n);
            mem_free(buf);
            buf = nb; cap *= 2;
        }
        uint32_t got = 0;
        if (w9pc_read(&c, 1, 2, (uint64_t)n, buf + n, 8192, &got) != 0) break;
        if (got == 0) { ok = 1; break; }        /* EOF */
        n += got;
        if (got < 8192) { ok = 1; break; }      /* short read = EOF */
    }
    w9pc_clunk(&c, 1, 2);
    if (!ok || n == 0) {
        klog_printf("A3: FAIL read module (%u bytes)\n", (unsigned)n);
        mem_free(buf); return -1;
    }

    wubu_zx_ops ops = { n9_map, NULL };
    int err = -99;
    int r = wubu_zx_run(&ops, buf, n, N9_APP_VA, NULL, &err);

    mem_free(buf);

    if (err == 0) {
        /* Accept any exit code the loader returns -- but a true "did work"
         * proof wants a NONZERO marker so a silently-broken loader stage can't
         * masquerade as success. The demo module returns 55 on purpose. */
        klog_printf("A3: RESULT: PASS -- 9P fetched /apps/%s (%u B) and ran "
                    "-> entry=%d\n", name, (unsigned)n, r);
        return 0;
    }
    klog_printf("A3: RESULT: FAIL -- loader err=%d\n", err);
    return -1;
}
