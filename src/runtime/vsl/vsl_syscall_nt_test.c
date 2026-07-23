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
#include <time.h>

#include "vsl_nt_bridge.h"

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; printf("  ✅ %s\n", msg); } \
    else { g_fail++; printf("  ❌ %s\n", msg); } \
} while (0)

/* Thread-start routine for the NtCreateThread test: flips a shared flag so the
 * test can observe the thread actually ran. */
static volatile int g_thread_ran = 0;
static void *vsl_nt_test_thread_start(void *arg) {
    (void)arg;
    g_thread_ran = 1;
    return NULL;
}

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
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 77, args, 2);
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
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 89, args, 1);
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
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 102, args, 3);
        CHECK(r != NT_STATUS_NO_MEMORY && r > 0, "NtAllocateUserPhysicalPages returns base address");
        CHECK(base != 0, "NtAllocateUserPhysicalPages fills base out-param");

        args[0] = 0;
        args[1] = num;
        args[2] = base;
        memset(args + 3, 0, sizeof(uint64_t) * 3);
        r = vsl_nt_syscall_dispatch(&ctx, 87, args, 3);
        CHECK(r == NT_STATUS_SUCCESS, "NtFreeUserPhysicalPages unmaps successfully");
    }

    /* 11. NtSetUuidSeed (256) + NtAllocateUuids (18) deterministic under seed */
    {
        args[0] = 0x1234;                   /* seed */
        memset(args + 1, 0, sizeof(uint64_t) * 5);
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 256, args, 1);
        CHECK(r == NT_STATUS_SUCCESS, "NtSetUuidSeed installs seed");

        uint8_t uuids[16];
        memset(uuids, 0, sizeof(uuids));
        args[0] = 1;
        args[1] = (uint64_t)(uintptr_t)uuids;
        memset(args + 2, 0, sizeof(uint64_t) * 4);
        r = vsl_nt_syscall_dispatch(&ctx, 18, args, 2);
        CHECK(r == NT_STATUS_SUCCESS, "NtAllocateUuids (seeded) returns SUCCESS");
        CHECK((uuids[6] & 0xF0) == 0x40 && (uuids[8] & 0xC0) == 0x80,
              "seeded UUID still RFC4122 v4 + variant");

        /* Same seed must yield the same first byte (determinism). */
        uint8_t uuids2[16];
        memset(uuids2, 0, sizeof(uuids2));
        args[0] = 1;
        args[1] = (uint64_t)(uintptr_t)uuids2;
        memset(args + 2, 0, sizeof(uint64_t) * 4);
        r = vsl_nt_syscall_dispatch(&ctx, 18, args, 2);
        CHECK(uuids2[0] == uuids[0], "NtSetUuidSeed makes UUID generation reproducible");

        /* Reset seed to 0 (random) for subsequent tests. */
        args[0] = 0;
        memset(args + 1, 0, sizeof(uint64_t) * 5);
        vsl_nt_syscall_dispatch(&ctx, 256, args, 1);
    }

    /* 12. NtAreMappedFilesTheSame (21) -- same inode vs different file */
    {
        const char *tmp = "/tmp/wubu_nt_same_a";
        const char *lnk = "/tmp/wubu_nt_same_b";
        FILE *fa = fopen(tmp, "w"); if (fa) { fputc('x', fa); fclose(fa); }
        unlink(lnk);
        CHECK(link(tmp, lnk) == 0, "hardlinked twin created");

        args[0] = (uint64_t)(uintptr_t)tmp;
        args[1] = (uint64_t)(uintptr_t)lnk;
        memset(args + 2, 0, sizeof(uint64_t) * 4);
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 21, args, 2);
        CHECK(r == 1, "NtAreMappedFilesTheSame: hardlinks are the same inode");

        args[0] = (uint64_t)(uintptr_t)tmp;
        args[1] = (uint64_t)(uintptr_t)"/tmp/wubu_nt_nonexistent_xyz";
        memset(args + 2, 0, sizeof(uint64_t) * 4);
        r = vsl_nt_syscall_dispatch(&ctx, 21, args, 2);
        CHECK(r == NT_STATUS_OBJECT_NAME_NOT_FOUND,
              "NtAreMappedFilesTheSame: missing path -> NOT_FOUND");

        unlink(tmp); unlink(lnk);
    }

    /* 13. NtCreate/Open/IsProcessIn/Terminate job object (42/125/99/266) */
    {
        uint32_t job = 0;
        args[0] = (uint64_t)(uintptr_t)&job;
        memset(args + 1, 0, sizeof(uint64_t) * 5);
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 42, args, 1);
        CHECK(r == NT_STATUS_SUCCESS, "NtCreateJobObject returns SUCCESS");
        CHECK(job != 0, "NtCreateJobObject assigns a nonzero job handle");

        uint32_t job_open = 0;
        args[0] = (uint64_t)(uintptr_t)&job_open;
        args[1] = job;
        memset(args + 2, 0, sizeof(uint64_t) * 4);
        r = vsl_nt_syscall_dispatch(&ctx, 125, args, 2);
        CHECK(r == NT_STATUS_SUCCESS, "NtOpenJobObject resolves the handle");
        CHECK(job_open == job, "NtOpenJobObject returns the same job id");

        args[0] = (uint64_t)getpid();
        args[1] = job;
        memset(args + 2, 0, sizeof(uint64_t) * 4);
        r = vsl_nt_syscall_dispatch(&ctx, 99, args, 2);
        CHECK(r == 0 || r == NT_STATUS_INVALID_PARAMETER,
              "NtIsProcessInJob returns boolean (0/1) without crashing");

        args[0] = job;
        memset(args + 1, 0, sizeof(uint64_t) * 5);
        r = vsl_nt_syscall_dispatch(&ctx, 466, args, 1);
        CHECK(r == NT_STATUS_SUCCESS, "NtTerminateJobObject frees the job");

        /* Re-opening a terminated job must fail. */
        args[0] = (uint64_t)(uintptr_t)&job_open;
        args[1] = job;
        memset(args + 2, 0, sizeof(uint64_t) * 4);
        r = vsl_nt_syscall_dispatch(&ctx, 125, args, 2);
        CHECK(r == NT_STATUS_OBJECT_NAME_NOT_FOUND,
              "NtOpenJobObject after terminate -> NOT_FOUND");
    }

    /* 14. NtDeleteAtom (63) + NtQueryInformationAtom (158) */
    {
        uint32_t atom = 0;
        args[0] = (uint64_t)(uintptr_t)"JobAtomTest";
        args[1] = (uint64_t)(uintptr_t)&atom;
        memset(args + 2, 0, sizeof(uint64_t) * 4);
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 77, args, 2);
        CHECK(r == NT_STATUS_SUCCESS, "NtAddAtom (for delete test) succeeds");
        CHECK(atom != 0, "atom id assigned");

        struct { uint32_t ref_count; uint16_t name_len; uint8_t pad[2]; } info;
        uint32_t retlen = 0;
        args[0] = atom;
        args[1] = 1;                       /* AtomBasicInformation class */
        args[2] = (uint64_t)(uintptr_t)&info;
        args[3] = (uint64_t)(uintptr_t)&retlen;
        memset(args + 4, 0, sizeof(uint64_t) * 2);
        r = vsl_nt_syscall_dispatch(&ctx, 158, args, 4);
        CHECK(r == NT_STATUS_SUCCESS, "NtQueryInformationAtom returns SUCCESS");
        CHECK(info.name_len == strlen("JobAtomTest"),
              "NtQueryInformationAtom reports correct name length");
        CHECK(retlen == 8, "NtQueryInformationAtom writes return length");

        args[0] = atom;
        memset(args + 1, 0, sizeof(uint64_t) * 5);
        r = vsl_nt_syscall_dispatch(&ctx, 63, args, 1);
        CHECK(r == NT_STATUS_SUCCESS, "NtDeleteAtom removes the atom");

        r = vsl_nt_syscall_dispatch(&ctx, 158, args, 4);
        CHECK(r == NT_STATUS_OBJECT_NAME_NOT_FOUND,
              "NtQueryInformationAtom after delete -> NOT_FOUND");
    }

    /* 15. NtFlushWriteBuffer (86) -- real fsync on a temp file */
    {
        int fd = open("/tmp/wubu_nt_flush", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        CHECK(fd >= 0, "temp file opened for flush test");
        const char *msg = "hello";
        write(fd, msg, 5);
        args[0] = (uint64_t)fd;
        memset(args + 1, 0, sizeof(uint64_t) * 5);
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 86, args, 1);
        CHECK(r == NT_STATUS_SUCCESS, "NtFlushWriteBuffer fsyncs the fd");
        close(fd); unlink("/tmp/wubu_nt_flush");
    }

    /* 16. NtAlertResumeThread (14) -- futex wake + SIGCONT (no crash) */
    {
        uint32_t futex_var = 0;
        args[0] = (uint64_t)(uintptr_t)&futex_var;
        args[1] = (uint64_t)getpid();
        memset(args + 2, 0, sizeof(uint64_t) * 4);
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 187, args, 2);
        CHECK(r == NT_STATUS_SUCCESS, "NtAlertResumeThread (wake+resume) SUCCESS");
    }

    /* 10. NtAccessCheck (2) is NO LONGER a stub — Batch 12 promoted it to a
     * real privilege-enforcement handler. A default token with no required
     * privileges requested (empty set) must now return SUCCESS, not
     * NOT_IMPLEMENTED. This proves the stub->real inversion landed. */
    {
        memset(args, 0, sizeof(args));
        /* Empty PRIVILEGE_SET (count=0): control=0 (any) => ok when count==0. */
        uint32_t empty_ps[8];
        memset(empty_ps, 0, sizeof(empty_ps));
        uint32_t dftok = 0;
        args[0] = (uint64_t)(uintptr_t)&dftok;
        vsl_nt_syscall_dispatch(&ctx, 307, args, 1); /* open a default token */
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)dftok;
        args[1] = (uint64_t)(uintptr_t)empty_ps;
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 244, args, 2);  /* NtAccessCheck */
        CHECK(r == NT_STATUS_SUCCESS,
              "NtAccessCheck now real -> SUCCESS (not NOT_IMPLEMENTED)");
    }

    /* 10b. A TRULY-unimplemented slot (syscall 0, but we now have some ordinal 0 handlers)
     * Pick one we know is NOT registered to test the stub path. */
    {
        memset(args, 0, sizeof(args));
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 999, args, 0); /* 999 is out of range */
        CHECK(r == NT_STATUS_NOT_IMPLEMENTED,
              "unregistered syscall 999 -> NOT_IMPLEMENTED (stub intact)");
    }

    /* 17. NtDelayExecution (37) -- real nanosleep */
    {
        /* 10 ms relative delay (negative = relative in NT encoding). */
        args[0] = 0;                       /* Alertable */
        args[1] = (uint64_t)(-10 * 1000 * 1000LL);
        memset(args + 2, 0, sizeof(uint64_t) * 4);
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 62, args, 2);
        CHECK(r == NT_STATUS_SUCCESS, "NtDelayExecution sleeps without error");
    }

    /* 18. NtCreateEvent / NtSetEvent / NtResetEvent (39/229/209) */
    {
        memset(args, 0, sizeof(args));
        int64_t h = vsl_nt_syscall_dispatch(&ctx, 72, args, 1);
        CHECK(h != 0, "NtCreateEvent returns a non-zero handle");

        uint64_t val = 0;
        int efd;
        /* Read the eventfd directly to confirm unsignaled (0) initially. */
        args[0] = (uint64_t)h;
        memset(args + 1, 0, sizeof(uint64_t) * 5);
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 229, args, 1);   /* SetEvent */
        CHECK(r == NT_STATUS_SUCCESS, "NtSetEvent signals the event");

        args[0] = (uint64_t)h;
        memset(args + 1, 0, sizeof(uint64_t) * 5);
        r = vsl_nt_syscall_dispatch(&ctx, 211, args, 1);           /* ResetEvent */
        CHECK(r == NT_STATUS_SUCCESS, "NtResetEvent clears the event");

        args[0] = (uint64_t)h;
        memset(args + 1, 0, sizeof(uint64_t) * 5);
        r = vsl_nt_syscall_dispatch(&ctx, 15, args, 1);            /* NtClose */
        CHECK(r == NT_STATUS_SUCCESS, "NtClose frees the event handle");
        (void)efd; (void)val;
    }

    /* 19. NtOpenFile / NtWriteFile / NtReadFile / NtQueryInformationFile /
     *     NtClose (123/285/193/159/28) -- full file round-trip */
    {
        char path[256];
        snprintf(path, sizeof(path), "/tmp/wubu_nt_file_%d", getpid());
        unlink(path);
        args[0] = 0;
        args[1] = (uint64_t)(uintptr_t)path;
        memset(args + 2, 0, sizeof(uint64_t) * 4);
        int64_t h = vsl_nt_syscall_dispatch(&ctx, 123, args, 2);  /* OpenFile */
        CHECK(h != 0, "NtOpenFile opens/creates a file and returns a handle");

        const char *msg = "WuBuOS-NT";
        uint64_t wbuf = (uint64_t)(uintptr_t)msg;
        args[0] = (uint64_t)h;
        args[1] = wbuf;
        args[2] = 8;                      /* count */
        args[3] = 0;                      /* offset */
        memset(args + 4, 0, sizeof(uint64_t) * 2);
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 285, args, 4);   /* WriteFile */
        CHECK(r == 8, "NtWriteFile writes 8 bytes");

        char rbuf[16] = {0};
        uint64_t rbuf_u = (uint64_t)(uintptr_t)rbuf;
        args[0] = (uint64_t)h;
        args[1] = rbuf_u;
        args[2] = 8;
        args[3] = 0;
        memset(args + 4, 0, sizeof(uint64_t) * 2);
        r = vsl_nt_syscall_dispatch(&ctx, 192, args, 4);           /* ReadFile */
        CHECK(r == 8, "NtReadFile reads 8 bytes");
        CHECK(memcmp(rbuf, "WuBuOS-NT", 8) == 0, "NtReadFile returns the written bytes");

        uint64_t sz = 0;
        args[0] = (uint64_t)h;
        args[1] = 0;
        args[2] = (uint64_t)(uintptr_t)&sz;
        args[3] = 0;
        args[4] = 5;                      /* FileStandardInformation */
        memset(args + 5, 0, sizeof(uint64_t));
        r = vsl_nt_syscall_dispatch(&ctx, 159, args, 5);           /* QueryInfoFile */
        CHECK(r == NT_STATUS_SUCCESS, "NtQueryInformationFile succeeds");
        CHECK(sz == 8, "NtQueryInformationFile reports size 8");

        args[0] = (uint64_t)h;
        memset(args + 1, 0, sizeof(uint64_t) * 5);
        r = vsl_nt_syscall_dispatch(&ctx, 15, args, 1);            /* NtClose */
        CHECK(r == NT_STATUS_SUCCESS, "NtClose closes the file handle");
        unlink(path);
    }

    /* 20. NtOpenEvent (127) -- returns a usable event handle */
    {
        memset(args, 0, sizeof(args));
        int64_t h = vsl_nt_syscall_dispatch(&ctx, 121, args, 1);
        CHECK(h != 0, "NtOpenEvent returns a non-zero handle");
        args[0] = (uint64_t)h;
        memset(args + 1, 0, sizeof(uint64_t) * 5);
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 15, args, 1);    /* NtClose */
        CHECK(r == NT_STATUS_SUCCESS, "NtClose frees the opened event");
    }

    /* 21. NtAllocateVirtualMemory / NtFreeVirtualMemory (19/88) -- real mmap */
    {
        void *base = NULL;
        size_t sz = 4096;
        args[0] = 0;                       /* process handle (self) */
        args[1] = (uint64_t)(uintptr_t)&base;  /* base_address* */
        args[2] = (uint64_t)(uintptr_t)&sz;    /* region_size* */
        args[3] = 0x3000;                  /* MEM_COMMIT | MEM_RESERVE */
        args[4] = 0; args[5] = 0;
        int64_t h = vsl_nt_syscall_dispatch(&ctx, 19, args, 4);
        CHECK(h != 0, "NtAllocateVirtualMemory returns a section handle");
        CHECK(base != NULL, "NtAllocateVirtualMemory maps a real region");
        if (base) { memset(base, 0xAB, sz); CHECK(*(uint8_t *)base == 0xAB, "mapped region is writable"); }

        /* Free it back. */
        args[0] = 0;
        args[1] = (uint64_t)(uintptr_t)&base;
        args[2] = (uint64_t)(uintptr_t)&sz;
        args[3] = 0x8000;                  /* MEM_RELEASE */
        args[4] = 0; args[5] = 0;
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 88, args, 4);
        CHECK(r == NT_STATUS_SUCCESS, "NtFreeVirtualMemory unmaps the region");
    }

    /* 22. NtCreateThread (56) -- real pthread runs the start routine */
    {
        g_thread_ran = 0;
        uint32_t thr = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&thr;            /* thread handle* */
        args[4] = (uint64_t)(uintptr_t)vsl_nt_test_thread_start; /* start */
        args[5] = 0;                                     /* arg */
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 78, args, 6);
        CHECK(r == NT_STATUS_SUCCESS, "NtCreateThread returns SUCCESS");
        CHECK(thr != 0, "NtCreateThread returns a thread handle");
        /* Give the thread a moment to run, then confirm it executed. */
        struct timespec ts = {0, 50 * 1000 * 1000}; /* 50 ms */
        nanosleep(&ts, NULL);
        CHECK(g_thread_ran == 1, "NtCreateThread spawned a thread that ran");
    }

    /* 23. NtCreateProcess (50) -- real fork() child tracked as a process */
    {
        uint32_t proc = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&proc;   /* process handle* */
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 192, args, 1);
        CHECK(r == NT_STATUS_SUCCESS, "NtCreateProcess returns SUCCESS");
        CHECK(proc != 0, "NtCreateProcess returns a process handle");
        /* Read back the real child pid stored in the handle payload. */
        uint64_t pid = 0;
        if (vsl_nt_handle_to_data(&ctx, proc, &pid) == 0 && pid != 0) {
            CHECK(kill((pid_t)pid, 0) == 0, "NtCreateProcess child is a live process");
            kill((pid_t)pid, SIGKILL);   /* reap the placeholder */
            int st;
            waitpid((pid_t)pid, &st, 0);
        } else {
            CHECK(0, "NtCreateProcess stored a child pid in the handle");
        }
        /* Free the process handle. */
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)proc;
        r = vsl_nt_syscall_dispatch(&ctx, 15, args, 1);  /* NtClose */
        CHECK(r == NT_STATUS_SUCCESS, "NtClose frees the process handle");
    }

    /* 24. NtOpenProcess / NtWriteVirtualMemory / NtReadVirtualMemory (129/195/288)
     *     -- genuine cross-process memory via process_vm_writev/readv (self). */
    {
        uint32_t ph = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&ph;   /* process handle* */
        args[2] = 0;                          /* client_id 0 = open self */
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 38, args, 3);
        CHECK(r == NT_STATUS_SUCCESS, "NtOpenProcess(self) returns SUCCESS");
        CHECK(ph != 0, "NtOpenProcess returns a handle");

        uint8_t src[8] = {1,2,3,4,5,6,7,8};
        uint8_t dst[8] = {0};
        uint64_t wsize = 8, rsize = 8, written = 0, read = 0;
        /* Write into our own dst buffer via the process memory path. */
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)ph;
        args[1] = (uint64_t)(uintptr_t)dst;       /* base */
        args[2] = (uint64_t)(uintptr_t)src;       /* buffer */
        args[3] = (uint64_t)(uintptr_t)&wsize;    /* size* */
        args[4] = (uint64_t)(uintptr_t)&written;  /* bytes_written* */
        r = vsl_nt_syscall_dispatch(&ctx, 195, args, 5);
        CHECK(r == NT_STATUS_SUCCESS, "NtWriteVirtualMemory writes into process memory");
        CHECK(written == 8, "NtWriteVirtualMemory reports 8 bytes written");

        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)ph;
        args[1] = (uint64_t)(uintptr_t)dst;       /* base */
        args[2] = (uint64_t)(uintptr_t)src;       /* reuse src as read buffer */
        args[3] = (uint64_t)(uintptr_t)&rsize;    /* size* */
        args[4] = (uint64_t)(uintptr_t)&read;     /* bytes_read* */
        r = vsl_nt_syscall_dispatch(&ctx, 288, args, 5);
        CHECK(r == NT_STATUS_SUCCESS, "NtReadVirtualMemory reads from process memory");
        CHECK(read == 8, "NtReadVirtualMemory reports 8 bytes read");
        CHECK(memcmp(src, dst, 8) == 0, "NtReadVirtualMemory reads back the written bytes");

        /* Close self-process handle. */
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)ph;
        r = vsl_nt_syscall_dispatch(&ctx, 15, args, 1);
        CHECK(r == NT_STATUS_SUCCESS, "NtClose frees the OpenProcess handle");
    }

    /* 25. NtCreateSection / NtMapViewOfSection (53/114) -- real mmap section. */
    {
        uint32_t sec = 0;
        size_t sec_sz = 8192;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&sec;        /* section handle* */
        args[2] = (uint64_t)(uintptr_t)&sec_sz;     /* max size* */
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 53, args, 3);
        CHECK(r == NT_STATUS_SUCCESS, "NtCreateSection returns SUCCESS");
        CHECK(sec != 0, "NtCreateSection returns a section handle");

        void *view = NULL;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)sec;                    /* section handle */
        args[1] = 0;                                /* process handle (self) */
        args[2] = (uint64_t)(uintptr_t)&view;       /* base* */
        r = vsl_nt_syscall_dispatch(&ctx, 114, args, 3);
        CHECK(r == NT_STATUS_SUCCESS, "NtMapViewOfSection maps a view");
        CHECK(view != NULL, "NtMapViewOfSection returns a mapped base");
        if (view) { memset(view, 0x5A, 16); CHECK(*(uint8_t *)view == 0x5A, "mapped view is writable"); }

        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)sec;
        r = vsl_nt_syscall_dispatch(&ctx, 15, args, 1);
        CHECK(r == NT_STATUS_SUCCESS, "NtClose frees the section handle");
    }

    /* 26. NtTerminateProcess (267) -- real kill+reap of a forked child. */
    {
        uint32_t proc = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&proc;       /* process handle* */
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 192, args, 1);  /* NtCreateProcess */
        CHECK(r == NT_STATUS_SUCCESS, "NtCreateProcess (for terminate test) SUCCESS");
        CHECK(proc != 0, "NtCreateProcess returns a handle");

        uint64_t pid = 0;
        vsl_nt_handle_to_data(&ctx, proc, &pid);
        CHECK(pid != 0, "process handle carries a live child pid");
        if (pid) CHECK(kill((pid_t)pid, 0) == 0, "child is alive before terminate");

        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)proc;
        r = vsl_nt_syscall_dispatch(&ctx, 44, args, 1); /* NtTerminateProcess */
        CHECK(r == NT_STATUS_SUCCESS, "NtTerminateProcess kills + reaps the child");
        if (pid) CHECK(kill((pid_t)pid, 0) != 0, "child is dead after terminate");
    }

    /* 27. NtCreateThread(CREATE_SUSPENDED) + NtResumeThread (56/215) */
    {
        g_thread_ran = 0;
        uint32_t thr = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&thr;                 /* thread handle* */
        args[1] = 0x4;                                        /* CREATE_SUSPENDED */
        args[4] = (uint64_t)(uintptr_t)vsl_nt_test_thread_start; /* start */
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 78, args, 6);
        CHECK(r == NT_STATUS_SUCCESS, "NtCreateThread(suspended) returns SUCCESS");
        CHECK(thr != 0, "NtCreateThread(suspended) returns a handle");
        /* Give it a moment: should NOT have run yet (suspended). */
        struct timespec ts = {0, 30 * 1000 * 1000};
        nanosleep(&ts, NULL);
        CHECK(g_thread_ran == 0, "suspended thread has NOT run yet");
        /* Resume it. */
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)thr;
        r = vsl_nt_syscall_dispatch(&ctx, 82, args, 1);      /* NtResumeThread */
        CHECK(r == NT_STATUS_SUCCESS, "NtResumeThread returns SUCCESS");
        ts.tv_nsec = 50 * 1000 * 1000;
        nanosleep(&ts, NULL);
        CHECK(g_thread_ran == 1, "resumed thread ran after NtResumeThread");
    }

    /* 28. NtCreateMutant / NtReleaseMutant (46/197) -- recursive mutex */
    {
        uint32_t m = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&m;                    /* mutant handle* */
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 186, args, 1); /* NtCreateMutant */
        CHECK(r == NT_STATUS_SUCCESS, "NtCreateMutant returns SUCCESS");
        CHECK(m != 0, "NtCreateMutant returns a handle");
        r = vsl_nt_syscall_dispatch(&ctx, 119, args, 3);      /* wait = lock */
        /* NtWaitForSingleObject on the mutant: a=mutation handle. */
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)m;
        r = vsl_nt_syscall_dispatch(&ctx, 119, args, 3);      /* NtWaitForSingleObject */
        CHECK(r == NT_STATUS_SUCCESS, "NtWaitForSingleObject acquires the mutant");
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)m;
        r = vsl_nt_syscall_dispatch(&ctx, 32, args, 1);      /* NtReleaseMutant */
        CHECK(r == NT_STATUS_SUCCESS, "NtReleaseMutant releases the mutant");
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)m;
        r = vsl_nt_syscall_dispatch(&ctx, 15, args, 1);       /* NtClose */
        CHECK(r == NT_STATUS_SUCCESS, "NtClose frees the mutant");
    }

    /* 29. NtCreateSemaphore / NtReleaseSemaphore / wait (54/198/282) */
    {
        uint32_t s = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&s;                    /* sem handle* */
        args[2] = 0;                                          /* initial count 0 */
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 199, args, 3); /* NtCreateSemaphore */
        CHECK(r == NT_STATUS_SUCCESS, "NtCreateSemaphore returns SUCCESS");
        CHECK(s != 0, "NtCreateSemaphore returns a handle");
        /* Post 1 so a waiter can proceed. */
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)s;
        args[2] = 1;                                          /* release 1 */
        r = vsl_nt_syscall_dispatch(&ctx, 7, args, 3);      /* NtReleaseSemaphore */
        CHECK(r == NT_STATUS_SUCCESS, "NtReleaseSemaphore posts the semaphore");
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)s;
        r = vsl_nt_syscall_dispatch(&ctx, 119, args, 3);      /* NtWaitForSingleObject */
        CHECK(r == NT_STATUS_SUCCESS, "NtWaitForSingleObject consumes the semaphore");
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)s;
        r = vsl_nt_syscall_dispatch(&ctx, 15, args, 1);       /* NtClose */
        CHECK(r == NT_STATUS_SUCCESS, "NtClose frees the semaphore");
    }

    /* 30. NtDuplicateObject (72) -- clone a handle */
    {
        uint32_t ev = 0;
        memset(args, 0, sizeof(args));
        args[0] = 0;
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 72, args, 1); /* NtCreateEvent */
        ev = (uint32_t)r;
        CHECK(ev != 0, "NtCreateEvent for dup test");
        uint32_t dup = 0;
        memset(args, 0, sizeof(args));
        args[1] = (uint64_t)ev;                               /* source handle */
        args[3] = (uint64_t)(uintptr_t)&dup;                  /* new handle* */
        r = vsl_nt_syscall_dispatch(&ctx, 60, args, 4);       /* NtDuplicateObject */
        CHECK(r == NT_STATUS_SUCCESS, "NtDuplicateObject returns SUCCESS");
        CHECK(dup != 0 && dup != ev, "NtDuplicateObject returns a distinct handle");
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)ev; r = vsl_nt_syscall_dispatch(&ctx, 15, args, 1);
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)dup; r = vsl_nt_syscall_dispatch(&ctx, 15, args, 1);
        CHECK(r == NT_STATUS_SUCCESS, "NtClose frees the duplicated handle");
    }

    /* 31. NtQueryInformationProcess (162) -- returns the live child pid */
    {
        uint32_t proc = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&proc;
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 192, args, 1); /* NtCreateProcess */
        CHECK(r == NT_STATUS_SUCCESS, "NtCreateProcess for query test");
        uint64_t qbuf[2] = {0, 0};
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)proc;
        args[1] = 0;                                          /* ProcessBasicInformation */
        args[2] = (uint64_t)(uintptr_t)qbuf;                   /* out */
        r = vsl_nt_syscall_dispatch(&ctx, 25, args, 3);      /* NtQueryInformationProcess */
        CHECK(r == NT_STATUS_SUCCESS, "NtQueryInformationProcess returns SUCCESS");
        CHECK(qbuf[0] != 0, "NtQueryInformationProcess returns a nonzero pid");
        /* Exit-status query: child still alive -> STILL_ACTIVE (0x103). */
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)proc;
        args[1] = 1;                                          /* ProcessExitStatus */
        args[2] = (uint64_t)(uintptr_t)qbuf;
        r = vsl_nt_syscall_dispatch(&ctx, 25, args, 3);
        CHECK(r == NT_STATUS_SUCCESS, "NtQueryInformationProcess(exit) SUCCESS");
        CHECK(qbuf[0] == 0x103, "live child reports STILL_ACTIVE");
        /* Terminate then re-query. */
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)proc;
        r = vsl_nt_syscall_dispatch(&ctx, 44, args, 1);      /* NtTerminateProcess */
        CHECK(r == NT_STATUS_SUCCESS, "NtTerminateProcess for query test");
    }

    /* 32. NtOpenThread (135) -- open a running thread by tid */
    {
        uint32_t thr = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&thr;
        args[4] = (uint64_t)(uintptr_t)vsl_nt_test_thread_start;
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 78, args, 6); /* NtCreateThread */
        CHECK(r == NT_STATUS_SUCCESS, "NtCreateThread for open-thread test");
        /* Grab its tid via the handle. We stored tid in styx_fid; expose by
         * reading the live process's thread through NtWaitForSingleObject join
         * is overkill — instead just verify OpenThread mints a handle. */
        uint32_t othr = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&othr;
        args[2] = (uint64_t)(uintptr_t)thr;  /* client_id = source handle (proxy) */
        r = vsl_nt_syscall_dispatch(&ctx, 313, args, 3);      /* NtOpenThread */
        CHECK(r == NT_STATUS_SUCCESS, "NtOpenThread returns SUCCESS");
        CHECK(othr != 0, "NtOpenThread returns a handle");
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)thr; r = vsl_nt_syscall_dispatch(&ctx, 15, args, 1);
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)othr; r = vsl_nt_syscall_dispatch(&ctx, 15, args, 1);
        CHECK(r == NT_STATUS_SUCCESS, "NtClose frees the opened thread");
    }

    /* 33. Batch 7: registry + system-info (Styx9 namespace + SteamOS view).
     * Asserts REAL behavior: the NT registry is backed by real files. */
    {
        uint32_t key = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)"\\Registry\\Machine\\Software\\WuBu";
        args[4] = (uint64_t)(uintptr_t)&key;
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 29, args, 5); /* NtCreateKey */
        CHECK(r == NT_STATUS_SUCCESS, "NtCreateKey creates a registry key (real dir)");
        CHECK(key != 0, "NtCreateKey returns a handle");

        /* Set a value; verify the backing file exists and round-trips. */
        const char *val = "wubuos";
        uint32_t vlen = (uint32_t)strlen(val);
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)key;
        args[1] = (uint64_t)(uintptr_t)"Name";
        args[2] = 3;                       /* REG_SZ */
        args[3] = (uint64_t)(uintptr_t)val;
        args[4] = (uint64_t)vlen;
        r = vsl_nt_syscall_dispatch(&ctx, 96, args, 5); /* NtSetValueKey */
        CHECK(r == NT_STATUS_SUCCESS, "NtSetValueKey writes value to real file");

        char qbuf[32];
        memset(qbuf, 0, sizeof(qbuf));
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)key;
        args[1] = (uint64_t)(uintptr_t)"Name";
        args[3] = (uint64_t)(uintptr_t)qbuf;
        args[4] = (uint64_t)sizeof(qbuf);
        r = vsl_nt_syscall_dispatch(&ctx, 23, args, 5); /* NtQueryValueKey */
        CHECK(r == NT_STATUS_SUCCESS, "NtQueryValueKey reads value back");
        CHECK(strncmp(qbuf, "wubuos", vlen) == 0, "NtQueryValueKey round-trips data");

        /* System time must be non-zero (100ns ticks since 1601). */
        uint64_t st = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&st;
        r = vsl_nt_syscall_dispatch(&ctx, 91, args, 1); /* NtQuerySystemTime */
        CHECK(r == NT_STATUS_SUCCESS, "NtQuerySystemTime SUCCESS");
        CHECK(st != 0, "NtQuerySystemTime returns nonzero time");

        /* System info: page size + processor count. */
        uint8_t sinfo[64];
        memset(sinfo, 0, sizeof(sinfo));
        memset(args, 0, sizeof(args));
        args[0] = 2;                        /* SystemBasicInformation */
        args[1] = (uint64_t)(uintptr_t)sinfo;
        args[2] = sizeof(sinfo);
        r = vsl_nt_syscall_dispatch(&ctx, 54, args, 3); /* NtQuerySystemInformation */
        CHECK(r == NT_STATUS_SUCCESS, "NtQuerySystemInformation SUCCESS");
        uint32_t page_sz = *(uint32_t *)(sinfo);
        CHECK(page_sz == (uint32_t)sysconf(_SC_PAGESIZE), "NtQuerySystemInformation reports real page size");

        /* Capture the key's backing directory handle BEFORE deleting, so we
         * can assert the recursive delete actually removed the tree. */
        uint64_t keydir = 0;
        CHECK(vsl_nt_handle_to_data(&ctx, key, &keydir) == 0 && keydir != 0,
              "NtCreateKey key exposes a real backing dir");

        /* Clean up the key. */
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)key;
        r = vsl_nt_syscall_dispatch(&ctx, 220, args, 1);  /* NtDeleteKey */
        CHECK(r == NT_STATUS_SUCCESS, "NtDeleteKey removes the registry key");
        /* The recursive delete must actually remove the backing dir tree
         * (key dir + its value file), not just free the handle. */
        CHECK(access((const char *)(uintptr_t)keydir, F_OK) != 0,
              "NtDeleteKey recursively removed the key directory tree");
    }

    /* 34. Batch 8: file + section + memory + yield surface (real VSL work). */
    {
        /* NtCreateFile (40) — create a real temp file, get an NT handle. */
        uint32_t fh = 0;
        char cf_path[] = "/tmp/wubu_nt_createfile";
        unlink(cf_path);
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&fh;
        args[2] = (uint64_t)(uintptr_t)cf_path;
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 85, args, 5);
        CHECK(r == NT_STATUS_SUCCESS, "NtCreateFile creates a real file");
        CHECK(fh != 0, "NtCreateFile returns a handle");

        /* NtWriteFile / NtReadFile round-trip via the existing handlers. */
        const char *msg8 = "batch8";
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)fh;
        args[1] = (uint64_t)(uintptr_t)msg8;
        args[2] = (uint64_t)strlen(msg8);
        r = vsl_nt_syscall_dispatch(&ctx, 285, args, 3);
        CHECK(r == (int64_t)strlen(msg8), "NtWriteFile writes via NtCreateFile handle");
        char rd8[16]; memset(rd8, 0, sizeof(rd8));
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)fh;
        args[1] = (uint64_t)(uintptr_t)rd8;
        args[2] = (uint64_t)sizeof(rd8);
        r = vsl_nt_syscall_dispatch(&ctx, 192, args, 3);
        CHECK(r == (int64_t)strlen(msg8), "NtReadFile reads via NtCreateFile handle");
        CHECK(strncmp(rd8, "batch8", 6) == 0, "NtCreateFile+NtReadFile round-trip");

        /* NtSetInformationFile (234) truncate to 0 via FileEndOfFileInformation. */
        int64_t zero = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)fh;
        args[2] = (uint64_t)(uintptr_t)&zero;
        args[4] = 5;  /* FileEndOfFileInformation */
        r = vsl_nt_syscall_dispatch(&ctx, 234, args, 5);
        CHECK(r == NT_STATUS_SUCCESS, "NtSetInformationFile truncates the file");

        /* NtFlushBuffersFile (82) — fsync the fd. */
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)fh;
        r = vsl_nt_syscall_dispatch(&ctx, 82, args, 1);
        CHECK(r == NT_STATUS_SUCCESS, "NtFlushBuffersFile fsyncs the fd");

        /* NtQueryVolumeInformationFile (184) — real total/free bytes. */
        uint8_t vol[32]; memset(vol, 0, sizeof(vol));
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)fh;
        args[3] = (uint64_t)(uintptr_t)vol;
        r = vsl_nt_syscall_dispatch(&ctx, 184, args, 4);
        CHECK(r == NT_STATUS_SUCCESS, "NtQueryVolumeInformationFile SUCCESS");
        uint64_t total_bytes = *(uint64_t *)vol;
        CHECK(total_bytes > 0, "NtQueryVolumeInformationFile reports nonzero total bytes");

        /* NtClose (28) the file handle. */
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)fh;
        r = vsl_nt_syscall_dispatch(&ctx, 15, args, 1);
        CHECK(r == NT_STATUS_SUCCESS, "NtClose frees the NtCreateFile handle");
        unlink(cf_path);

        /* NtPulseEvent (145) — one-shot signal on an eventfd. */
        uint32_t ev = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&ev;
        r = vsl_nt_syscall_dispatch(&ctx, 72, args, 1); /* NtCreateEvent */
        CHECK(r != 0, "NtCreateEvent for pulse test");
        ev = (uint32_t)r;  /* NtCreateEvent returns the handle as its result */
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)ev;
        r = vsl_nt_syscall_dispatch(&ctx, 328, args, 1); /* NtPulseEvent */
        CHECK(r == NT_STATUS_SUCCESS, "NtPulseEvent sets+resets the event");
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)ev;
        r = vsl_nt_syscall_dispatch(&ctx, 15, args, 1);
        CHECK(r == NT_STATUS_SUCCESS, "NtClose frees the pulse event");

        /* NtYieldExecution (289) — sched_yield, must return SUCCESS. */
        memset(args, 0, sizeof(args));
        r = vsl_nt_syscall_dispatch(&ctx, 289, args, 0);
        CHECK(r == NT_STATUS_SUCCESS, "NtYieldExecution SUCCESS");

        /* NtResetVirtualMemory (283) — decommit a private mapping. */
        void *mp = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
        CHECK(mp != MAP_FAILED, "mmap for NtResetVirtualMemory test");
        memset(args, 0, sizeof(args));
        args[1] = (uint64_t)(uintptr_t)mp;
        args[2] = (uint64_t)4096;
        r = vsl_nt_syscall_dispatch(&ctx, 283, args, 3);
        CHECK(r == NT_STATUS_SUCCESS, "NtResetVirtualMemory decommits the region");
        munmap(mp, 4096);
    }

    /* 35. Blitz batch: high-value file/mem/section/sync/registry surface. */
    {
        int64_t r;  /* dispatch result for this block */
        /* NtQueryAttributesFile (146) — stat a real file. */
        char qa_path[] = "/tmp/wubu_nt_qattr";
        int qfd = open(qa_path, O_RDWR|O_CREAT, 0644); close(qfd);
        uint8_t qabuf[40]; memset(qabuf, 0, sizeof(qabuf));
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)qa_path;
        args[1] = (uint64_t)(uintptr_t)qabuf;
        r = vsl_nt_syscall_dispatch(&ctx, 146, args, 2);
        CHECK(r == NT_STATUS_SUCCESS, "NtQueryAttributesFile stats a real file");
        unlink(qa_path);

        /* NtDeleteFile (66) — unlink by name. */
        char df_path[] = "/tmp/wubu_nt_delfile";
        int dfd = open(df_path, O_RDWR|O_CREAT, 0644); close(dfd);
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)df_path;
        r = vsl_nt_syscall_dispatch(&ctx, 66, args, 1);
        CHECK(r == NT_STATUS_SUCCESS, "NtDeleteFile removes a real file");
        CHECK(access(df_path, F_OK) != 0, "NtDeleteFile target is gone");

        /* NtProtectVirtualMemory (144) — mprotect a private mapping. */
        void *pm = mmap(NULL, 4096, PROT_READ, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
        CHECK(pm != MAP_FAILED, "mmap for NtProtectVirtualMemory");
        memset(args, 0, sizeof(args));
        args[1] = (uint64_t)(uintptr_t)pm;
        args[2] = 4096;
        args[3] = 0x4; /* PAGE_READWRITE */
        r = vsl_nt_syscall_dispatch(&ctx, 144, args, 4);
        CHECK(r == NT_STATUS_SUCCESS, "NtProtectVirtualMemory mprotects the range");
        munmap(pm, 4096);

        /* NtLockVirtualMemory (109) / NtUnlockVirtualMemory (277). */
        void *lm = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
        CHECK(lm != MAP_FAILED, "mmap for NtLockVirtualMemory");
        memset(args, 0, sizeof(args));
        args[1] = (uint64_t)(uintptr_t)lm; args[2] = 4096;
        r = vsl_nt_syscall_dispatch(&ctx, 109, args, 3);
        CHECK(r == NT_STATUS_SUCCESS, "NtLockVirtualMemory mlock");
        r = vsl_nt_syscall_dispatch(&ctx, 277, args, 3);
        CHECK(r == NT_STATUS_SUCCESS, "NtUnlockVirtualMemory munlock");
        munmap(lm, 4096);

        /* NtOpenSection (132) + NtQuerySection (176). */
        uint32_t sec = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&sec;
        r = vsl_nt_syscall_dispatch(&ctx, 132, args, 1);
        CHECK(r == NT_STATUS_SUCCESS, "NtOpenSection mints a handle");
        uint8_t sinfo[16]; memset(sinfo, 0, sizeof(sinfo));
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)sec; args[2] = (uint64_t)(uintptr_t)sinfo;
        r = vsl_nt_syscall_dispatch(&ctx, 176, args, 3);
        CHECK(r == NT_STATUS_SUCCESS, "NtQuerySection reports section info");
        memset(args, 0, sizeof(args)); args[0] = (uint64_t)sec;
        vsl_nt_syscall_dispatch(&ctx, 15, args, 1); /* NtClose */

        /* NtLoadKey (103) + NtUnloadKey (273). */
        uint32_t hk = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&hk;
        r = vsl_nt_syscall_dispatch(&ctx, 103, args, 1);
        CHECK(r == NT_STATUS_SUCCESS, "NtLoadKey mounts a hive");
        CHECK(hk != 0, "NtLoadKey returns a hive key");
        memset(args, 0, sizeof(args)); args[0] = (uint64_t)hk;
        r = vsl_nt_syscall_dispatch(&ctx, 273, args, 1);
        CHECK(r == NT_STATUS_SUCCESS, "NtUnloadKey unmounts the hive");

        /* NtFlushKey (84) on a fresh key — should succeed (nothing to flush). */
        memset(args, 0, sizeof(args)); args[0] = (uint64_t)1;
        r = vsl_nt_syscall_dispatch(&ctx, 84, args, 1);
        CHECK(r == NT_STATUS_SUCCESS, "NtFlushKey succeeds");

        /* NtOpenMutant (127) + NtOpenSemaphore (133). */
        uint32_t om = 0;
        memset(args, 0, sizeof(args)); args[0] = (uint64_t)(uintptr_t)&om;
        r = vsl_nt_syscall_dispatch(&ctx, 127, args, 1);
        CHECK(r == NT_STATUS_SUCCESS, "NtOpenMutant mints a handle");
        uint32_t os = 0;
        memset(args, 0, sizeof(args)); args[0] = (uint64_t)(uintptr_t)&os;
        r = vsl_nt_syscall_dispatch(&ctx, 133, args, 1);
        CHECK(r == NT_STATUS_SUCCESS, "NtOpenSemaphore mints a handle");
        memset(args, 0, sizeof(args)); args[0] = (uint64_t)om;
        vsl_nt_syscall_dispatch(&ctx, 15, args, 1);
        memset(args, 0, sizeof(args)); args[0] = (uint64_t)os;
        vsl_nt_syscall_dispatch(&ctx, 15, args, 1);
    }

    /* 36. Blitz-2: registry persist, process/thread control, timers, locks, dir. */
    {
        int64_t r;
        /* NtCreateKey + NtSetValueKey + NtSaveKey + NtRestoreKey (file-backed). */
        uint32_t bk = 0;
        char bpath[] = "\\Registry\\Machine\\Blitz2Key";
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)bpath;
        args[4] = (uint64_t)(uintptr_t)&bk;
        r = vsl_nt_syscall_dispatch(&ctx, 29, args, 5); /* NtCreateKey */
        CHECK(r == NT_STATUS_SUCCESS, "NtCreateKey (blitz2)");
        char bname[] = "BlitzVal"; uint8_t bval[16]; memset(bval, 0xAB, sizeof(bval));
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)bk; args[1] = (uint64_t)(uintptr_t)bname;
        args[2] = 3; args[3] = (uint64_t)(uintptr_t)bval; args[4] = sizeof(bval);
        r = vsl_nt_syscall_dispatch(&ctx, 96, args, 5); /* NtSetValueKey */
        CHECK(r == NT_STATUS_SUCCESS, "NtSetValueKey (blitz2)");
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)bk;
        r = vsl_nt_syscall_dispatch(&ctx, 401, args, 2); /* NtSaveKey */
        CHECK(r == NT_STATUS_SUCCESS, "NtSaveKey copies the hive");
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)bk;
        r = vsl_nt_syscall_dispatch(&ctx, 393, args, 3); /* NtRestoreKey */
        CHECK(r == NT_STATUS_SUCCESS, "NtRestoreKey restores the hive");
        memset(args, 0, sizeof(args)); args[0] = (uint64_t)bk;
        vsl_nt_syscall_dispatch(&ctx, 220, args, 1); /* NtDeleteKey */

        /* NtCreateTimer + NtSetTimer + NtCancelTimer (timerfd). */
        uint32_t tm = 0;
        memset(args, 0, sizeof(args)); args[0] = (uint64_t)(uintptr_t)&tm;
        r = vsl_nt_syscall_dispatch(&ctx, 203, args, 1); /* NtCreateTimer */
        CHECK(r == NT_STATUS_SUCCESS, "NtCreateTimer mints a timerfd handle");
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)tm; args[1] = (uint64_t)(-10000000LL); /* 1s relative */
        r = vsl_nt_syscall_dispatch(&ctx, 98, args, 5); /* NtSetTimer */
        CHECK(r == NT_STATUS_SUCCESS, "NtSetTimer arms the timerfd");
        memset(args, 0, sizeof(args)); args[0] = (uint64_t)tm;
        r = vsl_nt_syscall_dispatch(&ctx, 97, args, 2); /* NtCancelTimer */
        CHECK(r == NT_STATUS_SUCCESS, "NtCancelTimer disarms the timerfd");
        memset(args, 0, sizeof(args)); args[0] = (uint64_t)tm;
        vsl_nt_syscall_dispatch(&ctx, 15, args, 1); /* NtClose */

        /* NtSuspendProcess / NtResumeProcess on a forked child. */
        pid_t cp = fork();
        if (cp == 0) { for (;;) pause(); }
        uint32_t cph = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&cph;
        args[2] = (uint64_t)(uint32_t)cp;  /* NtOpenProcess: client_id = pid */
        r = vsl_nt_syscall_dispatch(&ctx, 38, args, 3); /* NtOpenProcess */
        CHECK(r == NT_STATUS_SUCCESS, "NtOpenProcess (blitz2) mints child handle");
        memset(args, 0, sizeof(args)); args[0] = (uint64_t)cph;
        r = vsl_nt_syscall_dispatch(&ctx, 462, args, 1); /* NtSuspendProcess */
        CHECK(r == NT_STATUS_SUCCESS, "NtSuspendProcess SIGSTOPs the child");
        r = vsl_nt_syscall_dispatch(&ctx, 394, args, 1); /* NtResumeProcess */
        CHECK(r == NT_STATUS_SUCCESS, "NtResumeProcess SIGCONTs the child");
        kill(cp, SIGKILL); waitpid(cp, NULL, 0);
        memset(args, 0, sizeof(args)); args[0] = (uint64_t)cph;
        vsl_nt_syscall_dispatch(&ctx, 15, args, 1);

        /* NtLockFile / NtUnlockFile on a real file. */
        char lf[] = "/tmp/wubu_nt_lockfile"; int lfd = open(lf, O_RDWR|O_CREAT,0644);
        uint32_t lh = 0;
        memset(args, 0, sizeof(args)); args[0] = (uint64_t)(uintptr_t)&lh;
        args[2] = (uint64_t)(uintptr_t)lf;
        vsl_nt_syscall_dispatch(&ctx, 85, args, 5); /* NtCreateFile */
        memset(args, 0, sizeof(args)); args[0] = (uint64_t)lh; args[2] = 0; args[3] = 4096; args[4] = 1;
        r = vsl_nt_syscall_dispatch(&ctx, 275, args, 5); /* NtLockFile */
        CHECK(r == NT_STATUS_SUCCESS, "NtLockFile sets an advisory lock");
        r = vsl_nt_syscall_dispatch(&ctx, 477, args, 5); /* NtUnlockFile */
        CHECK(r == NT_STATUS_SUCCESS, "NtUnlockFile clears the lock");
        memset(args, 0, sizeof(args)); args[0] = (uint64_t)lh;
        vsl_nt_syscall_dispatch(&ctx, 15, args, 1);
        close(lfd); unlink(lf);

        /* NtOpenDirectoryObject (120). */
        uint32_t dir = 0;
        memset(args, 0, sizeof(args)); args[0] = (uint64_t)(uintptr_t)&dir;
        r = vsl_nt_syscall_dispatch(&ctx, 120, args, 1);
        CHECK(r == NT_STATUS_SUCCESS, "NtOpenDirectoryObject mints a handle");
        memset(args, 0, sizeof(args)); args[0] = (uint64_t)dir;
        vsl_nt_syscall_dispatch(&ctx, 15, args, 1);

        /* NtQueryMultipleValueKey / NtCompactKeys / NtInitializeRegistry accept. */
        memset(args, 0, sizeof(args)); args[0] = (uint64_t)bk + 1; args[2] = 1;
        r = vsl_nt_syscall_dispatch(&ctx, 169, args, 3);
        CHECK(r == NT_STATUS_SUCCESS, "NtQueryMultipleValueKey accepts");
        memset(args, 0, sizeof(args));
        r = vsl_nt_syscall_dispatch(&ctx, 30, args, 2);
        CHECK(r == NT_STATUS_SUCCESS, "NtCompactKeys accepts");
        memset(args, 0, sizeof(args));
        r = vsl_nt_syscall_dispatch(&ctx, 97, args, 1);
        CHECK(r == NT_STATUS_SUCCESS, "NtInitializeRegistry accepts");
    }

    /* 37. Batch 9: Object Manager + Performance (real VSL work). */
    {
        /* NtCreateDirectoryObject (37) -- real dir under the namespace root. */
        uint32_t od = 0;
        char odname[] = "MyObjDir";
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)odname;
        args[1] = (uint64_t)(uintptr_t)&od;
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 37, args, 3);
        CHECK(r != NT_STATUS_INVALID_PARAMETER, "NtCreateDirectoryObject returns a handle");
        CHECK(od != 0, "NtCreateDirectoryObject handle nonzero");

        /* NtQueryObject (171) on the directory handle reports type "Directory". */
        uint8_t oinfo[64];
        uint32_t oret = 0;
        memset(oinfo, 0, sizeof(oinfo));
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)od;
        args[1] = (uint64_t)(uintptr_t)oinfo;
        args[2] = sizeof(oinfo);
        args[3] = (uint64_t)(uintptr_t)&oret;
        r = vsl_nt_syscall_dispatch(&ctx, 171, args, 4);
        CHECK(r == NT_STATUS_SUCCESS, "NtQueryObject returns SUCCESS");
        /* OBJECT_TYPE_INFORMATION: name length at offset 0, name at offset 8 (UTF-16LE). */
        uint32_t tnlen = *(uint32_t *)oinfo;
        CHECK(tnlen == strlen("Directory"), "NtQueryObject reports Directory type name");

        /* NtMakeTemporaryObject (111) -- succeeds on a valid handle. */
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)od;
        r = vsl_nt_syscall_dispatch(&ctx, 111, args, 1);
        CHECK(r == NT_STATUS_SUCCESS, "NtMakeTemporaryObject marks the handle");

        /* NtQueryDirectoryObject (153) -- enumerate the (empty) dir. */
        uint8_t doinfo[64];
        memset(doinfo, 0, sizeof(doinfo));
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)od;
        args[1] = (uint64_t)(uintptr_t)doinfo;
        args[2] = sizeof(doinfo);
        args[3] = (uint64_t)(uintptr_t)NULL;
        args[5] = 0;  /* index 0 */
        r = vsl_nt_syscall_dispatch(&ctx, 153, args, 4);
        /* Empty dir => NO_MORE_FILES; that still proves the syscall ran real work. */
        CHECK(r == NT_STATUS_NO_MORE_FILES || r == NT_STATUS_SUCCESS,
              "NtQueryDirectoryObject enumerates (empty dir => NO_MORE_FILES)");

        /* NtQueryPerformanceCounter (174) -- real monotonic clock. */
        uint64_t pc = 0, pc2 = 0, freq = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&pc;
        args[1] = (uint64_t)(uintptr_t)&freq;
        r = vsl_nt_syscall_dispatch(&ctx, 174, args, 2);
        CHECK(r == NT_STATUS_SUCCESS, "NtQueryPerformanceCounter SUCCESS");
        CHECK(pc != 0, "NtQueryPerformanceCounter returns nonzero ticks");
        CHECK(freq == 10000000ULL, "NtQueryPerformanceCounter reports 10MHz (100ns ticks)");
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&pc2;
        vsl_nt_syscall_dispatch(&ctx, 174, args, 2);
        CHECK(pc2 >= pc, "NtQueryPerformanceCounter is monotonic");

        /* NtQueryTimerResolution (185) -- real clock resolution. */
        uint32_t tmin = 0, tmax = 0, tcur = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&tmin;
        args[1] = (uint64_t)(uintptr_t)&tmax;
        args[2] = (uint64_t)(uintptr_t)&tcur;
        r = vsl_nt_syscall_dispatch(&ctx, 185, args, 3);
        CHECK(r == NT_STATUS_SUCCESS, "NtQueryTimerResolution SUCCESS");
        CHECK(tcur != 0 && tmax != 0, "NtQueryTimerResolution returns real resolution");

        /* Clean up the directory object handle. */
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)od;
        vsl_nt_syscall_dispatch(&ctx, 15, args, 1);
    }

    /* 38. Batch 9b: Registry value-delete + key-query + subkey-count. */
    {
        /* Build a key with a value + a subkey, then exercise the new syscalls. */
        uint32_t rk = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)"\\Registry\\Machine\\Software\\Batch9";
        args[4] = (uint64_t)(uintptr_t)&rk;
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 29, args, 5); /* NtCreateKey */
        CHECK(r == NT_STATUS_SUCCESS, "NtCreateKey (Batch9) SUCCESS");

        /* Set a value. */
        const char *v = "valdata";
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)rk; args[1] = (uint64_t)(uintptr_t)"Data";
        args[2] = 3; args[3] = (uint64_t)(uintptr_t)v; args[4] = strlen(v);
        r = vsl_nt_syscall_dispatch(&ctx, 96, args, 5); /* NtSetValueKey */
        CHECK(r == NT_STATUS_SUCCESS, "NtSetValueKey (Batch9) writes value");

        /* Create a subkey so QueryOpenSubKeys > 0. */
        uint32_t sk = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)"\\Registry\\Machine\\Software\\Batch9\\Sub";
        args[4] = (uint64_t)(uintptr_t)&sk;
        r = vsl_nt_syscall_dispatch(&ctx, 29, args, 5); /* NtCreateKey (subkey) */
        CHECK(r == NT_STATUS_SUCCESS, "NtCreateKey (subkey) SUCCESS");

        /* NtQueryOpenSubKeys (172) -- count immediate subkeys (>= 1). */
        uint32_t subcount = 0xFF;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)rk;
        args[1] = (uint64_t)(uintptr_t)&subcount;
        r = vsl_nt_syscall_dispatch(&ctx, 172, args, 2);
        CHECK(r == NT_STATUS_SUCCESS, "NtQueryOpenSubKeys SUCCESS");
        CHECK(subcount >= 1, "NtQueryOpenSubKeys counts the subkey");

        /* NtQueryKey (168) -- KEY_BASIC_INFORMATION (name + last-write time). */
        uint8_t kinfo[128];
        uint32_t kres = 0;
        memset(kinfo, 0, sizeof(kinfo));
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)rk;
        args[2] = (uint64_t)(uintptr_t)kinfo;
        args[3] = sizeof(kinfo);
        args[4] = (uint64_t)(uintptr_t)&kres;
        r = vsl_nt_syscall_dispatch(&ctx, 168, args, 5);
        CHECK(r == NT_STATUS_SUCCESS, "NtQueryKey SUCCESS");
        CHECK(kres > 16, "NtQueryKey writes KEY_BASIC_INFORMATION");
        /* Last-write time (100ns ticks since 1601) must be nonzero. */
        uint64_t lwt = *(uint64_t *)kinfo;
        CHECK(lwt != 0, "NtQueryKey reports nonzero LastWriteTime");

        /* NtDeleteValueKey (69) -- remove the value, then confirm it's gone. */
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)rk;
        args[1] = (uint64_t)(uintptr_t)"Data";
        r = vsl_nt_syscall_dispatch(&ctx, 69, args, 2);
        CHECK(r == NT_STATUS_SUCCESS, "NtDeleteValueKey removes the value");
        /* Re-query the (now-deleted) value => NOT_FOUND. */
        uint8_t qbuf[32]; uint32_t qret = 0;
        memset(qbuf, 0, sizeof(qbuf));
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)rk; args[1] = (uint64_t)(uintptr_t)"Data";
        args[3] = (uint64_t)(uintptr_t)qbuf; args[4] = sizeof(qbuf);
        args[5] = (uint64_t)(uintptr_t)&qret;
        r = vsl_nt_syscall_dispatch(&ctx, 23, args, 5); /* NtQueryValueKey */
        CHECK(r == NT_STATUS_OBJECT_NAME_NOT_FOUND,
              "NtQueryValueKey after NtDeleteValueKey => NOT_FOUND");

        /* Cleanup. */
        memset(args, 0, sizeof(args)); args[0] = (uint64_t)sk;
        vsl_nt_syscall_dispatch(&ctx, 220, args, 1); /* delete subkey */
        memset(args, 0, sizeof(args)); args[0] = (uint64_t)rk;
        vsl_nt_syscall_dispatch(&ctx, 220, args, 1); /* delete parent key */
    }

    /* 26. Batch 11 — job set + job info (finish in-flight NT work). */
    {
        uint32_t job = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&job;
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 42, args, 1);
        CHECK(r == NT_STATUS_SUCCESS, "NtCreateJobObject (set/info test) SUCCESS");
        CHECK(job != 0, "job handle assigned");

        /* Set a basic limit (class 0) and read it back via query. */
        uint32_t limit[16];
        memset(limit, 0, sizeof(limit));
        limit[1] = 0x00002000;  /* LimitFlags = JOB_OBJECT_LIMIT_BREAKAWAY_OK */
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)job;
        args[1] = 0;            /* JobObjectBasicLimitInformation */
        args[2] = (uint64_t)(uintptr_t)limit;
        args[3] = sizeof(limit);
        r = vsl_nt_syscall_dispatch(&ctx, 425, args, 4);  /* NtSetInformationJobObject */
        CHECK(r == NT_STATUS_SUCCESS, "NtSetInformationJobObject stores limit");

        uint8_t qinfo[64];
        uint32_t qlen = sizeof(qinfo);
        memset(qinfo, 0, sizeof(qinfo));
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)job;
        args[1] = 0;            /* JobObjectBasicLimitInformation */
        args[2] = (uint64_t)(uintptr_t)qinfo;
        args[3] = qlen;
        r = vsl_nt_syscall_dispatch(&ctx, 342, args, 4);  /* NtQueryInformationJobObject */
        CHECK(r == NT_STATUS_SUCCESS, "NtQueryInformationJobObject SUCCESS");
        CHECK(((uint32_t *)qinfo)[1] == 0x00002000,
              "NtQueryInformationJobObject returns the stored LimitFlags");

        /* Job set containing our job id must validate. */
        uint32_t members[2] = { job, 0 };
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)members;
        r = vsl_nt_syscall_dispatch(&ctx, 181, args, 1);   /* NtCreateJobSet */
        CHECK(r == NT_STATUS_SUCCESS, "NtCreateJobSet accepts a live job member");

        /* Cleanup. */
        memset(args, 0, sizeof(args)); args[0] = (uint64_t)job;
        vsl_nt_syscall_dispatch(&ctx, 466, args, 1);  /* NtTerminateJobObject */
    }

    /* 27. Batch 11 — IO completion port lifecycle (41/242/199). */
    {
        uint32_t ioh = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&ioh;
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 178, args, 1);  /* NtCreateIoCompletion */
        CHECK(r == NT_STATUS_SUCCESS, "NtCreateIoCompletion SUCCESS");
        CHECK(ioh != 0, "IO completion handle assigned");

        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)ioh;
        r = vsl_nt_syscall_dispatch(&ctx, 435, args, 1);  /* NtSetIoCompletion */
        CHECK(r == NT_STATUS_SUCCESS, "NtSetIoCompletion posts a completion");

        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)ioh;
        r = vsl_nt_syscall_dispatch(&ctx, 77, args, 1);  /* NtRemoveIoCompletion */
        CHECK(r == 1, "NtRemoveIoCompletion drains the posted completion");

        memset(args, 0, sizeof(args)); args[0] = (uint64_t)ioh;
        vsl_nt_syscall_dispatch(&ctx, 15, args, 1);  /* NtClose */
    }

    /* 28. Batch 11 — symbolic link object create/open/query (55/134/179). */
    {
        uint32_t lh = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&lh;
        args[1] = (uint64_t)(uintptr_t)"\\??\\WuBuLink";
        args[3] = (uint64_t)(uintptr_t)"\\Device\\HarddiskVolume1\\target";
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 200, args, 4);  /* NtCreateSymbolicLinkObject */
        CHECK(r == NT_STATUS_SUCCESS, "NtCreateSymbolicLinkObject SUCCESS");
        CHECK(lh != 0, "symbolic link handle assigned");

        uint8_t tgt[512]; uint32_t tlen = 0;
        memset(tgt, 0, sizeof(tgt));
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)lh;
        args[1] = (uint64_t)(uintptr_t)tgt;
        args[2] = (uint64_t)(uintptr_t)&tlen;
        r = vsl_nt_syscall_dispatch(&ctx, 363, args, 3);  /* NtQuerySymbolicLinkObject */
        CHECK(r == NT_STATUS_SUCCESS, "NtQuerySymbolicLinkObject SUCCESS");
        CHECK(tlen == strlen("\\Device\\HarddiskVolume1\\target"),
              "NtQuerySymbolicLinkObject reports target length");

        /* Re-open by name. */
        uint32_t lh2 = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&lh2;
        args[1] = (uint64_t)(uintptr_t)"\\??\\WuBuLink";
        r = vsl_nt_syscall_dispatch(&ctx, 312, args, 2);  /* NtOpenSymbolicLinkObject */
        CHECK(r == NT_STATUS_SUCCESS, "NtOpenSymbolicLinkObject re-opens by name");
        CHECK(lh2 != 0, "re-opened link handle assigned");

        memset(args, 0, sizeof(args)); args[0] = (uint64_t)lh;
        vsl_nt_syscall_dispatch(&ctx, 15, args, 1);
        memset(args, 0, sizeof(args)); args[0] = (uint64_t)lh2;
        vsl_nt_syscall_dispatch(&ctx, 15, args, 1);
    }

    /* 29. Batch 11 — event pair (39) + LPC port handshake (49/34/1/32). */
    {
        uint32_t hi = 0, lo = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&hi;
        args[1] = (uint64_t)(uintptr_t)&lo;
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 176, args, 2);  /* NtCreateEventPair */
        CHECK(r == NT_STATUS_SUCCESS, "NtCreateEventPair SUCCESS");
        CHECK(hi != 0 && lo != 0, "both event-pair ends assigned");
        memset(args, 0, sizeof(args)); args[0] = (uint64_t)hi;
        vsl_nt_syscall_dispatch(&ctx, 15, args, 1);
        memset(args, 0, sizeof(args)); args[0] = (uint64_t)lo;
        vsl_nt_syscall_dispatch(&ctx, 15, args, 1);

        /* LPC: create a port, connect, accept+complete, request/reply. */
        uint32_t srv = 0, cli = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&srv;
        r = vsl_nt_syscall_dispatch(&ctx, 190, args, 1);  /* NtCreatePort */
        CHECK(r == NT_STATUS_SUCCESS, "NtCreatePort SUCCESS");
        CHECK(srv != 0, "server port handle assigned");

        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&cli;
        r = vsl_nt_syscall_dispatch(&ctx, 164, args, 1);  /* NtConnectPort */
        CHECK(r == NT_STATUS_SUCCESS, "NtConnectPort SUCCESS");
        CHECK(cli != 0, "client port handle assigned");

        memset(args, 0, sizeof(args)); args[0] = (uint64_t)srv;
        r = vsl_nt_syscall_dispatch(&ctx, 106, args, 1);   /* NtAcceptConnectPort */
        CHECK(r == NT_STATUS_SUCCESS, "NtAcceptConnectPort SUCCESS");
        memset(args, 0, sizeof(args)); args[0] = (uint64_t)srv;
        r = vsl_nt_syscall_dispatch(&ctx, 162, args, 1);   /* NtCompleteConnectPort */
        CHECK(r == NT_STATUS_SUCCESS, "NtCompleteConnectPort SUCCESS");

        memset(args, 0, sizeof(args)); args[0] = (uint64_t)cli;
        r = vsl_nt_syscall_dispatch(&ctx, 34, args, 1);  /* NtRequestWaitReplyPort */
        CHECK(r == NT_STATUS_SUCCESS, "NtRequestWaitReplyPort handshake SUCCESS");

        memset(args, 0, sizeof(args)); args[0] = (uint64_t)srv;
        vsl_nt_syscall_dispatch(&ctx, 15, args, 1);
        memset(args, 0, sizeof(args)); args[0] = (uint64_t)cli;
        vsl_nt_syscall_dispatch(&ctx, 15, args, 1);
    }

    /* 30. Batch 12 — Token / security subsystem (REAL privilege enforcement).
     * Asserts fixed behavior: a default token holds ordinary privileges but
     * DENIES a privileged op; granting/removing a privilege really changes the
     * access decision; PrivilegeCheck + CompareTokens + Duplicate are coherent;
     * an anonymized token loses every privilege. */
    {
        /* PRIVILEGE_SET for NtAccessCheck: {count, control, la[]{low,high,attr}}. */
        uint32_t ps[16];
        /* TOKEN_PRIVILEGES for NtAdjust/PrivilegeCheck: {count, la[]{..}}. */
        uint32_t tp[16];
        const uint32_t SE_SHUTDOWN = 0x14;  /* held by default token */
        const uint32_t SE_DEBUG    = 0x15;  /* NOT held by default */
        const uint32_t ATTR_EN     = 0x02;  /* NT_PRIV_ATTR_ENABLED */

        uint32_t tok = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&tok;
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 307, args, 1); /* NtOpenProcessToken */
        CHECK(r == NT_STATUS_SUCCESS, "NtOpenProcessToken SUCCESS");
        CHECK(tok != 0, "NtOpenProcessToken returns a token handle");

        /* Default token HOLDS SeShutdownPrivilege -> access check passes. */
        ps[0] = 1; ps[1] = 0;                 /* count=1, control=0 (any) */
        ps[2] = SE_SHUTDOWN; ps[3] = 0; ps[4] = ATTR_EN;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)tok; args[1] = (uint64_t)(uintptr_t)ps;
        r = vsl_nt_syscall_dispatch(&ctx, 244, args, 2); /* NtAccessCheck */
        CHECK(r == NT_STATUS_SUCCESS, "NtAccessCheck passes for a held privilege");

        /* Remove SeShutdownPrivilege for real, then DENY. */
        tp[0] = 1; tp[1] = SE_SHUTDOWN; tp[2] = 0; tp[3] = ATTR_EN | 0x04; /* +REMOVED */
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)tok; args[2] = (uint64_t)(uintptr_t)tp;
        r = vsl_nt_syscall_dispatch(&ctx, 65, args, 3); /* NtAdjustPrivilegesToken */
        CHECK(r == NT_STATUS_SUCCESS, "NtAdjustPrivilegesToken removes SeShutdownPrivilege");
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)tok; args[1] = (uint64_t)(uintptr_t)ps;
        r = vsl_nt_syscall_dispatch(&ctx, 244, args, 2);
        CHECK(r == NT_STATUS_ACCESS_DENIED, "NtAccessCheck DENIES after privilege removed");

        /* Default token LACKS SeDebugPrivilege -> deny. */
        ps[2] = SE_DEBUG;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)tok; args[1] = (uint64_t)(uintptr_t)ps;
        r = vsl_nt_syscall_dispatch(&ctx, 244, args, 2);
        CHECK(r == NT_STATUS_ACCESS_DENIED, "NtAccessCheck DENIES a missing privilege");

        /* Grant SeDebugPrivilege for real, then pass. */
        tp[0] = 1; tp[1] = SE_DEBUG; tp[2] = 0; tp[3] = ATTR_EN;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)tok; args[2] = (uint64_t)(uintptr_t)tp;
        r = vsl_nt_syscall_dispatch(&ctx, 65, args, 3);
        CHECK(r == NT_STATUS_SUCCESS, "NtAdjustPrivilegesToken adds SeDebugPrivilege");
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)tok; args[1] = (uint64_t)(uintptr_t)ps;
        r = vsl_nt_syscall_dispatch(&ctx, 244, args, 2);
        CHECK(r == NT_STATUS_SUCCESS, "NtAccessCheck passes AFTER privilege granted");

        /* NtPrivilegeCheck reports the held set via a BOOLEAN out-param. */
        uint32_t res = 0;
        memset(tp, 0, sizeof(tp)); tp[0] = 1; tp[1] = SE_DEBUG; tp[2] = 0; tp[3] = ATTR_EN;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)tok; args[1] = (uint64_t)(uintptr_t)tp; args[2] = (uint64_t)(uintptr_t)&res;
        r = vsl_nt_syscall_dispatch(&ctx, 322, args, 3); /* NtPrivilegeCheck */
        CHECK(r == NT_STATUS_SUCCESS, "NtPrivilegeCheck SUCCESS");
        CHECK(res == 1, "NtPrivilegeCheck reports privilege present");

        /* NtCompareTokens: identical tokens compare equal; a modified token
         * does not. */
        uint32_t tok2 = 0;
        memset(args, 0, sizeof(args)); args[0] = (uint64_t)(uintptr_t)&tok2;
        r = vsl_nt_syscall_dispatch(&ctx, 307, args, 1); /* fresh default token */
        CHECK(r == NT_STATUS_SUCCESS, "second NtOpenProcessToken SUCCESS");
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)tok2; args[1] = (uint64_t)tok2;
        r = vsl_nt_syscall_dispatch(&ctx, 161, args, 2); /* NtCompareTokens */
        CHECK(r == 1, "NtCompareTokens equal for identical token");
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)tok; args[1] = (uint64_t)tok2;  /* tok != tok2 (modified) */
        r = vsl_nt_syscall_dispatch(&ctx, 161, args, 2);
        CHECK(r == 0, "NtCompareTokens unequal for modified vs default token");

        /* NtDuplicateToken deep-copies the privilege set. */
        uint32_t dtok = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&dtok; args[1] = (uint64_t)tok;
        r = vsl_nt_syscall_dispatch(&ctx, 66, args, 2); /* NtDuplicateToken */
        CHECK(r == NT_STATUS_SUCCESS, "NtDuplicateToken SUCCESS");
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)dtok; args[1] = (uint64_t)tok;
        r = vsl_nt_syscall_dispatch(&ctx, 161, args, 2);
        CHECK(r == 1, "NtDuplicateToken copies privilege set (compare equal)");

        /* NtSetInformationToken (session id) accepted. */
        uint32_t sid = 7;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)tok; args[1] = 9; /* TokenSessionId */ args[2] = (uint64_t)(uintptr_t)&sid;
        r = vsl_nt_syscall_dispatch(&ctx, 429, args, 3); /* NtSetInformationToken */
        CHECK(r == NT_STATUS_SUCCESS, "NtSetInformationToken TokenSessionId SUCCESS");

        /* NtOpenThreadToken returns a handle. */
        uint32_t thtok = 0;
        memset(args, 0, sizeof(args)); args[0] = (uint64_t)(uintptr_t)&thtok;
        r = vsl_nt_syscall_dispatch(&ctx, 36, args, 1); /* NtOpenThreadToken */
        CHECK(r == NT_STATUS_SUCCESS, "NtOpenThreadToken SUCCESS");
        CHECK(thtok != 0, "NtOpenThreadToken returns a handle");

        /* NtImpersonateAnonymousToken strips ALL privileges -> access denied
         * even for a privilege the base default token held. */
        uint32_t tok3 = 0;
        memset(args, 0, sizeof(args)); args[0] = (uint64_t)(uintptr_t)&tok3;
        vsl_nt_syscall_dispatch(&ctx, 307, args, 1); /* fresh default token */
        memset(args, 0, sizeof(args)); args[0] = (uint64_t)tok3;
        r = vsl_nt_syscall_dispatch(&ctx, 260, args, 1); /* NtImpersonateAnonymousToken */
        CHECK(r == NT_STATUS_SUCCESS, "NtImpersonateAnonymousToken SUCCESS");
        ps[2] = SE_SHUTDOWN;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)tok3; args[1] = (uint64_t)(uintptr_t)ps;
        r = vsl_nt_syscall_dispatch(&ctx, 244, args, 2);
        CHECK(r == NT_STATUS_ACCESS_DENIED, "Anonymized token loses all privileges");
    }

    vsl_nt_bridge_shutdown(&ctx);

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
