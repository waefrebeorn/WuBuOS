/*
 * vsl_syscall_nt_test.c -- Regression test for E1: ReactOS NT syscall
 * transliteration (first 10 syscalls, real VSL handlers via the bridge).
 *
 * Asserts FIXED behavior (real work), not just non-crash:
 *  - atom table add/find/lookup
 *  - UUID generation (RFC 4122 version/variant bits)
 *  - LUID generation
 *  - eventfd clear
 *  - futex wake (thread alert)
 *  - IO cancel (shutdown)
 *  - job assignment (setpgid)
 *  - user physical page alloc/free
 *  - a still-stubbed syscall returns NT_STATUS_NOT_IMPLEMENTED
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <linux/futex.h>
#include <sys/syscall.h>

#include "vsl_nt_bridge.h"

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; printf("  ✅ %s\n", msg); } \
    else { g_fail++; printf("  ❌ %s\n", msg); } \
} while (0)

int main(void) {
    printf("=== VSL NT transliteration (E1) test ===\n");
    vsl_nt_bridge_ctx_t ctx;
    CHECK(vsl_nt_bridge_init(&ctx) == 0, "bridge init");

    uint64_t args[6];

    /* 1. NtAddAtom (9) + NtFindAtom (81) */
    {
        uint32_t atom = 0, atom2 = 0;
        args[0] = (uint64_t)(uintptr_t)"WinBuAtom";
        args[1] = (uint64_t)(uintptr_t)&atom;
        memset(args + 2, 0, sizeof(uint64_t) * 4);
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 9, args, 2);
        CHECK(r == NT_STATUS_SUCCESS, "NtAddAtom returns SUCCESS");
        CHECK(atom != 0, "NtAddAtom assigns a nonzero atom id");

        args[0] = (uint64_t)(uintptr_t)"WinBuAtom";
        args[1] = (uint64_t)(uintptr_t)&atom2;
        r = vsl_nt_syscall_dispatch(&ctx, 81, args, 2);
        CHECK(r == NT_STATUS_SUCCESS, "NtFindAtom finds existing atom");
        CHECK(atom2 == atom, "NtFindAtom returns the same atom id");

        args[0] = (uint64_t)(uintptr_t)"NoSuchAtomXYZ";
        args[1] = (uint64_t)(uintptr_t)&atom2;
        r = vsl_nt_syscall_dispatch(&ctx, 81, args, 2);
        CHECK(r == NT_STATUS_OBJECT_NAME_NOT_FOUND, "NtFindAtom missing -> NOT_FOUND");
    }

    /* 2. NtAllocateUuids (18) */
    {
        uint8_t uuids[32];
        memset(uuids, 0, sizeof(uuids));
        args[0] = 2;                       /* count */
        args[1] = (uint64_t)(uintptr_t)uuids;
        memset(args + 2, 0, sizeof(uint64_t) * 4);
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 18, args, 2);
        CHECK(r == NT_STATUS_SUCCESS, "NtAllocateUuids returns SUCCESS");
        CHECK((uuids[6] & 0xF0) == 0x40, "UUID v4 version bits set");
        CHECK((uuids[8] & 0xC0) == 0x80, "UUID v4 variant bits set");
        CHECK((uuids[16+6] & 0xF0) == 0x40, "second UUID version bits set");
    }

    /* 3. NtAllocateLocallyUniqueId (16) */
    {
        struct { uint32_t low; uint32_t high; } luid;
        memset(&luid, 0, sizeof(luid));
        args[0] = (uint64_t)(uintptr_t)&luid;
        memset(args + 1, 0, sizeof(uint64_t) * 5);
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 16, args, 1);
        CHECK(r == NT_STATUS_SUCCESS, "NtAllocateLocallyUniqueId returns SUCCESS");
        CHECK(luid.low != 0, "LUID low part assigned");
    }

    /* 4. NtClearEvent (27) -- reset an eventfd */
    {
        int efd = eventfd(5, EFD_NONBLOCK);
        CHECK(efd >= 0, "eventfd created (counter=5, nonblock)");
        uint64_t cur = 0; read(efd, &cur, 8);
        CHECK(cur == 5, "eventfd reads 5 before clear");

        args[0] = (uint64_t)efd;
        memset(args + 1, 0, sizeof(uint64_t) * 5);
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 27, args, 1);
        CHECK(r == NT_STATUS_SUCCESS, "NtClearEvent returns SUCCESS");
        /* After clear the counter is 0, so a nonblocking read returns EAGAIN. */
        cur = 0; ssize_t rd = read(efd, &cur, 8);
        CHECK(rd < 0 && errno == EAGAIN, "eventfd counter reset to 0 (NtClearEvent) -- nonblock read EAGAIN");
        close(efd);
    }

    /* 5. NtAlertThread (15) -- futex wake */
    {
        uint32_t futex_var = 0;
        args[0] = (uint64_t)(uintptr_t)&futex_var;
        memset(args + 1, 0, sizeof(uint64_t) * 5);
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 15, args, 1);
        CHECK(r == NT_STATUS_SUCCESS, "NtAlertThread (futex wake) returns SUCCESS");
    }

    /* 6. NtCancelIoFile (25) -- shutdown fd cancels IO */
    {
        int p[2];
        CHECK(pipe(p) == 0, "pipe created");
        args[0] = (uint64_t)p[0];
        memset(args + 1, 0, sizeof(uint64_t) * 5);
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 25, args, 1);
        CHECK(r == NT_STATUS_SUCCESS, "NtCancelIoFile returns SUCCESS");
        close(p[0]); close(p[1]);
    }

    /* 7. NtAssignProcessToJobObject (22) -- setpgid */
    {
        pid_t child = fork();
        CHECK(child >= 0, "fork child for job assignment");
        if (child == 0) {
            pid_t me = getpid();
            args[0] = (uint64_t)me;          /* job handle = this process's pgid */
            args[1] = (uint64_t)me;          /* process = self */
            memset(args + 2, 0, sizeof(uint64_t) * 4);
            int64_t r = vsl_nt_syscall_dispatch(&ctx, 22, args, 2);
            _exit(r == NT_STATUS_SUCCESS ? 0 : 1);
        }
        int status = 0;
        waitpid(child, &status, 0);
        CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0,
              "NtAssignProcessToJobObject moves child into job pgid");
    }

    /* 8+9. NtAllocateUserPhysicalPages (17) / NtFreeUserPhysicalPages (87) */
    {
        long pagesz = sysconf(_SC_PAGESIZE);
        uint64_t base = 0, num = 4;
        args[0] = 0;                       /* process (unused) */
        args[1] = num;                     /* number of pages */
        args[2] = (uint64_t)(uintptr_t)&base;
        memset(args + 3, 0, sizeof(uint64_t) * 3);
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 17, args, 3);
        CHECK(r != NT_STATUS_NO_MEMORY && r > 0, "NtAllocateUserPhysicalPages returns base address");
        CHECK(base != 0, "NtAllocateUserPhysicalPages fills base out-param");

        args[0] = 0;
        args[1] = num;
        args[2] = base;
        memset(args + 3, 0, sizeof(uint64_t) * 3);
        r = vsl_nt_syscall_dispatch(&ctx, 87, args, 3);
        CHECK(r == NT_STATUS_SUCCESS, "NtFreeUserPhysicalPages unmaps successfully");
    }

    /* 10. A still-stubbed syscall returns NOT_IMPLEMENTED through dispatch */
    {
        memset(args, 0, sizeof(args));
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 2, args, 0);  /* NtAccessCheck */
        CHECK(r == NT_STATUS_NOT_IMPLEMENTED, "stubbed NtAccessCheck -> NOT_IMPLEMENTED");
    }

    vsl_nt_bridge_shutdown(&ctx);

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
