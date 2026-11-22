/* wubu_wemu_bridge_test.c — L14 acceptance gate: REAL static busybox
 * applets run through WEMU (interpreter sandbox) + the VSL Linux
 * syscall table, and the gate asserts EXACT output / exit codes —
 * the QEMU project's own user-mode smoke standard, never "non-crash".
 *
 * Applets: echo, uname, cat, ls, true, false. */
#include "wubu_wemu_bridge.h"
#include "wubu_vsl.h"
#include "vsl/vsl_driver.h"
#include "vsl/vsl_file.h"
#include "wubu_drv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

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

    /* ---- wave 2: broader applet surface (devil's-advocate expansion).
     * Each failure here is a discovered syscall/instruction gap. File
     * applets get EXACT oracles; checksum/base64 applets are compared
     * against the HOST coreutils output (independent oracle). ---- */
    {
        char fx[] = "/tmp/wemu_fx_XXXXXX";
        int fd = mkstemp(fx);
        if (fd < 0) FAIL("mkstemp fx\n");
        const char *data = "delta\nalpha\ncharlie\nbravo\nalpha\n";
        if (write(fd, data, strlen(data)) != (ssize_t)strlen(data)) FAIL("write fx\n");
        close(fd);

        struct { char *argv[8]; const char *want; } cases[] = {
            { {"busybox","head","-n","2",fx,NULL}, "delta\nalpha\n" },
            { {"busybox","tail","-n","1",fx,NULL}, "alpha\n" },
            { {"busybox","grep","alpha",fx,NULL}, "alpha\nalpha\n" },
            { {"busybox","sort",fx,NULL}, "alpha\nalpha\nbravo\ncharlie\ndelta\n" },
            { {"busybox","sed","s/alpha/ALPHA/",fx,NULL}, "delta\nALPHA\ncharlie\nbravo\nALPHA\n" },
            { {"busybox","seq","1","5",NULL}, "1\n2\n3\n4\n5\n" },
            { {"busybox","expr","3","+","4",NULL}, "7\n" },
        };
        for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
            run_applet(cases[i].argv, &cap);
            report(cases[i].argv[1], &cap);
            if (cap.exit_code != 0) FAIL("%s exit %d\n", cases[i].argv[1], cap.exit_code);
            if (!cap.out || strcmp(cap.out, cases[i].want) != 0)
                FAIL("%s stdout=[%s] want [%s]\n", cases[i].argv[1],
                     cap.out ? cap.out : "(null)", cases[i].want);
            printf("ok: busybox %s -> exact\n", cases[i].argv[1]);
            wubu_wemu_capture_free(&cap);
        }

        /* wc -l: count prefix (path formatting varies) */
        {
            char *argv[] = { "busybox","wc","-l",fx,NULL };
            run_applet(argv, &cap); report("wc", &cap);
            if (cap.exit_code != 0 || !cap.out || strncmp(cap.out, "5 ", 2) != 0)
                FAIL("wc -l want '5 ...' got [%s]\n", cap.out ? cap.out : "(null)");
            printf("ok: busybox wc -l -> 5\n");
            wubu_wemu_capture_free(&cap);
        }
        /* cut + awk on their own fixtures */
        {
            char cf[] = "/tmp/wemu_cut_XXXXXX";
            fd = mkstemp(cf);
            if (fd < 0) FAIL("mkstemp cut\n");
            if (write(fd, "a:b:c\nd:e:f\n", 10) != 10) FAIL("write cut\n");
            close(fd);
            char *argv[] = { "busybox","cut","-d:","-f1",cf,NULL };
            run_applet(argv, &cap); report("cut", &cap);
            if (cap.exit_code != 0 || !cap.out || strcmp(cap.out, "a\nd\n") != 0)
                FAIL("cut got [%s]\n", cap.out ? cap.out : "(null)");
            printf("ok: busybox cut -> exact\n");
            wubu_wemu_capture_free(&cap);
            unlink(cf);
        }
        {
            char af[] = "/tmp/wemu_awk_XXXXXX";
            fd = mkstemp(af);
            if (fd < 0) FAIL("mkstemp awk\n");
            if (write(fd, "one two\nthree four\n", 17) != 17) FAIL("write awk\n");
            close(fd);
            char *argv[] = { "busybox","awk","{print $1}",af,NULL };
            run_applet(argv, &cap); report("awk", &cap);
            if (cap.exit_code != 0 || !cap.out || strcmp(cap.out, "one\nthree\n") != 0)
                FAIL("awk got [%s]\n", cap.out ? cap.out : "(null)");
            printf("ok: busybox awk -> exact\n");
            wubu_wemu_capture_free(&cap);
            unlink(af);
        }
        /* host-oracle applets: md5sum / sha256sum / base64 */
        {
            const char *tools[] = { "md5sum", "sha256sum", "base64" };
            for (int t = 0; t < 3; t++) {
                char cmd[512], want[4096];
                snprintf(cmd, sizeof cmd, "%s %s", tools[t], fx);
                FILE *p = popen(cmd, "r");
                if (!p) FAIL("popen %s\n", tools[t]);
                size_t wn = fread(want, 1, sizeof want - 1, p);
                want[wn] = 0; pclose(p);
                char *argv[] = { "busybox", (char *)tools[t], fx, NULL };
                run_applet(argv, &cap); report(tools[t], &cap);
                if (cap.exit_code != 0 || !cap.out || strcmp(cap.out, want) != 0)
                    FAIL("%s mismatch:\n got [%s]\nwant [%s]\n", tools[t],
                         cap.out ? cap.out : "(null)", want);
                printf("ok: busybox %s == host coreutils\n", tools[t]);
                wubu_wemu_capture_free(&cap);
            }
        }
        /* filesystem mutation round-trip: mkdir/touch/cp/mv/rm */
        {
            char dir[] = "/tmp/wemu_fs_XXXXXX";
            if (!mkdtemp(dir)) FAIL("mkdtemp fs\n");
            char f1[300], f2[300], f3[300];
            snprintf(f1, sizeof f1, "%s/f", dir);
            snprintf(f2, sizeof f2, "%s/g", dir);
            snprintf(f3, sizeof f3, "%s/h", dir);
            char *mk[] = { "busybox","touch",f1,NULL };
            run_applet(mk, &cap); report("touch", &cap);
            if (cap.exit_code != 0 || access(f1, F_OK) != 0) FAIL("touch\n");
            wubu_wemu_capture_free(&cap);
            char *cp[] = { "busybox","cp",f1,f2,NULL };
            run_applet(cp, &cap); report("cp", &cap);
            if (cap.exit_code != 0 || access(f2, F_OK) != 0) FAIL("cp\n");
            wubu_wemu_capture_free(&cap);
            char *mv[] = { "busybox","mv",f2,f3,NULL };
            run_applet(mv, &cap); report("mv", &cap);
            if (cap.exit_code != 0 || access(f3, F_OK) != 0 || access(f2, F_OK) == 0)
                FAIL("mv\n");
            wubu_wemu_capture_free(&cap);
            char *rm[] = { "busybox","rm",f1,f3,NULL };
            run_applet(rm, &cap); report("rm", &cap);
            if (cap.exit_code != 0 || access(f1, F_OK) == 0 || access(f3, F_OK) == 0)
                FAIL("rm\n");
            wubu_wemu_capture_free(&cap);
            rmdir(dir);
            printf("ok: busybox touch/cp/mv/rm round-trip\n");
        }
        /* pwd / id / sleep / test / date / hostname: exit + shape */
        {
            char *argv[] = { "busybox","pwd",NULL };
            run_applet(argv, &cap); report("pwd", &cap);
            char cwd[512];
            if (cap.exit_code != 0 || !cap.out || !getcwd(cwd, sizeof cwd) ||
                strncmp(cap.out, cwd, strlen(cwd)) != 0)
                FAIL("pwd got [%s] want [%s]\n", cap.out ? cap.out : "(null)", cwd);
            printf("ok: busybox pwd -> cwd\n");
            wubu_wemu_capture_free(&cap);
        }
        {
            char *argv[] = { "busybox","id",NULL };
            run_applet(argv, &cap); report("id", &cap);
            if (cap.exit_code != 0 || !cap.out || !strstr(cap.out, "uid="))
                FAIL("id got [%s]\n", cap.out ? cap.out : "(null)");
            printf("ok: busybox id -> uid=...\n");
            wubu_wemu_capture_free(&cap);
        }
        {
            char *argv[] = { "busybox","sleep","0",NULL };
            run_applet(argv, &cap); report("sleep", &cap);
            if (cap.exit_code != 0) FAIL("sleep exit %d\n", cap.exit_code);
            printf("ok: busybox sleep 0\n");
            wubu_wemu_capture_free(&cap);
        }
        {
            char *argv[] = { "busybox","test","-f",fx,NULL };
            run_applet(argv, &cap); report("test -f", &cap);
            if (cap.exit_code != 0) FAIL("test -f existing exit %d\n", cap.exit_code);
            wubu_wemu_capture_free(&cap);
            char *argv2[] = { "busybox","test","-f","/nonexistent-wemu",NULL };
            run_applet(argv2, &cap); report("test -f missing", &cap);
            if (cap.exit_code != 1) FAIL("test -f missing exit %d want 1\n", cap.exit_code);
            wubu_wemu_capture_free(&cap);
            printf("ok: busybox test -f -> 0/1\n");
        }
        {
            char *argv[] = { "busybox","date",NULL };
            run_applet(argv, &cap); report("date", &cap);
            /* shape is not enough: epoch-0 passed the old check. Assert
             * the CURRENT year — gettimeofday/time must return real time. */
            time_t now = time(NULL);
            struct tm tmv;
            localtime_r(&now, &tmv);
            char year[16];
            snprintf(year, sizeof year, "%d", tmv.tm_year + 1900);
            if (cap.exit_code != 0 || !cap.out || !strstr(cap.out, year))
                FAIL("date exit %d out [%s] want year %s\n", cap.exit_code,
                     cap.out ? cap.out : "(null)", year);
            printf("ok: busybox date -> %s", cap.out);
            wubu_wemu_capture_free(&cap);
        }
        {
            char *argv[] = { "busybox","hostname",NULL };
            run_applet(argv, &cap); report("hostname", &cap);
            if (cap.exit_code != 0 || !cap.out || cap.out_len < 1)
                FAIL("hostname exit %d\n", cap.exit_code);
            printf("ok: busybox hostname -> %s", cap.out);
            wubu_wemu_capture_free(&cap);
        }
        unlink(fx);
    }

    /* ---- D4: the guest can read the VSL driver inventory ---- */
    {
        if (vsl_init() != 0) FAIL("D4: vsl_init\n");

        /* D4b: publish the REAL OS driver registry summary as the host
         * section, then verify a WEMU guest can read it via cat. */
        wubu_drv_init();
        wubu_drv_dev_t hd;
        memset(&hd, 0, sizeof(hd));
        hd.bus = WUBU_DRV_BUS_PCI;
        hd.vendor = 0x144d; hd.device = 0xa80a;
        hd.class_code = 0x01; hd.subclass = 0x08;
        hd.bar0 = 0xFC000000ULL;
        if (wubu_drv_add_device(&hd) < 0) FAIL("D4b: add host device\n");
        wubu_drv_probe();
        char host_summary[2048];
        if (wubu_drv_summary(host_summary, sizeof(host_summary)) != 1)
            FAIL("D4b: host summary line count\n");
        vsl_driver_inventory_set_host(host_summary);

        int id = vsl_register_driver(VSL_DRV_BLOCK, 0x1000,
                                     0x20000000, 0x1000, 11);
        if (id < 0) FAIL("D4: vsl_register_driver\n");
        char *argv[] = { "busybox", "cat", "/proc/wubu/drivers", NULL };
        run_applet(argv, &cap);
        report("cat-drivers", &cap);
        if (cap.exit_code != 0)
            FAIL("D4: cat exit %d\n", cap.exit_code);
        if (!cap.out || !strstr(cap.out, "type=BLOCK"))
            FAIL("D4: inventory=[%s] missing VSL driver\n",
                 cap.out ? cap.out : "(null)");
        if (!strstr(cap.out, "[drv] 144d:a80a") ||
            !strstr(cap.out, "-> nvme"))
            FAIL("D4b: guest cannot see the host driver summary: [%s]\n",
                 cap.out ? cap.out : "(null)");
        printf("ok: WEMU guest reads VSL driver + host registry inventory\n");
        wubu_wemu_capture_free(&cap);

        /* Differential: clear the host section; the VSL line remains. */
        vsl_driver_inventory_set_host(NULL);
        {
            char *argv2[] = { "busybox", "cat", "/proc/wubu/drivers", NULL };
            run_applet(argv2, &cap);
            if (cap.exit_code != 0 || !cap.out) FAIL("D4b: second cat\n");
            if (strstr(cap.out, "[drv] 144d:a80a"))
                FAIL("D4b: host section persisted after clear\n");
            if (!strstr(cap.out, "type=BLOCK"))
                FAIL("D4b: VSL section lost after host clear\n");
            printf("ok: host section clears, VSL driver stays\n");
            wubu_wemu_capture_free(&cap);
        }
    }

    printf("WEMU BUSYBOX GATE PASS (real static applets, exact output)\n");
    return 0;
}