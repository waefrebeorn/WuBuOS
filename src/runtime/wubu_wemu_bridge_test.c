/* wubu_wemu_bridge_test.c — L14 acceptance gate: REAL static busybox
 * applets run through WEMU (interpreter sandbox) + the VSL Linux
 * syscall table, and the gate asserts EXACT output / exit codes —
 * the QEMU project's own user-mode smoke standard, never "non-crash".
 *
 * Applets: echo, uname, cat, ls, true, false. */
#include "wubu_wemu_bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define FAIL(...) do { fprintf(stderr, "FAIL: " __VA_ARGS__); exit(1); } while (0)

static const char *busybox(void) {
    const char *p = getenv("WEMU_BUSYBOX");
    return p ? p : "/home/wubu/opt/busybox-static/usr/bin/busybox";
}

static void report(const char *applet, wubu_wemu_capture *cap) {
    printf("  [%s] exit=%d insns=%llu syscalls=%llu",
           applet, cap->exit_code,
           (unsigned long long)cap->insns,
           (unsigned long long)cap->syscalls);
    if (cap->n_missing) {
        printf(" ENOSYS-nrs:");
        for (size_t i = 0; i < cap->n_missing; i++)
            printf(" %llu", (unsigned long long)cap->missing_nrs[i]);
    }
    printf("\n");
    if (cap->fault)
        printf("  fault vec=%d rip=%llx\n", cap->fault,
               (unsigned long long)cap->fault_rip);
    if (cap->err_len)
        printf("  stderr: %s", cap->err);
}

static void run_applet(char *const argv[], wubu_wemu_capture *cap) {
    if (wubu_wemu_run_elf(busybox(), argv, cap) != 0) {
        report(argv[1], cap);
        FAIL("applet %s did not exit cleanly (fault=%d rip=%llx)\n",
             argv[1], cap->fault, (unsigned long long)cap->fault_rip);
    }
}

int main(void) {
    wubu_wemu_capture cap;

    /* ---- echo: exact stdout ---- */
    {
        char *argv[] = { "busybox", "echo", "hello-wemu", NULL };
        run_applet(argv, &cap);
        report("echo", &cap);
        if (cap.exit_code != 0) FAIL("echo exit %d\n", cap.exit_code);
        if (!cap.out || strcmp(cap.out, "hello-wemu\n") != 0)
            FAIL("echo stdout=[%s] want [hello-wemu\\n]\n",
                 cap.out ? cap.out : "(null)");
        printf("ok: busybox echo hello-wemu -> exact stdout\n");
        wubu_wemu_capture_free(&cap);
    }

    /* ---- uname: asserts Linux ---- */
    {
        char *argv[] = { "busybox", "uname", NULL };
        run_applet(argv, &cap);
        report("uname", &cap);
        if (cap.exit_code != 0) FAIL("uname exit %d\n", cap.exit_code);
        if (!cap.out || strncmp(cap.out, "Linux", 5) != 0)
            FAIL("uname stdout=[%s] want Linux...\n",
                 cap.out ? cap.out : "(null)");
        printf("ok: busybox uname -> %s", cap.out);
        wubu_wemu_capture_free(&cap);
    }

    /* ---- cat: exact file content ---- */
    {
        char tmpl[] = "/tmp/wemu_cat_XXXXXX";
        int fd = mkstemp(tmpl);
        if (fd < 0) FAIL("mkstemp\n");
        const char *content = "line one\nline two\ttabbed\n\x01\x02 binary-ish\n";
        size_t n = strlen(content);
        if (write(fd, content, n) != (ssize_t)n) FAIL("write temp\n");
        close(fd);
        char *argv[] = { "busybox", "cat", tmpl, NULL };
        run_applet(argv, &cap);
        report("cat", &cap);
        unlink(tmpl);
        if (cap.exit_code != 0) FAIL("cat exit %d\n", cap.exit_code);
        if (!cap.out || cap.out_len != n || memcmp(cap.out, content, n) != 0)
            FAIL("cat content mismatch (%zu bytes)\n", cap.out_len);
        printf("ok: busybox cat <file> -> exact %zu-byte content\n", n);
        wubu_wemu_capture_free(&cap);
    }

    /* ---- ls: directory entries ---- */
    {
        char dir[] = "/tmp/wemu_ls_XXXXXX";
        if (!mkdtemp(dir)) FAIL("mkdtemp\n");
        const char *names[] = { "alpha.txt", "beta.txt", "gamma" };
        for (int i = 0; i < 3; i++) {
            char p[256];
            snprintf(p, sizeof p, "%s/%s", dir, names[i]);
            FILE *f = fopen(p, "w");
            if (!f) FAIL("create %s\n", p);
            fputs("x", f);
            fclose(f);
        }
        char *argv[] = { "busybox", "ls", dir, NULL };
        run_applet(argv, &cap);
        report("ls", &cap);
        for (int i = 0; i < 3; i++) {
            char p[256];
            snprintf(p, sizeof p, "%s/%s", dir, names[i]);
            unlink(p);
        }
        rmdir(dir);
        if (cap.exit_code != 0) FAIL("ls exit %d\n", cap.exit_code);
        for (int i = 0; i < 3; i++) {
            char line[64];
            snprintf(line, sizeof line, "%s\n", names[i]);
            if (!cap.out || !strstr(cap.out, line))
                FAIL("ls missing entry %s in [%s]\n", names[i],
                     cap.out ? cap.out : "(null)");
        }
        printf("ok: busybox ls <dir> -> entries alpha.txt beta.txt gamma\n");
        wubu_wemu_capture_free(&cap);
    }

    /* ---- true / false: exit codes ---- */
    {
        char *argv_t[] = { "busybox", "true", NULL };
        run_applet(argv_t, &cap);
        report("true", &cap);
        if (cap.exit_code != 0) FAIL("true exit %d\n", cap.exit_code);
        wubu_wemu_capture_free(&cap);
        char *argv_f[] = { "busybox", "false", NULL };
        run_applet(argv_f, &cap);
        report("false", &cap);
        if (cap.exit_code != 1) FAIL("false exit %d want 1\n", cap.exit_code);
        wubu_wemu_capture_free(&cap);
        printf("ok: busybox true -> 0, busybox false -> 1\n");
    }

    printf("WEMU BUSYBOX GATE PASS (real static applets, exact output)\n");
    return 0;
}