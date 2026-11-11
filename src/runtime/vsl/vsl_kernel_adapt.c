/*
 * vsl_kernel_adapt.c -- the real VSL personalities, registered as kernels.
 *
 * Until this file existed the class bits inside the syscall number WERE the
 * router: 0xC0 meant CP/M, 0xB0 meant classic Mac, any other high byte meant
 * macOS, and a bare number meant Linux.  That works, but it means the box
 * cannot load or swap a personality -- the routing is a hardcoded if-chain in
 * vsl_syscall_dispatch(), and adding a kernel means editing that chain.
 *
 * Here each personality owns its number range in a try_dispatch() callback and
 * the interchange decides.  The ranges are DISJOINT, so the outcome does not
 * depend on the order the kernels happen to be registered in:
 *
 *   cpm         0xC0 << 24 | fn  << 8
 *   macclassic  0xB0 << 24 | trap
 *   macos       any OTHER high byte
 *   nt          bare numbers -- but only while NT is the SELECTED kernel
 *   linux       bare numbers -- while NT is not
 *
 * NT and Linux share the bare range, so the SELECTED kernel is authoritative
 * for it.  That is what selection means here: "the kernel in charge", not "one
 * of several that might answer".
 *
 * C11, opaque, no GUI dependency.
 */
#include "vsl_syscall_internal.h"
#include "vsl_nt_bridge.h"
#include "../wubu_kernel_xchg.h"
#include "../wubu_trace.h"

#include <stdio.h>
#include <string.h>

/* The NT bridge is an OPTIONAL link dependency.  A link without it (the CP/M
 * or classic-Mac tests, say) still registers the nt kernel -- it just cannot
 * answer, which is what "not handled" means.  Never call a weak symbol
 * without testing its address first. */
extern int vsl_nt_bridge_init(vsl_nt_bridge_ctx_t *ctx) __attribute__((weak));
extern int64_t vsl_nt_syscall_dispatch(vsl_nt_bridge_ctx_t *ctx, uint16_t num,
                                       uint64_t *args, int n_args)
    __attribute__((weak));

/* -- NT: the bare range, when NT is selected ------------------------- */

static vsl_nt_bridge_ctx_t g_nt_ctx;
static int g_nt_ctx_ready;

static int nt_is_selected(void)
{
    const char *a = wubu_kernel_active();
    return a && strcmp(a, "nt") == 0;
}

static int nt_try(uint64_t num, uint64_t *regs, int64_t *out)
{
    if (num & 0xFF000000u) return 0;          /* a class bit is set: not ours */
    if (!nt_is_selected()) return 0;          /* Linux owns the bare range */
    if (!vsl_nt_bridge_init || !vsl_nt_syscall_dispatch) return 0;   /* not linked */
    if (!g_nt_ctx_ready) {
        memset(&g_nt_ctx, 0, sizeof(g_nt_ctx));
        if (vsl_nt_bridge_init(&g_nt_ctx) != 0) return 0;   /* not handled */
        g_nt_ctx_ready = 1;
    }
    uint64_t args[6] = { regs[0], regs[1], regs[2], regs[3], regs[4], regs[5] };
    *out = vsl_nt_syscall_dispatch(&g_nt_ctx, (uint16_t)num, args, 6);
    return 1;
}

static int nt_probe(const char *path, const uint8_t *h, size_t n)
{
    if (path && strstr(path, ".exe")) return 1;
    if (path && strstr(path, ".dll")) return 1;
    return (n >= 2 && h && h[0] == 'M' && h[1] == 'Z');
}

/* -- Linux: the bare range, while NT is not selected ----------------- */

static int linux_try(uint64_t num, uint64_t *regs, int64_t *out)
{
    if (num & 0xFF000000u) return 0;
    if (nt_is_selected()) return 0;           /* NT is in charge right now */
    *out = vsl_syscall(num, regs[0], regs[1], regs[2], regs[3], regs[4], regs[5]);
    return 1;
}

static int linux_probe(const char *path, const uint8_t *h, size_t n)
{
    if (path && strstr(path, ".elf")) return 1;
    return (n >= 4 && h && h[0] == 0x7f && h[1] == 'E' && h[2] == 'L' && h[3] == 'F');
}

