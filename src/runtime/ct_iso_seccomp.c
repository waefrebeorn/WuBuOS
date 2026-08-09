/*
 * ct_iso_seccomp.c  --  WuBuOS container seccomp-BPF syscall filtering (Cell 420 split).
 * Allowlist approach: deny-by-default, permit explicit syscalls per runtime.
 * Also owns wubu_ct_child_isolation (the child-side seccomp + ns apply).
 */

/* feature-gated symbols (FTW_DEPTH / CLONE_NEW* / st_atime /
 * DT_DIR) hidden by the build's -D_POSIX_C_SOURCE=200809L — a
 * legitimate GNU-surface use, kept localized. */
#define _GNU_SOURCE

#include "ct_iso_seccomp.h"
#include "ct_iso_ns.h"          /* wubu_ns_unshare, WUBU_NS_FLAGS */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <linux/seccomp.h>
#include <linux/filter.h>
#include <linux/audit.h>
#include <sys/prctl.h>
#include <sys/syscall.h>

/* ===================================================================
 * SECCOMP BPF FILTER INFRASTRUCTURE
 * =================================================================== */

/* BPF instruction helpers (avoid conflict with linux/filter.h) */
#undef BPF_STMT
#undef BPF_JUMP
#define BPF_STMT(code, k)    (struct sock_filter){ (uint16_t)(code), 0, 0, (uint32_t)(k) }
#define BPF_JUMP(code, k, jt, jf) (struct sock_filter){ (uint16_t)(code), (uint8_t)(jt), (uint8_t)(jf), (uint32_t)(k) }

/* Load architecture (AUDIT_ARCH_X86_64) -- offset 4 in struct seccomp_data */
#define LOAD_ARCH \
    BPF_STMT(BPF_LD | BPF_W | BPF_ABS, 4)

/* Load syscall number -- offset 0 in struct seccomp_data */
#define LOAD_SYSCALL \
    BPF_STMT(BPF_LD | BPF_W | BPF_ABS, 0)

/* Jump if equal (architecture match) */
#define JEQ_ARCH(val, jt) \
    BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, (val), (jt), 0)

/* Jump if equal (syscall match) */
#define JEQ_SYSCALL(val, jt) \
    BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, (val), (jt), 0)

/* Return ALLOW */
#define RET_ALLOW \
    BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW)

/* Return KILL_PROCESS */
#define RET_KILL \
    BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS)

/* Return ERRNO (value = -errno) */
#define RET_ERRNO(err) \
    BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | ((err) & SECCOMP_RET_DATA))

/* Architecture constant */
#ifndef AUDIT_ARCH_X86_64
#define AUDIT_ARCH_X86_64 0xC000003E
#endif

/* ===================================================================
 * SECCOMP PROFILES PER RUNTIME
 * =================================================================== */

/* The built-in allowlists are defined in seccomp_registry.c (the
 * self-contained profile-registry module — no cgroup/ns deps). */
extern const int g_seccomp_basic_allowlist[];
extern const int g_seccomp_gpu_allowlist[];
extern const int g_seccomp_wine_allowlist[];

/* ===================================================================
 * SECCOMP PROFILE REGISTRY (the Revolver Doctrine: hot-swappable sets)
 *
 * The registry lives in seccomp_registry.c (self-contained, no cgroup/
 * ns deps). The three built-in allowlists above are the seed; callers
 * register NEW profiles via wubu_seccomp_profile_register, and the
 * env-var lookup probes the live registry (wubu_seccomp_profile_lookup).
 * =================================================================== */

/* ===================================================================
 * SECCOMP FILTER BUILDING
 * =================================================================== */

struct seccomp_filter_builder {
    struct sock_filter *filters;
    size_t count;
    size_t capacity;
};

static void filter_init(struct seccomp_filter_builder *b, size_t initial) {
    b->capacity = initial ? initial : 256;
    b->count = 0;
    b->filters = calloc(b->capacity, sizeof(struct sock_filter));
}

static void filter_add(struct seccomp_filter_builder *b, struct sock_filter f) {
    if (b->count >= b->capacity) {
        b->capacity *= 2;
        b->filters = realloc(b->filters, b->capacity * sizeof(struct sock_filter));
    }
    b->filters[b->count++] = f;
}

