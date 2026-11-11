/* test_kernel_xchg.c -- the kernel interchange network.
 *
 * Three fake kernels (linux/nt/macos) with disjoint syscall ranges and
 * payload formats, plus a schema-mismatched one, prove the registry, the
 * fail-closed contract, the fallthrough router, the swap, and the log.
 */
#include <stdio.h>
#include <string.h>
#include "wubu_kernel_xchg.h"

static int failures = 0;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

/* -- fake kernels --------------------------------------------------- */

static int linux_dispatch(uint64_t num, uint64_t *regs, int64_t *out)
{
    (void)regs;
    if (num >= 1 && num <= 3) { *out = (int64_t)num * 10; return 1; }
    return 0;
}
static int nt_dispatch(uint64_t num, uint64_t *regs, int64_t *out)
{
    (void)regs;
    if (num >= 100 && num <= 102) { *out = (int64_t)num + 1000; return 1; }
    return 0;
}
static int macos_dispatch(uint64_t num, uint64_t *regs, int64_t *out)
{
    (void)regs;
    if (num >= 200 && num <= 202) { *out = (int64_t)num + 2000; return 1; }
    return 0;
}
static int bad_dispatch(uint64_t num, uint64_t *regs, int64_t *out)
{
    (void)regs;
    if (num == 300) { *out = 300; return 1; }
    return 0;
}

static int linux_probe(const char *p, const uint8_t *h, size_t n)
{
    if (p && strstr(p, ".elf")) return 1;
    return (n >= 4 && h && h[0] == 0x7f && h[1] == 'E' && h[2] == 'L' && h[3] == 'F');
}
static int nt_probe(const char *p, const uint8_t *h, size_t n)
{
    if (p && strstr(p, ".exe")) return 1;
    return (n >= 2 && h && h[0] == 'M' && h[1] == 'Z');
}
static int macos_probe(const char *p, const uint8_t *h, size_t n)
{
    if (p && strstr(p, ".macho")) return 1;
    return (n >= 4 && h && h[0] == 0xcf && h[1] == 0xfa && h[2] == 0xed && h[3] == 0xfe);
}
static int exec_ok(const char *path, int argc, char *const argv[])
{
    (void)path; (void)argc; (void)argv;
    return 0;
}

static const wubu_kernel_ops_t k_linux = {
    "linux", 1, 1, "elf64", linux_dispatch, linux_probe, exec_ok
};
static const wubu_kernel_ops_t k_nt = {
    "nt", 1, 1, "pe", nt_dispatch, nt_probe, exec_ok
};
static const wubu_kernel_ops_t k_macos = {
    "macos", 1, 1, "macho", macos_dispatch, macos_probe, exec_ok
};
/* Schema 99 against our schema 1: must be REFUSED, never silently accepted. */
static const wubu_kernel_ops_t k_bad = {
    "bad", 1, 99, "weird", bad_dispatch, NULL, NULL
};
/* Emits no trace at all: acceptable under any schema. */
static const wubu_kernel_ops_t k_holyd = {
    "holyd", 1, 0, "holyc", NULL, NULL, exec_ok
};
/* Can neither dispatch, probe nor exec: inert, must not enter the network. */
static const wubu_kernel_ops_t k_inert = {
    "inert", 1, 1, "none", NULL, NULL, NULL
};

