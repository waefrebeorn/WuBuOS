/*
 * vsl_syscall_cpm_test.c  --  CP/M BDOS personality regression test.
 *
 * Builds FCBs + a DMA buffer in process memory and drives BDOS functions
 * through vsl_syscall_dispatch() (the same router real CP/M binaries hit).
 * Asserts REAL host effects: create -> write -> read round-trip, delete,
 * search-first. No stubs.
 */

#include "vsl/vsl_syscall_cpm_internal.h"
#include "vsl/vsl_syscall_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static int g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) printf("  [PASS] %s\n", msg); \
    else { printf("  [FAIL] %s\n", msg); g_fail++; } \
} while (0)

/* Encode a CP/M BDOS call number for vsl_syscall_dispatch:
 *   class 0xC0 in the top byte, BDOS fn in bits 15..8. */
static uint64_t cpm_num(uint8_t fn) {
    return ((uint64_t)0xC0 << 24) | ((uint64_t)fn << 8);
}

/* Build a CP/M 2.2 FCB (36 bytes) for "TEST    TXT" (8.3, space padded). */
static void make_fcb(uint8_t *fcb, const char *name11) {
    memset(fcb, 0, 36);
    memcpy(fcb + 1, name11, 11);
}

int main(void) {
    /* Use a temp CP/M disk root so we don't touch the real cwd. */
    char root[256];
    snprintf(root, sizeof(root), "/tmp/wubu_cpm_test_%d", (int)getpid());
    setenv("WUBU_CPM_ROOT", root, 1);
    mkdir(root, 0755);
    mkdir(root, 0755); /* A: */

    uint64_t regs[6] = {0};

    /* --- 25: select disk A (drive 0) --- */
    regs[0] = 0; /* drive A */
    int64_t r = vsl_syscall_dispatch(cpm_num(14), regs);
    CHECK(r == 0, "BDOS 14 select disk A");

    /* --- 26: set DMA to our buffer --- */
    static uint8_t dma[128];
    regs[0] = (uint64_t)(uintptr_t)dma;
    regs[1] = sizeof(dma);
    r = vsl_syscall_dispatch(cpm_num(CPM_BDOS_SETDMA), regs);
    CHECK(r == 0, "BDOS 26 set DMA");

    /* --- 22: create file TEST.TXT --- */
    uint8_t fcb[36];
    make_fcb(fcb, "TEST    TXT");
    regs[0] = (uint64_t)(uintptr_t)fcb;
    r = vsl_syscall_dispatch(cpm_num(CPM_BDOS_MAKE_FILE), regs);
    CHECK(r == 0, "BDOS 22 create TEST.TXT");

    /* --- 21: write sequential (record 0). Set FCB cr=0. --- */
    const char *msg = "HELLO CPM";
    memset(dma, 0, 128);
    memcpy(dma, msg, strlen(msg));
    fcb[0x20] = 0; /* current record = 0 */
    regs[0] = (uint64_t)(uintptr_t)fcb;
    r = vsl_syscall_dispatch(cpm_num(CPM_BDOS_WRITE_SEQ), regs);
    CHECK(r == 0, "BDOS 21 write sequential record 0");

    /* --- 20: read sequential back; should match what we wrote --- */
    memset(dma, 0xCC, 128);
    fcb[0x20] = 0;
    regs[0] = (uint64_t)(uintptr_t)fcb;
    r = vsl_syscall_dispatch(cpm_num(CPM_BDOS_READ_SEQ), regs);
    CHECK(r == 0, "BDOS 20 read sequential record 0");
    CHECK(memcmp(dma, msg, strlen(msg)) == 0, "read-back matches written bytes");

    /* --- 35: compute random file size -> record count in FCB r0/r1/r2 --- */
    regs[0] = (uint64_t)(uintptr_t)fcb;
    r = vsl_syscall_dispatch(cpm_num(CPM_BDOS_RND_SIZE), regs);
    CHECK(r == 0, "BDOS 35 random size");
    int recs = fcb[0x21] | (fcb[0x22] << 8) | (fcb[0x23] << 16);
    CHECK(recs == 1, "file size = 1 record (128 bytes)");

    /* --- 33: random read record 0 via FCB extent --- */
    memset(dma, 0xCC, 128);
    fcb[0x21] = 0; fcb[0x22] = 0; fcb[0x23] = 0;
    regs[0] = (uint64_t)(uintptr_t)fcb;
    r = vsl_syscall_dispatch(cpm_num(CPM_BDOS_RND_READ), regs);
    CHECK(r == 0, "BDOS 33 random read record 0");
    CHECK(memcmp(dma, msg, strlen(msg)) == 0, "random read matches");

    /* --- 19: delete TEST.TXT --- */
    regs[0] = (uint64_t)(uintptr_t)fcb;
    r = vsl_syscall_dispatch(cpm_num(CPM_BDOS_DELETE), regs);
    CHECK(r == 0, "BDOS 19 delete TEST.TXT");

    /* --- 17: search first for TEST.TXT should now NOT find it --- */
    uint8_t sfcb[36];
    make_fcb(sfcb, "TEST    TXT");
    regs[0] = (uint64_t)(uintptr_t)sfcb;
    r = vsl_syscall_dispatch(cpm_num(CPM_BDOS_SFIRST), regs);
    CHECK(r == CPM_ERR, "BDOS 17 search-first finds nothing after delete");

    /* --- 17: search first with wildcard '*.*' should find the disk root exists
     * (empty dir still enumerates nothing, but a created file would appear) --- */
    uint8_t wfcb[36];
    make_fcb(wfcb, "***********");
    regs[0] = (uint64_t)(uintptr_t)wfcb;
    r = vsl_syscall_dispatch(cpm_num(CPM_BDOS_SFIRST), regs);
    CHECK(r == CPM_ERR, "BDOS 17 wildcard search empty dir -> not found");

    /* --- 2: console output (write 'X') should not crash and returns 0 --- */
    regs[0] = 'X';
    r = vsl_syscall_dispatch(cpm_num(CPM_BDOS_CONOUT), regs);
    CHECK(r == 0, "BDOS 2 console output");

    /* --- unimplemented fn -> ENOSYS (negative) --- */
    r = vsl_syscall_dispatch(cpm_num(40), regs); /* 40 not in table */
    CHECK(r < 0, "BDOS 40 unimplemented -> error");

    printf("\n%s CP/M BDOS personality tests\n",
           g_fail == 0 ? "ALL PASSED" : "SOME FAILED");
    return g_fail == 0 ? 0 : 1;
}