static void filter_add_allow_syscall(struct seccomp_filter_builder *b, int syscall_nr) {
    /* if (syscall == nr) return ALLOW; */
    filter_add(b, LOAD_SYSCALL);
    filter_add(b, JEQ_SYSCALL(syscall_nr, 1));
    filter_add(b, RET_ALLOW);
}

static int filter_build_allowlist(struct seccomp_filter_builder *b,
                                   const int *allowlist, size_t n_allowlist) {
    /* Load architecture and verify x86_64 */
    filter_add(b, LOAD_ARCH);
    filter_add(b, JEQ_ARCH(AUDIT_ARCH_X86_64, 1));
    filter_add(b, RET_KILL);

    /* Load syscall number */
    filter_add(b, LOAD_SYSCALL);

    /* For each allowed syscall, add a JEQ -> ALLOW */
    for (size_t i = 0; i < n_allowlist; i++) {
        int nr = allowlist[i];
        if (nr < 0) break;
        filter_add_allow_syscall(b, nr);
    }

    /* Default: KILL_PROCESS */
    filter_add(b, RET_KILL);

    return 0;
}

/* ===================================================================
 * CGROUPS V2 INTEGRATION
 * =================================================================== */


/* Namespace flags for full container isolation */
#define WUBU_NS_FLAGS (CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWNET | \
                       CLONE_NEWUSER | CLONE_NEWUTS | CLONE_NEWIPC)









/* ===================================================================
 * CGROUP V2 I/O CONTROLLER SUPPORT
 * =================================================================== */



/* ===================================================================
 * CONTAINER ISOLATION INTEGRATION
 * =================================================================== */

SeccompProfile runtime_to_seccomp(CtRuntime runtime) {
    switch (runtime) {
        case CT_NATIVE:
        case CT_HOLYC:
            return SECCOMP_PROFILE_BASIC;
        case CT_STEAMOS:
            return SECCOMP_PROFILE_GPU;
        case CT_PROTON:
            return SECCOMP_PROFILE_WINE;
        default:
            return SECCOMP_PROFILE_BASIC;
    }
}

