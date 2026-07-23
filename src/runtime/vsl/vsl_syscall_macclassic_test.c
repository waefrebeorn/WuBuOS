/*
 * vsl_syscall_macclassic_test.c  --  Classic Mac OS (68K A-line) personality test.
 *
 * Drives trap words through vsl_syscall_dispatch() (the SAME router a real
 * Classic Mac binary hits, once the 68K emulator decodes an A-line trap).
 * Asserts REAL host effects: NewPtr/DisposePtr, NewHandle/DisposeHandle,
 * Open/Write/Read file round-trip, Create/Delete, TickCount monotonic,
 * DrawChar/WriteChar/WriteLn. No stubs.
 */

#include "vsl/vsl_syscall_macclassic_internal.h"
#include "vsl/vsl_syscall_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>

static int g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) printf("  [PASS] %s\n", msg); \
    else { printf("  [FAIL] %s\n", msg); g_fail++; } \
} while (0)

/* Encode a Classic Mac A-line trap for vsl_syscall_dispatch:
 *   class 0xB0 in the top byte, trap number in the low 12 bits. */
static uint64_t macc_num(uint32_t trap) {
    return ((uint64_t)0xB0 << 24) | ((uint64_t)(trap & 0xFFF));
}

int main(void) {
    uint64_t regs[6] = {0};

    /* --- NewPtr / GetPtrSize / DisposePtr --- */
    regs[0] = 64;
    int64_t p = vsl_syscall_dispatch(macc_num(MACC_NEWPTR), regs);
    CHECK(p != 0 && p != -1, "NewPtr(64) -> non-null pointer");

    regs[0] = (uint64_t)p;
    int64_t sz = vsl_syscall_dispatch(macc_num(MACC_GETPTRSIZE), regs);
    CHECK(sz >= 64, "GetPtrSize >= requested bytes");

    regs[0] = (uint64_t)p;
    int64_t dr = vsl_syscall_dispatch(macc_num(MACC_DISPOSEPTR), regs);
    CHECK(dr == 0, "DisposePtr -> 0");

    /* --- NewHandle / DisposeHandle --- */
    regs[0] = 32;
    int64_t h = vsl_syscall_dispatch(macc_num(MACC_NEWHANDLE), regs);
    CHECK(h != 0 && h != -1, "NewHandle(32) -> non-null handle");

    regs[0] = (uint64_t)h;
    int64_t dh = vsl_syscall_dispatch(macc_num(MACC_DISPOSEHANDLE), regs);
    CHECK(dh == 0, "DisposeHandle -> 0");

    /* --- File round-trip via Open/Write/Read/Close --- */
    char path[256];
    snprintf(path, sizeof(path), "/tmp/wubu_macc_test_%d.bin", (int)getpid());
    unlink(path);
    regs[0] = (uint64_t)(uintptr_t)path;
    int64_t ref = vsl_syscall_dispatch(macc_num(MACC_OPEN), regs);
    CHECK(ref > 0, "Open -> refnum > 0");
    int refnum = (int)(ref & 0xFFFF);

    const char *msg = "HELLO MAC";
    uint8_t wbuf[32]; memcpy(wbuf, msg, strlen(msg));
    regs[0] = (uint64_t)refnum;
    regs[1] = (uint64_t)(uintptr_t)wbuf;
    regs[2] = strlen(msg);
    int64_t nw = vsl_syscall_dispatch(macc_num(MACC_WRITE), regs);
    CHECK(nw == (int64_t)strlen(msg), "Write -> byte count");

    regs[0] = (uint64_t)refnum;
    int64_t cl = vsl_syscall_dispatch(macc_num(MACC_CLOSE), regs);
    CHECK(cl == 0, "Close -> 0");

    /* reopen and read back */
    regs[0] = (uint64_t)(uintptr_t)path;
    ref = vsl_syscall_dispatch(macc_num(MACC_OPEN), regs);
    refnum = (int)(ref & 0xFFFF);
    uint8_t rbuf[32]; memset(rbuf, 0, sizeof(rbuf));
    regs[0] = (uint64_t)refnum;
    regs[1] = (uint64_t)(uintptr_t)rbuf;
    regs[2] = sizeof(rbuf);
    int64_t nr = vsl_syscall_dispatch(macc_num(MACC_READ), regs);
    CHECK(nr == (int64_t)strlen(msg), "Read -> byte count");
    CHECK(memcmp(rbuf, msg, strlen(msg)) == 0, "read-back matches written bytes");
    regs[0] = (uint64_t)refnum;
    vsl_syscall_dispatch(macc_num(MACC_CLOSE), regs);

    /* --- Create + Delete --- */
    char cpath[256];
    snprintf(cpath, sizeof(cpath), "/tmp/wubu_macc_create_%d", (int)getpid());
    unlink(cpath);
    regs[0] = (uint64_t)(uintptr_t)cpath;
    int64_t cr = vsl_syscall_dispatch(macc_num(MACC_CREATE), regs);
    CHECK(cr == 0, "Create -> 0");
    CHECK(access(cpath, F_OK) == 0, "created file exists on host fs");
    regs[0] = (uint64_t)(uintptr_t)cpath;
    int64_t del = vsl_syscall_dispatch(macc_num(MACC_DELETE), regs);
    CHECK(del == 0, "Delete -> 0");
    CHECK(access(cpath, F_OK) != 0, "deleted file gone on host fs");

    /* --- TickCount monotonic --- */
    int64_t t1 = vsl_syscall_dispatch(macc_num(MACC_TICKCOUNT), regs);
    struct timespec ts = {0, 2000000}; /* 2 ms */
    nanosleep(&ts, NULL);
    int64_t t2 = vsl_syscall_dispatch(macc_num(MACC_TICKCOUNT), regs);
    CHECK(t2 >= t1, "TickCount monotonic (t2 >= t1)");

    /* --- DrawChar / WriteChar / WriteLn (headless -> stdout) --- */
    regs[0] = 'Z';
    int64_t dc = vsl_syscall_dispatch(macc_num(MACC_DRAWCHAR), regs);
    CHECK(dc == 0, "DrawChar('Z') -> 0");
    regs[0] = 'Q';
    CHECK(vsl_syscall_dispatch(macc_num(MACC_WRITECHAR), regs) == 0, "WriteChar('Q') -> 0");
    CHECK(vsl_syscall_dispatch(macc_num(MACC_WRITE0), regs) == 0, "WriteLn -> 0");

    /* --- unimplemented / out-of-range traps -> paramErr (-1) --- */
    int64_t ur = vsl_syscall_dispatch(macc_num(0xFFF), regs);
    CHECK(ur == -1, "unimplemented trap 0xFFF -> paramErr");
    int64_t ur2 = vsl_syscall_dispatch(macc_num(0xBC0), regs); /* > MACC_TRAP_MAX */
    CHECK(ur2 == -1, "out-of-range trap 0xBC0 -> paramErr");

    printf("\n%s Classic Mac OS (68K A-line) personality tests\n",
           g_fail == 0 ? "ALL PASSED" : "SOME FAILED");
    return g_fail == 0 ? 0 : 1;
}
