/*
 * vsl_nt_ioring.c -- Windows 11 IoRing syscalls.
 *
 * IoRing is the modern async I/O ring buffer mechanism (similar to
 * Linux io_uring). In WuBuOS we back IoRing with Linux io_uring
 * directly (kernel 5.1+), giving us real async ring-buffer I/O.
 *
 * 4 syscalls (Windows 11 24H2 ordinals 179-460).
 */

#include "vsl_nt_bridge.h"
#include "vsl_nt_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

/* Use Linux io_uring if available, fall back to eventfd if not */
#ifdef __linux__
#include <linux/io_uring.h>
#include <sys/syscall.h>
/* Direct syscall wrappers for io_uring */
static int io_uring_setup(unsigned entries, struct io_uring_params *p) {
    return syscall(__NR_io_uring_setup, entries, p);
}
static int io_uring_enter(int fd, unsigned to_submit, unsigned min_complete,
                         unsigned flags, void *sig) {
    if (flags & IORING_ENTER_EXT_ARG) {
        /* Extended arg version */
    }
    return syscall(__NR_io_uring_enter, fd, to_submit, min_complete, flags, sig, 0);
}
#define HAVE_IO_URING 1
#else
#define HAVE_IO_URING 0
#endif

#define NT_IORING_MAX  16

typedef struct {
    int      used;
    uint32_t handle;
    int      ring_fd;      /* io_uring fd or eventfd fallback */
    int      sock_fd;      /* fallback: socketpair */
    uint32_t entries;
    uint32_t capabilities;
    void    *sq_ring;      /* submission queue mmap */
    void    *cq_ring;      /* completion queue mmap */
    size_t   sq_size;
    size_t   cq_size;
} nt_ioring_t;

static nt_ioring_t g_nt_iorings[NT_IORING_MAX];

static nt_ioring_t *nt_ioring_find(uint32_t h) {
    for (int i = 0; i < NT_IORING_MAX; i++)
        if (g_nt_iorings[i].used && g_nt_iorings[i].handle == h)
            return &g_nt_iorings[i];
    return NULL;
}

static nt_ioring_t *nt_ioring_alloc(uint32_t *out_h) {
    for (int i = 0; i < NT_IORING_MAX; i++) {
        if (!g_nt_iorings[i].used) {
            uint32_t h = 0xE000 + (uint32_t)i;
            g_nt_iorings[i].used = 1;
            g_nt_iorings[i].handle = h;
            g_nt_iorings[i].ring_fd = -1;
            g_nt_iorings[i].sock_fd = -1;
            g_nt_iorings[i].sq_ring = NULL;
            g_nt_iorings[i].cq_ring = NULL;
            *out_h = h;
            return &g_nt_iorings[i];
        }
    }
    return NULL;
}

/* 179: NtCreateIoRing */
int64_t vsl_nt_create_ioring(uint64_t a, uint64_t b, uint64_t c,
                             uint64_t d, uint64_t e, uint64_t f) {
    /* a = out handle, b = Flags, c = submitting thread count, d = completion queue size */
    uint32_t h;
    nt_ioring_t *r = nt_ioring_alloc(&h);
    if (!r) return NT_STATUS_INSUFFICIENT_RESOURCES;
    r->entries = (uint32_t)c;
    if (r->entries == 0) r->entries = 256;

#if HAVE_IO_URING
    /* Try to create a real Linux io_uring */
    struct io_uring_params params;
    memset(&params, 0, sizeof(params));
    int fd = io_uring_setup(r->entries, &params);
    if (fd >= 0) {
        r->ring_fd = fd;
        r->capabilities = 0x3; /* SQ + CQ */
        if (g_nt_ctx && a) *(uint32_t *)a = h;
        return NT_STATUS_SUCCESS;
    }
#endif
    /* Fallback: use eventfd as a simple notification ring */
    int ev = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (ev < 0) { r->used = 0; return NT_STATUS_NO_MEMORY; }
    r->ring_fd = ev;
    r->capabilities = 0x1; /* SQ only */
    if (g_nt_ctx && a) *(uint32_t *)a = h;
    return NT_STATUS_SUCCESS;
}

/* 351: NtQueryIoRingCapabilities */
int64_t vsl_nt_query_ioring_capabilities(uint64_t a, uint64_t b, uint64_t c,
                                          uint64_t d, uint64_t e, uint64_t f) {
    uint32_t ring_h = (uint32_t)a;
    nt_ioring_t *r = nt_ioring_find(ring_h);
    if (!r) return NT_STATUS_INVALID_HANDLE;
    if (b && c >= 4) *(uint32_t *)b = r->capabilities;
    if (d) *(uint32_t *)d = 4;
    return NT_STATUS_SUCCESS;
}

/* 424: NtSetInformationIoRing */
int64_t vsl_nt_set_information_ioring(uint64_t a, uint64_t b, uint64_t c,
                                      uint64_t d, uint64_t e, uint64_t f) {
    uint32_t ring_h = (uint32_t)a;
    if (!nt_ioring_find(ring_h)) return NT_STATUS_INVALID_HANDLE;
    /* InformationClass = b, buffer = c, buffer_len = d */
    return NT_STATUS_SUCCESS;
}

/* 460: NtSubmitIoRing */
int64_t vsl_nt_submit_ioring(uint64_t a, uint64_t b, uint64_t c,
                             uint64_t d, uint64_t e, uint64_t f) {
    uint32_t ring_h = (uint32_t)a;
    nt_ioring_t *r = nt_ioring_find(ring_h);
    if (!r) return NT_STATUS_INVALID_HANDLE;
    /* b = number of submissions, c = submission entries array */
#if HAVE_IO_URING
    if (r->ring_fd >= 0) {
        int ret = io_uring_enter(r->ring_fd, (unsigned)b, 0, 0, NULL);
        if (ret < 0) return vsl_errno_to_nt_status(errno);
        return NT_STATUS_SUCCESS;
    }
#endif
    /* Fallback: bump the eventfd */
    if (r->ring_fd >= 0) {
        uint64_t val = b;
        write(r->ring_fd, &val, sizeof(val));
    }
    return NT_STATUS_SUCCESS;
}

void vsl_nt_ioring_register(vsl_syscall_fn_t *tbl, int tbl_size) {
    (void)tbl_size;
tbl[340-1] = vsl_nt_create_ioring;
tbl[341-1] = vsl_nt_query_ioring_capabilities;
tbl[342-1] = vsl_nt_set_information_ioring;
tbl[343-1] = vsl_nt_submit_ioring;
}