/* Apply seccomp filter in the child process (after fork, before exec) */
int wubu_ct_apply_seccomp(void *ct_ptr) {
    WubuCt *ct = (WubuCt *)ct_ptr;
    SeccompProfile profile = runtime_to_seccomp(ct->runtime);
    if (profile == SECCOMP_PROFILE_NONE) return 0;

    struct seccomp_filter_builder builder;
    filter_init(&builder, 512);

    /* Build combined allowlist based on profile */
    const int *lists[3];
    size_t n_lists = 0;

    lists[n_lists++] = g_seccomp_basic_allowlist;
    if (profile == SECCOMP_PROFILE_GPU) {
        lists[n_lists++] = g_seccomp_gpu_allowlist;
    } else if (profile == SECCOMP_PROFILE_WINE) {
        lists[n_lists++] = g_seccomp_wine_allowlist;
    }

    /* Calculate total allowed syscalls */
    size_t total = 0;
    for (size_t i = 0; i < n_lists; i++) {
        for (size_t j = 0; lists[i][j] >= 0; j++) total++;
    }

    /* Build filter */
    filter_build_allowlist(&builder, g_seccomp_basic_allowlist, total);

    /* Install filter via prctl + seccomp syscall */
    struct sock_fprog prog = {
        .len = (unsigned short)builder.count,
        .filter = builder.filters,
    };

    /* PR_SET_NO_NEW_PRIVS must be set before seccomp */
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        free(builder.filters);
        return -1;
    }

    if (syscall(SYS_seccomp, SECCOMP_SET_MODE_FILTER, 0, &prog) != 0) {
        free(builder.filters);
        return -1;
    }

    free(builder.filters);
    return 0;
}
int wubu_ct_child_isolation(void) {
    /* Get cgroup path from environment */
    const char *cgroup_path = getenv("WUBU_CGROUP_PATH");
    if (cgroup_path) {
        pid_t pid = getpid();
        wubu_ct_cgroup_attach(cgroup_path, pid);
    }

    /* Unshare namespaces for container isolation */
    const char *ns_flags_str = getenv("WUBU_NS_FLAGS");
    if (ns_flags_str) {
        int flags = atoi(ns_flags_str);
        if (flags != 0) {
            wubu_ns_unshare(flags);
        }
    } else {
        /* Default: full isolation */
        wubu_ns_unshare(WUBU_NS_FLAGS);
    }

    /* Apply seccomp filter */
    const char *profile = getenv("WUBU_SECCOMP_PROFILE");
    SeccompProfile sp = SECCOMP_PROFILE_BASIC;
    const int *custom_allowlist = NULL;
    if (profile) {
        if (strcmp(profile, "gpu") == 0) sp = SECCOMP_PROFILE_GPU;
        else if (strcmp(profile, "wine") == 0) sp = SECCOMP_PROFILE_WINE;
        else if (strcmp(profile, "none") == 0) sp = SECCOMP_PROFILE_NONE;
        else {
            /* The Revolver Doctrine: any REGISTERED profile name is
             * loadable — probe the live registry before falling back.
             * The custom allowlist (NULL-terminated) is used as-is. */
            custom_allowlist = wubu_seccomp_profile_lookup(profile);
            if (custom_allowlist) sp = SECCOMP_PROFILE_GPU;
            /* else: unknown name -> basic (fail soft) */
        }
    }

    if (sp != SECCOMP_PROFILE_NONE) {
        struct seccomp_filter_builder builder;
        filter_init(&builder, 512);

        const int *lists[3];
        size_t n_lists = 1;
        lists[0] = g_seccomp_basic_allowlist;
        if (custom_allowlist) {
            /* A registered custom profile: basic + custom (probe, don't
             * assume — the custom set IS the cartridge). */
            lists[n_lists++] = custom_allowlist;
        } else if (sp == SECCOMP_PROFILE_GPU) {
            lists[n_lists++] = g_seccomp_gpu_allowlist;
        } else if (sp == SECCOMP_PROFILE_WINE) {
            lists[n_lists++] = g_seccomp_wine_allowlist;
        }

        size_t total = 0;
        for (size_t i = 0; i < n_lists; i++) {
            for (size_t j = 0; lists[i][j] >= 0; j++) total++;
        }

        filter_build_allowlist(&builder, g_seccomp_basic_allowlist, total);

        struct sock_fprog prog = {
            .len = (unsigned short)builder.count,
            .filter = builder.filters,
        };

        prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
        syscall(SYS_seccomp, SECCOMP_SET_MODE_FILTER, 0, &prog);
        free(builder.filters);
    }

    return 0;
}

/* ===================================================================
 * Namespace Isolation API
 * =================================================================== */

/* Write to a cgroup file (generic helper) */
int wubu_seccomp_install(SeccompProfile profile) {
    if (profile == SECCOMP_PROFILE_NONE) return 0;

    struct seccomp_filter_builder builder;
    filter_init(&builder, 512);

    const int *lists[2];
    size_t n_lists = 1;
    lists[0] = g_seccomp_basic_allowlist;
    if (profile == SECCOMP_PROFILE_GPU) lists[n_lists++] = g_seccomp_gpu_allowlist;
    else if (profile == SECCOMP_PROFILE_WINE) lists[n_lists++] = g_seccomp_wine_allowlist;

    size_t total = 0;
    for (size_t i = 0; i < n_lists; i++) {
        for (size_t j = 0; lists[i][j] >= 0; j++) total++;
    }

    filter_build_allowlist(&builder, g_seccomp_basic_allowlist, total);

    struct sock_fprog prog = {
        .len = (unsigned short)builder.count,
        .filter = builder.filters,
    };

    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        free(builder.filters);
        return -1;
    }

    if (syscall(SYS_seccomp, SECCOMP_SET_MODE_FILTER, 0, &prog) != 0) {
        free(builder.filters);
        return -1;
    }

    free(builder.filters);
    return 0;
}

/* Unshare namespaces for container isolation */
