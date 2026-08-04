/*
 * wubu_runtime_personalities.c -- WUBURUNTIME Wave 3 (the gap filler).
 *
 * Every OO runtime speaks syscalls its own way. A personality is a
 * per-space dispatch table that maps a runtime's syscalls onto the
 * OS-native substrate, so the JVM doesn't re-implement files and the
 * Wasm instance doesn't re-implement memory.
 *
 * Built-ins:
 *   posix  -- direct native (open/read/write/close/malloc/free/exit).
 *   image  -- the Smalltalk-family personality: native substrate, but
 *             the space IS the image (object-graph persistence lives
 *             in the space's hive region, not scattered files).
 *   wasi   -- the sandboxed personality: sys_open only accepts paths
 *             under the space's namespace root (/n/...), the WASI ->
 *             native mapping. Refuses everything else.
 *
 * C11, self-contained.
 */
#define _POSIX_C_SOURCE 200809L
#include "wubu_runtime.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

/* ---- the native primitives (all personalities share these) ---- */

static int native_open(const char *path, int flags)
{
    int f = 0;
    if (flags & 1) f |= O_WRONLY;
    if (flags & 2) f |= O_RDWR;
    if (flags & 4) f |= O_CREAT;
    if ((flags & 3) == 0) f |= O_RDONLY;  /* open() REQUIRES an access mode */
    return open(path, f, 0644);
}

static int native_read(int fd, void *buf, size_t n)
{
    return (int)read((int)fd, buf, n);
}

static int native_write(int fd, const void *buf, size_t n)
{
    return (int)write((int)fd, buf, n);
}

static int native_close(int fd)
{
    return close((int)fd);
}

static void *native_heap_alloc(size_t n)
{
    return malloc(n);
}

static void native_heap_free(void *p)
{
    free(p);
}

static void native_exit(int code)
{
    fflush(stdout);
    exit(code);
}

/* ---- the WASI sandbox: refuse paths outside the namespace root ---- */

static int wasi_open(const char *path, int flags)
{
    /* the sandbox: a Wasm instance may only touch its own namespace.
     * The space's namespace_path is /n/<name>/; we enforce the /n/
     * prefix here (the full per-space root is enforced by the space
     * broker layer above). Refusal is a POLICY sentinel, distinct
     * from an OS error. */
    if (!path || strncmp(path, "/n/", 3) != 0)
        return WUBU_RT_SANDBOX_REFUSED;
    return native_open(path, flags);
}

/* ---- the built-in tables (the gap filler, real and native) ---- */

const wubu_rt_personality_t wubu_rt_personality_posix = {
    .name = "posix",
    .sys_open = native_open,
    .sys_read = native_read,
    .sys_write = native_write,
    .sys_close = native_close,
    .sys_heap_alloc = native_heap_alloc,
    .sys_heap_free = native_heap_free,
    .sys_exit = native_exit,
};

const wubu_rt_personality_t wubu_rt_personality_image = {
    .name = "image",
    .sys_open = native_open,
    .sys_read = native_read,
    .sys_write = native_write,
    .sys_close = native_close,
    .sys_heap_alloc = native_heap_alloc,
    .sys_heap_free = native_heap_free,
    .sys_exit = native_exit,
};

const wubu_rt_personality_t wubu_rt_personality_wasi = {
    .name = "wasi",
    .sys_open = wasi_open,       /* the sandbox */
    .sys_read = native_read,
    .sys_write = native_write,
    .sys_close = native_close,
    .sys_heap_alloc = native_heap_alloc,
    .sys_heap_free = native_heap_free,
    .sys_exit = native_exit,
};

/* ---- the registry glue (set + dispatch + list) ---- */

int wubu_runtime_set_personality(wubu_runtime_t *rt, uint64_t id,
                                 const char *kind)
{
    if (!rt || !kind) return -1;
    wubu_rt_space_t *sp = wubu_runtime_find(rt, id);
    if (!sp) return -1;

    if (!strcmp(kind, "posix"))
        sp->personality = &wubu_rt_personality_posix;
    else if (!strcmp(kind, "image"))
        sp->personality = &wubu_rt_personality_image;
    else if (!strcmp(kind, "wasi"))
        sp->personality = &wubu_rt_personality_wasi;
    else
        return -1;               /* unknown kind */

    /* a personality is only meaningful when the space is live */
    if (sp->state == WUBU_RT_COLD)
        sp->state = WUBU_RT_WARM;
    return 0;
}

int64_t wubu_runtime_call(wubu_runtime_t *rt, uint64_t id,
                          wubu_rt_syscall_t sys, int64_t a1,
                          int64_t a2, int64_t a3)
{
    if (!rt) return -1;
    wubu_rt_space_t *sp = wubu_runtime_find(rt, id);
    if (!sp || !sp->personality)
        return -1;               /* no personality (cold / not attached) */
    const wubu_rt_personality_t *p = sp->personality;

    switch (sys) {
    case WUBU_RT_SYS_OPEN:
        return p->sys_open((const char *)(uintptr_t)a1, (int)a2);
    case WUBU_RT_SYS_READ:
        return p->sys_read((int)a1, (void *)(uintptr_t)a2, (size_t)a3);
    case WUBU_RT_SYS_WRITE:
        return p->sys_write((int)a1, (const void *)(uintptr_t)a2, (size_t)a3);
    case WUBU_RT_SYS_CLOSE:
        return p->sys_close((int)a1);
    case WUBU_RT_SYS_HEAP_ALLOC:
        return (int64_t)(uintptr_t)p->sys_heap_alloc((size_t)a1);
    case WUBU_RT_SYS_HEAP_FREE:
        p->sys_heap_free((void *)(uintptr_t)a1);
        return 0;
    case WUBU_RT_SYS_EXIT:
        p->sys_exit((int)a1);
        return 0;                /* not reached */
    default:
        return -1;
    }
}