/* -- macOS: every high byte except the two reserved classes --------- */

static int macos_try(uint64_t num, uint64_t *regs, int64_t *out)
{
    uint32_t cb = (uint32_t)(num >> 24);
    if (cb == 0x00 || cb == 0xC0 || cb == 0xB0) return 0;   /* reserved */
    *out = vsl_mac_syscall_dispatch(num, regs[0], regs[1], regs[2],
                                    regs[3], regs[4], regs[5]);
    return 1;
}

static int macos_probe(const char *path, const uint8_t *h, size_t n)
{
    if (path && strstr(path, ".macho")) return 1;
    return (n >= 4 && h && h[0] == 0xcf && h[1] == 0xfa &&
            h[2] == 0xed && h[3] == 0xfe);
}

/* -- CP/M and classic Mac: their reserved class bytes ---------------- */

static int cpm_try(uint64_t num, uint64_t *regs, int64_t *out)
{
    if ((uint32_t)(num >> 24) != 0xC0) return 0;
    uint64_t fn = (num >> 8) & 0xFF;
    *out = vsl_cpm_syscall_dispatch(fn, regs[0], regs[1], regs[2],
                                    regs[3], regs[4], regs[5]);
    return 1;
}

static int macclassic_try(uint64_t num, uint64_t *regs, int64_t *out)
{
    if ((uint32_t)(num >> 24) != 0xB0) return 0;
    *out = vsl_macclassic_syscall_dispatch(num & 0xFFF, regs[0], regs[1], regs[2],
                                           regs[3], regs[4], regs[5]);
    return 1;
}

/* -- the kernel table ----------------------------------------------- */

#define K_SCHEMA WUBU_TRACE_SCHEMA_MAJOR

static const wubu_kernel_ops_t K_CPM = {
    "cpm", 1, K_SCHEMA, NULL, cpm_try, NULL, NULL
};
static const wubu_kernel_ops_t K_MACC = {
    "macclassic", 1, K_SCHEMA, NULL, macclassic_try, NULL, NULL
};
static const wubu_kernel_ops_t K_MACOS = {
    "macos", 1, K_SCHEMA, "macho", macos_try, macos_probe, NULL
};
static const wubu_kernel_ops_t K_LINUX = {
    "linux", 1, K_SCHEMA, "elf64", linux_try, linux_probe, NULL
};
static const wubu_kernel_ops_t K_NT = {
    "nt", 1, K_SCHEMA, "pe", nt_try, nt_probe, NULL
};

static int g_xchg_done;

/* Register the five real personalities.  Idempotent: a name already in the
 * network is left alone (register() rejects duplicates by design, so a name
 * that is present is a name that is already correctly registered). */
int vsl_kernel_interchange_init(void)
{
    if (g_xchg_done) return 0;
    wubu_kernel_set_our_schema(WUBU_TRACE_SCHEMA_MAJOR);
    if (!wubu_kernel_ops("cpm"))         wubu_kernel_register(&K_CPM);
    if (!wubu_kernel_ops("macclassic")) wubu_kernel_register(&K_MACC);
    if (!wubu_kernel_ops("macos"))      wubu_kernel_register(&K_MACOS);
    if (!wubu_kernel_ops("linux"))      wubu_kernel_register(&K_LINUX);
    if (!wubu_kernel_ops("nt"))         wubu_kernel_register(&K_NT);
    /* The default kernel is Linux: the bare numbers ARE the Linux ABI. */
    if (!wubu_kernel_active()) wubu_kernel_select("linux");
    g_xchg_done = 1;
    return 0;
}

/* The interchange-based dispatch entry.  The live vsl_syscall_dispatch()
 * funnels through here, so the router -- not the class bits -- decides. */
int64_t vsl_kernel_interchange_dispatch(uint64_t num, uint64_t *regs,
                                        const char **handled_by)
{
    if (vsl_kernel_interchange_init() != 0) return WUBU_KERNEL_ENOSYS;
    return wubu_kernel_interchange(num, regs, handled_by);
}
