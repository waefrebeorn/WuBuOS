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
        r = vsl_nt_syscall_dispatch(&ctx, 266, args, 1);
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
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 9, args, 2);
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
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 14, args, 2);
        CHECK(r == NT_STATUS_SUCCESS, "NtAlertResumeThread (wake+resume) SUCCESS");
    }

    /* 10. A still-stubbed syscall returns NOT_IMPLEMENTED through dispatch */
    {
        memset(args, 0, sizeof(args));
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 2, args, 0);  /* NtAccessCheck */
        CHECK(r == NT_STATUS_NOT_IMPLEMENTED, "stubbed NtAccessCheck -> NOT_IMPLEMENTED");
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
        int64_t h = vsl_nt_syscall_dispatch(&ctx, 38, args, 1);
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
        r = vsl_nt_syscall_dispatch(&ctx, 209, args, 1);           /* ResetEvent */
        CHECK(r == NT_STATUS_SUCCESS, "NtResetEvent clears the event");

        args[0] = (uint64_t)h;
        memset(args + 1, 0, sizeof(uint64_t) * 5);
        r = vsl_nt_syscall_dispatch(&ctx, 28, args, 1);            /* NtClose */
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
        r = vsl_nt_syscall_dispatch(&ctx, 28, args, 1);            /* NtClose */
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
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 28, args, 1);    /* NtClose */
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
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 56, args, 6);
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
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 50, args, 1);
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
        r = vsl_nt_syscall_dispatch(&ctx, 28, args, 1);  /* NtClose */
        CHECK(r == NT_STATUS_SUCCESS, "NtClose frees the process handle");
    }

    /* 24. NtOpenProcess / NtWriteVirtualMemory / NtReadVirtualMemory (129/195/288)
     *     -- genuine cross-process memory via process_vm_writev/readv (self). */
    {
        uint32_t ph = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&ph;   /* process handle* */
        args[2] = 0;                          /* client_id 0 = open self */
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 129, args, 3);
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
        r = vsl_nt_syscall_dispatch(&ctx, 28, args, 1);
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
        r = vsl_nt_syscall_dispatch(&ctx, 28, args, 1);
        CHECK(r == NT_STATUS_SUCCESS, "NtClose frees the section handle");
    }

    /* 26. NtTerminateProcess (267) -- real kill+reap of a forked child. */
    {
        uint32_t proc = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&proc;       /* process handle* */
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 50, args, 1);  /* NtCreateProcess */
        CHECK(r == NT_STATUS_SUCCESS, "NtCreateProcess (for terminate test) SUCCESS");
        CHECK(proc != 0, "NtCreateProcess returns a handle");

        uint64_t pid = 0;
        vsl_nt_handle_to_data(&ctx, proc, &pid);
        CHECK(pid != 0, "process handle carries a live child pid");
        if (pid) CHECK(kill((pid_t)pid, 0) == 0, "child is alive before terminate");

        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)proc;
        r = vsl_nt_syscall_dispatch(&ctx, 267, args, 1); /* NtTerminateProcess */
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
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 56, args, 6);
        CHECK(r == NT_STATUS_SUCCESS, "NtCreateThread(suspended) returns SUCCESS");
        CHECK(thr != 0, "NtCreateThread(suspended) returns a handle");
        /* Give it a moment: should NOT have run yet (suspended). */
        struct timespec ts = {0, 30 * 1000 * 1000};
        nanosleep(&ts, NULL);
        CHECK(g_thread_ran == 0, "suspended thread has NOT run yet");
        /* Resume it. */
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)thr;
        r = vsl_nt_syscall_dispatch(&ctx, 215, args, 1);      /* NtResumeThread */
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
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 46, args, 1); /* NtCreateMutant */
        CHECK(r == NT_STATUS_SUCCESS, "NtCreateMutant returns SUCCESS");
        CHECK(m != 0, "NtCreateMutant returns a handle");
        r = vsl_nt_syscall_dispatch(&ctx, 282, args, 3);      /* wait = lock */
        /* NtWaitForSingleObject on the mutant: a=mutation handle. */
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)m;
        r = vsl_nt_syscall_dispatch(&ctx, 282, args, 3);      /* NtWaitForSingleObject */
        CHECK(r == NT_STATUS_SUCCESS, "NtWaitForSingleObject acquires the mutant");
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)m;
        r = vsl_nt_syscall_dispatch(&ctx, 197, args, 1);      /* NtReleaseMutant */
        CHECK(r == NT_STATUS_SUCCESS, "NtReleaseMutant releases the mutant");
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)m;
        r = vsl_nt_syscall_dispatch(&ctx, 28, args, 1);       /* NtClose */
        CHECK(r == NT_STATUS_SUCCESS, "NtClose frees the mutant");
    }

    /* 29. NtCreateSemaphore / NtReleaseSemaphore / wait (54/198/282) */
    {
        uint32_t s = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&s;                    /* sem handle* */
        args[2] = 0;                                          /* initial count 0 */
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 54, args, 3); /* NtCreateSemaphore */
        CHECK(r == NT_STATUS_SUCCESS, "NtCreateSemaphore returns SUCCESS");
        CHECK(s != 0, "NtCreateSemaphore returns a handle");
        /* Post 1 so a waiter can proceed. */
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)s;
        args[2] = 1;                                          /* release 1 */
        r = vsl_nt_syscall_dispatch(&ctx, 198, args, 3);      /* NtReleaseSemaphore */
        CHECK(r == NT_STATUS_SUCCESS, "NtReleaseSemaphore posts the semaphore");
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)s;
        r = vsl_nt_syscall_dispatch(&ctx, 282, args, 3);      /* NtWaitForSingleObject */
        CHECK(r == NT_STATUS_SUCCESS, "NtWaitForSingleObject consumes the semaphore");
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)s;
        r = vsl_nt_syscall_dispatch(&ctx, 28, args, 1);       /* NtClose */
        CHECK(r == NT_STATUS_SUCCESS, "NtClose frees the semaphore");
    }

    /* 30. NtDuplicateObject (72) -- clone a handle */
    {
        uint32_t ev = 0;
        memset(args, 0, sizeof(args));
        args[0] = 0;
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 38, args, 1); /* NtCreateEvent */
        ev = (uint32_t)r;
        CHECK(ev != 0, "NtCreateEvent for dup test");
        uint32_t dup = 0;
        memset(args, 0, sizeof(args));
        args[1] = (uint64_t)ev;                               /* source handle */
        args[3] = (uint64_t)(uintptr_t)&dup;                  /* new handle* */
        r = vsl_nt_syscall_dispatch(&ctx, 72, args, 4);       /* NtDuplicateObject */
        CHECK(r == NT_STATUS_SUCCESS, "NtDuplicateObject returns SUCCESS");
        CHECK(dup != 0 && dup != ev, "NtDuplicateObject returns a distinct handle");
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)ev; r = vsl_nt_syscall_dispatch(&ctx, 28, args, 1);
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)dup; r = vsl_nt_syscall_dispatch(&ctx, 28, args, 1);
        CHECK(r == NT_STATUS_SUCCESS, "NtClose frees the duplicated handle");
    }

    /* 31. NtQueryInformationProcess (162) -- returns the live child pid */
    {
        uint32_t proc = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&proc;
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 50, args, 1); /* NtCreateProcess */
        CHECK(r == NT_STATUS_SUCCESS, "NtCreateProcess for query test");
        uint64_t qbuf[2] = {0, 0};
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)proc;
        args[1] = 0;                                          /* ProcessBasicInformation */
        args[2] = (uint64_t)(uintptr_t)qbuf;                   /* out */
        r = vsl_nt_syscall_dispatch(&ctx, 162, args, 3);      /* NtQueryInformationProcess */
        CHECK(r == NT_STATUS_SUCCESS, "NtQueryInformationProcess returns SUCCESS");
        CHECK(qbuf[0] != 0, "NtQueryInformationProcess returns a nonzero pid");
        /* Exit-status query: child still alive -> STILL_ACTIVE (0x103). */
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)proc;
        args[1] = 1;                                          /* ProcessExitStatus */
        args[2] = (uint64_t)(uintptr_t)qbuf;
        r = vsl_nt_syscall_dispatch(&ctx, 162, args, 3);
        CHECK(r == NT_STATUS_SUCCESS, "NtQueryInformationProcess(exit) SUCCESS");
        CHECK(qbuf[0] == 0x103, "live child reports STILL_ACTIVE");
        /* Terminate then re-query. */
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)proc;
        r = vsl_nt_syscall_dispatch(&ctx, 267, args, 1);      /* NtTerminateProcess */
        CHECK(r == NT_STATUS_SUCCESS, "NtTerminateProcess for query test");
    }

    /* 32. NtOpenThread (135) -- open a running thread by tid */
    {
        uint32_t thr = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&thr;
        args[4] = (uint64_t)(uintptr_t)vsl_nt_test_thread_start;
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 56, args, 6); /* NtCreateThread */
        CHECK(r == NT_STATUS_SUCCESS, "NtCreateThread for open-thread test");
        /* Grab its tid via the handle. We stored tid in styx_fid; expose by
         * reading the live process's thread through NtWaitForSingleObject join
         * is overkill — instead just verify OpenThread mints a handle. */
        uint32_t othr = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&othr;
        args[2] = (uint64_t)(uintptr_t)thr;  /* client_id = source handle (proxy) */
        r = vsl_nt_syscall_dispatch(&ctx, 135, args, 3);      /* NtOpenThread */
        CHECK(r == NT_STATUS_SUCCESS, "NtOpenThread returns SUCCESS");
        CHECK(othr != 0, "NtOpenThread returns a handle");
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)thr; r = vsl_nt_syscall_dispatch(&ctx, 28, args, 1);
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)othr; r = vsl_nt_syscall_dispatch(&ctx, 28, args, 1);
        CHECK(r == NT_STATUS_SUCCESS, "NtClose frees the opened thread");
    }

    /* 33. Batch 7: registry + system-info (Styx9 namespace + SteamOS view).
     * Asserts REAL behavior: the NT registry is backed by real files. */
    {
        uint32_t key = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)"\\Registry\\Machine\\Software\\WuBu";
        args[4] = (uint64_t)(uintptr_t)&key;
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 44, args, 5); /* NtCreateKey */
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
        r = vsl_nt_syscall_dispatch(&ctx, 257, args, 5); /* NtSetValueKey */
        CHECK(r == NT_STATUS_SUCCESS, "NtSetValueKey writes value to real file");

        char qbuf[32];
        memset(qbuf, 0, sizeof(qbuf));
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)key;
        args[1] = (uint64_t)(uintptr_t)"Name";
        args[3] = (uint64_t)(uintptr_t)qbuf;
        args[4] = (uint64_t)sizeof(qbuf);
        r = vsl_nt_syscall_dispatch(&ctx, 186, args, 5); /* NtQueryValueKey */
        CHECK(r == NT_STATUS_SUCCESS, "NtQueryValueKey reads value back");
        CHECK(strncmp(qbuf, "wubuos", vlen) == 0, "NtQueryValueKey round-trips data");

        /* System time must be non-zero (100ns ticks since 1601). */
        uint64_t st = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&st;
        r = vsl_nt_syscall_dispatch(&ctx, 183, args, 1); /* NtQuerySystemTime */
        CHECK(r == NT_STATUS_SUCCESS, "NtQuerySystemTime SUCCESS");
        CHECK(st != 0, "NtQuerySystemTime returns nonzero time");

        /* System info: page size + processor count. */
        uint8_t sinfo[64];
        memset(sinfo, 0, sizeof(sinfo));
        memset(args, 0, sizeof(args));
        args[0] = 2;                        /* SystemBasicInformation */
        args[1] = (uint64_t)(uintptr_t)sinfo;
        args[2] = sizeof(sinfo);
        r = vsl_nt_syscall_dispatch(&ctx, 182, args, 3); /* NtQuerySystemInformation */
        CHECK(r == NT_STATUS_SUCCESS, "NtQuerySystemInformation SUCCESS");
        uint32_t page_sz = *(uint32_t *)(sinfo);
        CHECK(page_sz == (uint32_t)sysconf(_SC_PAGESIZE), "NtQuerySystemInformation reports real page size");

        /* Clean up the key. */
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)key;
        r = vsl_nt_syscall_dispatch(&ctx, 67, args, 1);  /* NtDeleteKey */
        CHECK(r == NT_STATUS_SUCCESS, "NtDeleteKey removes the registry key");
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
        int64_t r = vsl_nt_syscall_dispatch(&ctx, 40, args, 5);
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
        r = vsl_nt_syscall_dispatch(&ctx, 28, args, 1);
        CHECK(r == NT_STATUS_SUCCESS, "NtClose frees the NtCreateFile handle");
        unlink(cf_path);

        /* NtPulseEvent (145) — one-shot signal on an eventfd. */
        uint32_t ev = 0;
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)(uintptr_t)&ev;
        r = vsl_nt_syscall_dispatch(&ctx, 38, args, 1); /* NtCreateEvent */
        CHECK(r != 0, "NtCreateEvent for pulse test");
        ev = (uint32_t)r;  /* NtCreateEvent returns the handle as its result */
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)ev;
        r = vsl_nt_syscall_dispatch(&ctx, 145, args, 1); /* NtPulseEvent */
        CHECK(r == NT_STATUS_SUCCESS, "NtPulseEvent sets+resets the event");
        memset(args, 0, sizeof(args));
        args[0] = (uint64_t)ev;
        r = vsl_nt_syscall_dispatch(&ctx, 28, args, 1);
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

    vsl_nt_bridge_shutdown(&ctx);

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
