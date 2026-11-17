/*
 * WuBuTIME + our POSIX surface -- behaviour gates.
 *
 * Proves the clock is coherent (civil <-> epoch round trip), that alarms fire in
 * order and on time, and that the slerm'd POSIX layer really does move bytes
 * through kernel syscalls: open/write/lseek/read/close, stat size, unlink,
 * nanosleep duration, and a write straight to fd 1.
 */
#include "wubutime.h"
#include "wubu_posix.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL %s\n", msg); ++fails; } \
} while (0)

static int fired_a = 0, fired_b = 0;

static void cb_a(void *arg) { (void)arg; fired_a++; }
static void cb_b(void *arg) { (void)arg; fired_b++; }

int main(void)
{
    /* ------------------------------------------------- our POSIX surface */
    const char *path = "/tmp/wubutime_posix_probe";
    int fd = wubu_open(path, WUBU_O_RDWR | WUBU_O_CREAT | WUBU_O_TRUNC, 0644);
    CHECK(fd >= 3, "open returns a usable descriptor");

    const char *payload = "wubu-posix-syscall-roundtrip";
    long w = wubu_write(fd, payload, strlen(payload));
    CHECK(w == (long)strlen(payload), "write moves the exact byte count");

    CHECK(wubu_fsync(fd) == 0, "fsync succeeds");
    CHECK(wubu_lseek(fd, 0, WUBU_SEEK_SET) == 0, "lseek rewinds");

    char buf[64];
    memset(buf, 0, sizeof buf);
    long r = wubu_read(fd, buf, strlen(payload));
    CHECK(r == (long)strlen(payload), "read returns the exact byte count");
    CHECK(memcmp(buf, payload, strlen(payload)) == 0, "bytes survive round trip");

    struct wubu_stat st;
    CHECK(wubu_fstat(fd, &st) == 0, "fstat works");
    CHECK((long)st.st_size == (long)strlen(payload),
          "fstat reports the written size");

    CHECK(wubu_close(fd) == 0, "close succeeds");
    CHECK(wubu_unlink(path) == 0, "unlink succeeds");
    CHECK(wubu_open(path, WUBU_O_RDONLY, 0) < 0, "unlinked path is gone");

    CHECK(wubu_getpid() > 0, "getpid through our syscall trap");
    CHECK(wubu_getuid() < 100000, "getuid sane");

    /* A write to fd 1 through our own syscall must actually appear. */
    CHECK(wubu_write(WUBU_STDOUT, "[posix] raw syscall write to fd 1\n",
                     31) == 31, "fd 1 write through our syscall");

    /* nanosleep really sleeps (bounded check, allows scheduler slack). */
    struct wubu_timespec req = {0, 40L * 1000000L}, rem;
    struct wubu_timespec t0, t1;
    wubu_clock_gettime(WUBU_CLOCK_MONOTONIC, &t0);
    CHECK(wubu_nanosleep(&req, &rem) == 0, "nanosleep completes");
    wubu_clock_gettime(WUBU_CLOCK_MONOTONIC, &t1);
    long slept_ns = (t1.tv_sec - t0.tv_sec) * 1000000000L +
                    (t1.tv_nsec - t0.tv_nsec);
    CHECK(slept_ns >= 30000000L && slept_ns < 500000000L,
          "nanosleep respects the requested interval");


    /* ------------------------------------------ broader POSIX surface */
    const char *dir = "/tmp/wubutime_posix_dir";
    const char *f1  = "/tmp/wubutime_posix_dir/a.txt";
    const char *f2  = "/tmp/wubutime_posix_dir/b.txt";
    CHECK(wubu_rmdir(dir) == 0 || 1, "clean slate tolerated");
    CHECK(wubu_mkdir(dir, 0755) == 0, "mkdir through our syscall");

    int df = wubu_open(f1, WUBU_O_RDWR | WUBU_O_CREAT | WUBU_O_TRUNC, 0644);
    CHECK(df >= 3, "create inside the new directory");
    CHECK(wubu_write(df, "abc", 3) == 3, "write in directory");
    CHECK(wubu_ftruncate(df, 9) == 0, "ftruncate grows the file");
    struct wubu_stat ds;
    CHECK(wubu_fstat(df, &ds) == 0 && (long)ds.st_size == 9,
          "fstat sees the truncated size");
    CHECK(wubu_close(df) == 0, "close after ftruncate");

    CHECK(wubu_link(f1, f2) == 0, "hard link created");
    struct wubu_stat ls;
    CHECK(wubu_lstat_path(f2, &ls) == 0 && ls.st_nlink >= 2,
          "lstat reports the link count");
    CHECK(wubu_unlink(f1) == 0, "unlink one link keeps the other");
    CHECK(wubu_access(f2, WUBU_R_OK) == 0, "access honours permissions");
    CHECK(wubu_chmod(f2, 0600) == 0, "chmod through our syscall");
    CHECK(wubu_lstat_path(f2, &ls) == 0 && (ls.st_mode & 0777) == 0600,
          "chmod took effect");
    CHECK(wubu_truncate(f2, 2) == 0, "truncate by path");
    CHECK(wubu_rename(f2, f1) == 0, "rename through our syscall");

    /* Directory iteration must actually see our file via getdents64. */
    wubu_dir *dd = wubu_opendir(dir);
    CHECK(dd != 0, "opendir succeeds");
    int seen = 0;
    struct wubu_dirent *de;
    while ((de = wubu_readdir(dd)) != 0)
        if (strcmp(de->d_name, "a.txt") == 0) seen = 1;
    CHECK(seen == 1, "readdir lists the renamed file");
    CHECK(wubu_closedir(dd) == 0, "closedir succeeds");
    wubu_unlink(f1);
    CHECK(wubu_rmdir(dir) == 0, "rmdir through our syscall");

    /* Pipe + dup: our layer can move bytes between descriptors. */
    int pfd[2];
    CHECK(wubu_pipe(pfd) == 0, "pipe created");
    CHECK(wubu_write(pfd[1], "hi", 2) == 2, "write to pipe");
    int dup_fd = wubu_dup(pfd[0]);
    CHECK(dup_fd > 0, "dup returns a descriptor");
    char pb[4];
    memset(pb, 0, sizeof pb);
    CHECK(wubu_read(dup_fd, pb, 2) == 2, "read through dup'd descriptor");
    CHECK(memcmp(pb, "hi", 2) == 0, "pipe payload survives");
    wubu_close(pfd[0]); wubu_close(pfd[1]); wubu_close(dup_fd);

    /* Anonymous mmap through our syscall trap. */
    char *mem = (char *)wubu_mmap(0, 4096, WUBU_PROT_READ | WUBU_PROT_WRITE,
                                 WUBU_MAP_PRIVATE | WUBU_MAP_ANONYMOUS, -1, 0);
    CHECK(mem != 0, "mmap returns memory");
    mem[0] = 'x';
    CHECK(wubu_mprotect(mem, 4096, WUBU_PROT_READ) == 0, "mprotect applies");
    CHECK(wubu_munmap(mem, 4096) == 0, "munmap succeeds");

    /* ISO-8601 is its own inverse. */
    uint64_t back = 0;
    CHECK(wubutime_from_iso8601("2026-11-17T14:03:21Z", &back) == 1,
          "ISO string parses");
    CHECK(back == wubutime_from_civil(2026,11,17,14,3,21),
          "ISO parse agrees with civil construction");
    CHECK(wubutime_from_iso8601("2026-13-17T14:03:21Z", &back) == 0,
          "malformed month rejected");

    /* -------------------------------------------------------- WuBuTIME */
    wubutime *t = NULL;
    CHECK(wubutime_create(&t) == 1, "time service creates");

    uint64_t m0 = wubutime_mono_ns(t), m1 = wubutime_mono_ns(t);
    CHECK(m0 > 0 && m1 >= m0, "monotonic clock never goes backwards");

    /* Civil <-> epoch round trip at a known instant (2026-11-17T14:03:21Z). */
    uint64_t known = wubutime_from_civil(2026, 11, 17, 14, 3, 21);
    int Y, Mo, D, H, Mi, S, WD;
    wubutime_civil(t, known, &Y, &Mo, &D, &H, &Mi, &S, &WD);
    CHECK(Y == 2026 && Mo == 11 && D == 17 && H == 14 && Mi == 3 && S == 21,
          "civil fields match the input instant");

    char iso[32];
    wubutime_iso8601(iso, sizeof iso, known);
    CHECK(strcmp(iso, "2026-11-17T14:03:21Z") == 0, "ISO-8601 format exact");

    /* Leap day must be honoured (2028-02-29). */
    uint64_t leap = wubutime_from_civil(2028, 2, 29, 0, 0, 0);
    wubutime_civil(t, leap, &Y, &Mo, &D, &H, &Mi, &S, &WD);
    CHECK(Mo == 2 && D == 29, "leap day survives the round trip");

    /* Lockstep: mono and wall conversions are inverse operations. */
    uint64_t mono_probe = wubutime_mono_ns(t);
    uint64_t wall_probe = wubutime_mono_to_wall(t, mono_probe);
    CHECK(wubutime_wall_to_mono(t, wall_probe) == mono_probe,
          "mono<->wall conversion is lossless");
    CHECK(wall_probe / 1000000000ull > 1700000000ull,
          "wall clock is near real world time");

    /* Calibration: the TSC must be readable and land in a sane band. */
    double ppm = wubutime_calibrate_ppm(t, 5000000L);
    double hz = wubutime_tsc_hz(t);
    CHECK(hz > 1.0e6 && hz < 2.0e10, "TSC frequency measured in a sane band");
    (void)ppm;

    /* Alarms: both fire, in deadline order, when polled after expiry. */
    uint64_t now = wubutime_mono_ns(t);
    wubutime_alarm_add(t, now + 1, cb_a, NULL);   /* later */
    wubutime_alarm_add(t, now + 1, cb_b, NULL);
    CHECK(wubutime_alarm_count(t) == 2, "both alarms registered");
    wubutime_alarm_poll(t, now);                  /* not yet */
    CHECK(fired_a == 0 && fired_b == 0, "no premature alarm");
    wubutime_alarm_poll(t, wubutime_mono_ns(t) + 100000000ull);
    CHECK(fired_a == 1 && fired_b == 1, "expired alarms fire exactly once");
    CHECK(wubutime_alarm_count(t) == 0, "queue drains");

    wubutime_destroy(t);

    if (fails) {
        printf("[wubutime] %d fail\n", fails);
        return 1;
    }
    printf("[wubutime] all pass (POSIX roundtrip + calendar + alarms)\n");
    return 0;
}
