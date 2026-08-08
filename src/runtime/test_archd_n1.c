/* test_archd_n1.c — BATTLESHIP N1 gate: shell injection is dead.
 *
 * N1 (🔴 security): wubu_archd ran user-controlled strings (package
 * names, service names) through /bin/sh -c — classic injection. The
 * fix replaces those with argv-based execv (run_argv/run_chroot_argv),
 * so metacharacters in user input are inert.
 *
 * This test proves it: an argument containing `; touch /tmp/n1_pwned`
 * must NOT create the file. Through the OLD run_cmd() it would; through
 * run_argv() it cannot, because the child execs the binary directly
 * with the string as a single argv element.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

/* forward decl — same signature as wubu_archd_util.c / wubu_arch.h */
extern int run_argv(const char *file, char *const argv[]);

static int failures = 0;
#define CHECK(c, msg) do { \
        if (c) printf("  [PASS] %s\n", msg); \
        else { printf("  [FAIL] %s\n", msg); failures++; } \
    } while (0)

static void rm_pwn(void) { remove("/tmp/n1_pwned"); }
static int pwn_exists(void) {
    struct stat st;
    return stat("/tmp/n1_pwned", &st) == 0;
}

int main(void) {
    printf("=== test_archd_n1 (shell injection) ===\n");

    /* 1. the attack: argument with shell metacharacters */
    rm_pwn();
    const char *evil = "echo x; touch /tmp/n1_pwned; echo y";
    char *argv[] = { (char *)"echo", (char *)evil, NULL };
    int rc = run_argv("/bin/echo", argv);
    CHECK(rc == 0, "run_argv executes the binary");
    CHECK(!pwn_exists(), "metacharacters are INERT (no /tmp/n1_pwned)");

    /* 2. command substitution attempt */
    rm_pwn();
    const char *evil2 = "$(touch /tmp/n1_pwned)";
    char *argv2[] = { (char *)"echo", (char *)evil2, NULL };
    run_argv("/bin/echo", argv2);
    CHECK(!pwn_exists(), "command substitution is INERT");

    /* 3. backtick attempt */
    rm_pwn();
    const char *evil3 = "`touch /tmp/n1_pwned`";
    char *argv3[] = { (char *)"echo", (char *)evil3, NULL };
    run_argv("/bin/echo", argv3);
    CHECK(!pwn_exists(), "backticks are INERT");

    /* 4. && chain attempt */
    rm_pwn();
    const char *evil4 = "echo && touch /tmp/n1_pwned";
    char *argv4[] = { (char *)"echo", (char *)evil4, NULL };
    run_argv("/bin/echo", argv4);
    CHECK(!pwn_exists(), "&& chain is INERT");

    /* 5. benign args pass through intact */
    {
        char *argv5[] = { (char *)"true", NULL };
        CHECK(run_argv("/bin/true", argv5) == 0, "benign argv passes");
    }

    /* 6. NULL/empty args rejected */
    char *null_argv[] = { NULL };
    CHECK(run_argv("/bin/echo", null_argv) == -1, "empty argv rejected");

    rm_pwn();
    if (failures) { printf("N1 GATE FAILED (%d)\n", failures); return 1; }
    printf("N1 GATE PASSED — shell injection is dead\n");
    return 0;
}
