/*
 * wubu_kernel_xchg.h -- the kernel interchange network.
 *
 * WuBuOS does not commit to ONE kernel.  A kernel -- an OS personality such
 * as Linux, Windows NT, macOS, HolyC, CP/M or classic Mac -- is a first-class
 * REGISTERED object with a versioned contract, and the interchange routes
 * each syscall or payload to whichever kernel owns it, falling through to
 * the next kernel when one does not implement the call.  That is what makes
 * the box "the ultimate development environment": the kernel is a hot-swappable
 * part, not the foundation.
 *
 * The contract is the firewall (MEGA_COMPAT DA-2): a kernel whose trace schema
 * major differs from ours is REFUSED, never silently accepted -- a kernel that
 * cannot be observed at the boundary must not be allowed to run.
 *
 * Opaque registry, C11, minimal includes, no external dependencies.
 */
#ifndef WUBU_KERNEL_XCHG_H
#define WUBU_KERNEL_XCHG_H

#include <stddef.h>
#include <stdint.h>

#define WUBU_KERNEL_NAME_MAX 32
#define WUBU_KERNEL_MAX      16

/* No kernel claimed the call.  Kept distinct from any real syscall result so
 * the caller can tell "unimplemented" from a legitimate -38. */
#define WUBU_KERNEL_ENOSYS   (-38)

typedef struct wubu_kernel_ops {
    const char *name;          /* "linux", "nt", "macos", "holyd", "cpm", ... */
    int abi_version;           /* the kernel's own ABI version */
    int schema_version;        /* /n/trace schema major; 0 = emits none */
    const char *formats;       /* space-separated payload formats it can exec */

    /* Route one syscall number.  Return 1 if this kernel handled it (writing
     * the result to *out), 0 if the call is not ours -- 0 is what lets the
     * interchange fall through to the next kernel instead of guessing. */
    int (*try_dispatch)(uint64_t num, uint64_t *regs, int64_t *out);

    /* Does this kernel accept this payload?  1 = yes, 0 = no.  hdr may be
     * NULL when the caller has no bytes yet (path-only probe). */
    int (*probe)(const char *path, const uint8_t *hdr, size_t n);

    /* Load + run a payload.  0 = ok, nonzero = errno-ish reason. */
    int (*exec)(const char *path, int argc, char *const argv[]);
} wubu_kernel_ops_t;

/* -- registry ------------------------------------------------------- */

/* Register a kernel.  0 = ok.  -1 on bad args or a DUPLICATE name: two
 * kernels answering to one name is exactly the drift the interchange exists
 * to prevent, so re-registration is an error rather than an idempotent no-op. */
int  wubu_kernel_register(const wubu_kernel_ops_t *ops);
/* Unregister.  The ACTIVE kernel cannot be unregistered -- swap first. */
int  wubu_kernel_unregister(const char *name);
int  wubu_kernel_count(void);
const char *wubu_kernel_name(int i);
const wubu_kernel_ops_t *wubu_kernel_ops(const char *name);

/* -- contract ------------------------------------------------------- */

/* OUR trace schema major (the operator's schema).  A kernel with a different
 * non-zero schema is refused at select/swap time.  Set once at boot, before
 * any selection. */
void wubu_kernel_set_our_schema(int major);
int  wubu_kernel_our_schema(void);

/* Fail-closed contract check.  A kernel that emits a trace schema different
 * from ours is refused; schema_version == 0 means "emits nothing" and is
 * always acceptable. */
int  wubu_kernel_schema_ok(const char *name, int our_schema);

/* -- selection / swap ----------------------------------------------- */

/* Make a kernel active.  Fails closed on an unknown name or a schema
 * mismatch; on failure the active kernel is left UNCHANGED. */
int  wubu_kernel_select(const char *name);
const char *wubu_kernel_active(void);
/* Swap + report the previous active name into prev (for rollback / the
 * network log).  prev may be NULL. */
int  wubu_kernel_swap(const char *name, char *prev, size_t prev_n);

/* -- routing -------------------------------------------------------- */

/* The interchange: ask the kernels in order (active first, then registration
 * order) and let the FIRST one that claims the call answer it.  Returns the
 * syscall result.  *handled_by is set to the owning kernel's name, or NULL
 * when no kernel claimed the call (return value WUBU_KERNEL_ENOSYS). */
int64_t wubu_kernel_interchange(uint64_t num, uint64_t *regs,
                                const char **handled_by);
/* Dispatch through the ACTIVE kernel only -- no fallthrough. */
int64_t wubu_kernel_dispatch_active(uint64_t num, uint64_t *regs);

/* Which registered kernel can load this payload?  NULL when none. */
const char *wubu_kernel_who_execs(const char *path, const uint8_t *hdr, size_t n);

/* -- observability -------------------------------------------------- */

/* Plain-text network state (kernel list + active marker + swap history), for
 * the /n/kernels publisher and the operator log. */
int wubu_kernel_xchg_dump(char *buf, size_t n);
/* The last swaps, newest first, one "<from> -> <to>" per line. */
int wubu_kernel_history(char *buf, size_t n);

#endif /* WUBU_KERNEL_XCHG_H */