int main(void)
{
    printf("=== test_kernel_xchg ===\n");
    uint64_t regs[6] = {0};
    const char *by = NULL;

    wubu_kernel_set_our_schema(1);
    CHECK(wubu_kernel_our_schema() == 1, "our schema stored");

    /* -- registry -- */
    CHECK(wubu_kernel_active() == NULL, "nothing active at boot");
    CHECK(wubu_kernel_register(&k_linux) == 0, "linux registered");
    CHECK(wubu_kernel_register(&k_nt) == 0, "nt registered");
    CHECK(wubu_kernel_register(&k_macos) == 0, "macos registered");
    CHECK(wubu_kernel_count() == 3, "three kernels");
    CHECK(strcmp(wubu_kernel_name(0), "linux") == 0, "registration order kept");
    CHECK(strcmp(wubu_kernel_name(2), "macos") == 0, "third name");
    CHECK(wubu_kernel_name(3) == NULL, "out of range -> NULL");
    CHECK(wubu_kernel_register(&k_linux) == -1, "duplicate name rejected");
    CHECK(wubu_kernel_count() == 3, "count unchanged after duplicate");
    CHECK(wubu_kernel_register(&k_inert) == -1, "inert kernel rejected");
    CHECK(wubu_kernel_register(NULL) == -1, "null ops rejected");
    CHECK(wubu_kernel_ops("nt") == &k_nt, "ops lookup by name");
    CHECK(wubu_kernel_ops("nope") == NULL, "unknown ops -> NULL");

    /* -- contract: fail closed -- */
    CHECK(wubu_kernel_register(&k_bad) == 0, "mismatched kernel registered");
    CHECK(wubu_kernel_register(&k_holyd) == 0, "schema-0 kernel registered");
    CHECK(wubu_kernel_schema_ok("bad", 1) == 0, "schema mismatch detected");
    CHECK(wubu_kernel_schema_ok("linux", 1) == 1, "matching schema ok");
    CHECK(wubu_kernel_schema_ok("holyd", 1) == 1, "schema 0 (emits nothing) ok");
    CHECK(wubu_kernel_schema_ok("nope", 1) == 0, "unknown kernel has no contract");

    /* -- selection -- */
    CHECK(wubu_kernel_select("nt") == 0, "select nt");
    CHECK(strcmp(wubu_kernel_active(), "nt") == 0, "active is nt");
    CHECK(wubu_kernel_select("nope") == -1, "unknown select fails");
    CHECK(strcmp(wubu_kernel_active(), "nt") == 0, "active unchanged after failure");
    CHECK(wubu_kernel_select("bad") == -1, "select refused on schema mismatch");
    CHECK(strcmp(wubu_kernel_active(), "nt") == 0, "active unchanged after refused select");

    /* -- routing: every kernel is asked, first claim wins -- */
    CHECK(wubu_kernel_interchange(1, regs, &by) == 10 && strcmp(by, "linux") == 0,
          "syscall 1 -> linux");
    CHECK(wubu_kernel_interchange(100, regs, &by) == 1100 && strcmp(by, "nt") == 0,
          "syscall 100 -> nt");
    CHECK(wubu_kernel_interchange(201, regs, &by) == 2201 && strcmp(by, "macos") == 0,
          "syscall 201 -> macos");
    CHECK(wubu_kernel_interchange(999, regs, &by) == WUBU_KERNEL_ENOSYS && by == NULL,
          "unclaimed -> ENOSYS, no owner");

    /* -- the active kernel goes first, but others still answer -- */
    CHECK(wubu_kernel_select("linux") == 0, "select linux");
    CHECK(wubu_kernel_interchange(100, regs, &by) == 1100 && strcmp(by, "nt") == 0,
          "nt still answers while linux is active");
    CHECK(wubu_kernel_dispatch_active(100, regs) == WUBU_KERNEL_ENOSYS,
          "active-only dispatch does not fall through");
    CHECK(wubu_kernel_dispatch_active(1, regs) == 10,
          "active-only dispatch answers its own call");

    /* -- swap + history -- */
    char prev[WUBU_KERNEL_NAME_MAX];
    CHECK(wubu_kernel_swap("macos", prev, sizeof(prev)) == 0, "swap to macos");
    CHECK(strcmp(prev, "linux") == 0, "previous name reported");
    CHECK(strcmp(wubu_kernel_active(), "macos") == 0, "active is macos");
    CHECK(wubu_kernel_swap("nope", prev, sizeof(prev)) == -1, "swap to unknown fails");
    CHECK(strcmp(wubu_kernel_active(), "macos") == 0, "active unchanged after failed swap");

    char hist[512];
    CHECK(wubu_kernel_history(hist, sizeof(hist)) == 0, "history written");
    CHECK(strstr(hist, "linux -> macos") != NULL, "swap recorded in the log");
    CHECK(strstr(hist, "nt -> linux") != NULL, "earlier swap still in the log");

    /* -- payload routing -- */
    CHECK(strcmp(wubu_kernel_who_execs("a.elf", NULL, 0), "linux") == 0, "elf -> linux");
    CHECK(strcmp(wubu_kernel_who_execs("a.exe", NULL, 0), "nt") == 0, "pe -> nt");
    CHECK(strcmp(wubu_kernel_who_execs("a.macho", NULL, 0), "macos") == 0, "macho -> macos");
    CHECK(wubu_kernel_who_execs("a.xyz", NULL, 0) == NULL, "unknown format -> NULL");
    {
        static const uint8_t elf[4] = {0x7f, 'E', 'L', 'F'};
        static const uint8_t mz[2] = {'M', 'Z'};
        CHECK(strcmp(wubu_kernel_who_execs("noext", elf, 4), "linux") == 0,
              "ELF magic detected without a path hint");
        CHECK(strcmp(wubu_kernel_who_execs("noext", mz, 2), "nt") == 0,
              "MZ magic detected without a path hint");
    }

    /* -- unregister: the active kernel is protected -- */
    CHECK(wubu_kernel_unregister("macos") == -1, "active kernel cannot be removed");
    CHECK(wubu_kernel_unregister("nope") == -1, "unknown unregister fails");
    CHECK(wubu_kernel_unregister("linux") == 0, "inactive kernel removed");
    CHECK(wubu_kernel_count() == 4, "count after removal");
    CHECK(wubu_kernel_who_execs("a.elf", NULL, 0) == NULL, "removed kernel no longer routes");

    /* -- dump -- */
    char buf[1024];
    CHECK(wubu_kernel_xchg_dump(buf, sizeof(buf)) == 0, "dump written");
    CHECK(strstr(buf, "active macos") != NULL, "dump names the active kernel");
    CHECK(strstr(buf, "kernel nt ") != NULL, "dump lists nt");
    CHECK(strstr(buf, "kernel linux") == NULL, "dump omits the removed kernel");
    CHECK(wubu_kernel_xchg_dump(buf, 8) == -1, "truncated dump is reported, not silent");

    if (failures == 0) printf("ALL KERNEL_XCHG TESTS PASSED\n");
    else printf("%d KERNEL_XCHG FAILURES\n", failures);
    return failures ? 1 : 0;
}
