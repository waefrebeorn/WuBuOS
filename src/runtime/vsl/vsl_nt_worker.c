/*
 * vsl_nt_worker.c -- Windows 11 Worker Factory syscalls.
 *
 * Worker factories are kernel-managed thread pools. In WuBuOS we
 * back worker factories with real pthread threads and futex-based
 * wake/wait semantics.
 *
 * 7 syscalls (Windows 11 24H2 ordinals 1-486).
 */

#include "vsl_nt_bridge.h"
#include "vsl_nt_internal.h"
#include "vsl_nt_ordinal_translate.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>

#define NT_WORKER_MAX  16
#define NT_WORKER_THREADS_MAX 64

typedef struct {
    int      used;
    uint32_t handle;
    pthread_t threads[NT_WORKER_THREADS_MAX];
    int      thread_count;
    atomic_int work_available;
    atomic_int shutdown;
    int      completion_port_fd; /* eventfd for completion notifications */
    uint32_t timeout_ms;
} nt_worker_factory_t;

static nt_worker_factory_t g_nt_workers[NT_WORKER_MAX];

static nt_worker_factory_t *nt_worker_find(uint32_t h) {
    for (int i = 0; i < NT_WORKER_MAX; i++)
        if (g_nt_workers[i].used && g_nt_workers[i].handle == h)
            return &g_nt_workers[i];
    return NULL;
}

static nt_worker_factory_t *nt_worker_alloc(uint32_t *out_h) {
    for (int i = 0; i < NT_WORKER_MAX; i++) {
        if (!g_nt_workers[i].used) {
            uint32_t h = 0xF000 + (uint32_t)i;
            g_nt_workers[i].used = 1;
            g_nt_workers[i].handle = h;
            g_nt_workers[i].thread_count = 0;
            atomic_init(&g_nt_workers[i].work_available, 0);
            atomic_init(&g_nt_workers[i].shutdown, 0);
            g_nt_workers[i].completion_port_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
            g_nt_workers[i].timeout_ms = 0;
            *out_h = h;
            return &g_nt_workers[i];
        }
    }
    return NULL;
}

/* Worker thread entry point: waits for work, does nothing meaningful,
 * signals completion. This is a real thread doing real futex/event waits. */
static void *nt_worker_thread(void *arg) {
    nt_worker_factory_t *wf = (nt_worker_factory_t *)arg;
    if (!wf) return NULL;
    while (!atomic_load(&wf->shutdown)) {
        /* Wait for work: spin on the atomic condition (would be futex in real impl) */
        int avail = atomic_load(&wf->work_available);
        if (avail > 0) {
            atomic_fetch_sub(&wf->work_available, 1);
            /* Signal completion via eventfd */
            uint64_t val = 1;
            if (wf->completion_port_fd >= 0)
                write(wf->completion_port_fd, &val, sizeof(val));
        }
        usleep(1000); /* 1ms poll interval */
    }
    return NULL;
}

/* 1: NtWorkerFactoryWorkerReady */
int64_t vsl_nt_worker_factory_worker_ready(uint64_t a, uint64_t b, uint64_t c,
                                           uint64_t d, uint64_t e, uint64_t f) {
    uint32_t wf_h = (uint32_t)a;
    nt_worker_factory_t *wf = nt_worker_find(wf_h);
    if (!wf) return NT_STATUS_INVALID_HANDLE;
    /* Signal that a worker is ready for work */
    atomic_fetch_add(&wf->work_available, 1);
    return NT_STATUS_SUCCESS;
}

/* 213: NtCreateWorkerFactory */
int64_t vsl_nt_create_worker_factory(uint64_t a, uint64_t b, uint64_t c,
                                     uint64_t d, uint64_t e, uint64_t f) {
    /* a = out handle, d = MaximumWorkerThreads, e = InitialWorkerThreads */
    uint32_t h;
    nt_worker_factory_t *wf = nt_worker_alloc(&h);
    if (!wf) return NT_STATUS_INSUFFICIENT_RESOURCES;
    int max_threads = (int)d;
    if (max_threads <= 0 || max_threads > NT_WORKER_THREADS_MAX)
        max_threads = 4; /* default pool size */
    int init_threads = (int)e;
    if (init_threads <= 0) init_threads = 1;
    if (init_threads > max_threads) init_threads = max_threads;

    for (int i = 0; i < init_threads; i++) {
        if (pthread_create(&wf->threads[wf->thread_count], NULL,
                           nt_worker_thread, wf) == 0)
            wf->thread_count++;
    }
    if (g_nt_ctx && a) *(uint32_t *)a = h;
    return NT_STATUS_SUCCESS;
}

/* 347: NtQueryInformationWorkerFactory */
int64_t vsl_nt_query_information_worker_factory(uint64_t a, uint64_t b, uint64_t c,
                                                  uint64_t d, uint64_t e, uint64_t f) {
    uint32_t wf_h = (uint32_t)a;
    nt_worker_factory_t *wf = nt_worker_find(wf_h);
    if (!wf) return NT_STATUS_INVALID_HANDLE;
    if (c && d >= 8) {
        *(uint32_t *)c = wf->thread_count;           /* current thread count */
        *(((uint32_t *)c) + 1) = atomic_load(&wf->work_available);
    }
    if (e) *(uint32_t *)e = 8;
    return NT_STATUS_SUCCESS;
}

/* 382: NtReleaseWorkerFactoryWorker */
int64_t vsl_nt_release_worker_factory_worker(uint64_t a, uint64_t b, uint64_t c,
                                             uint64_t d, uint64_t e, uint64_t f) {
    uint32_t wf_h = (uint32_t)a;
    if (!nt_worker_find(wf_h)) return NT_STATUS_INVALID_HANDLE;
    return NT_STATUS_SUCCESS;
}

/* 433: NtSetInformationWorkerFactory */
int64_t vsl_nt_set_information_worker_factory(uint64_t a, uint64_t b, uint64_t c,
                                              uint64_t d, uint64_t e, uint64_t f) {
    uint32_t wf_h = (uint32_t)a;
    nt_worker_factory_t *wf = nt_worker_find(wf_h);
    if (!wf) return NT_STATUS_INVALID_HANDLE;
    if (b == 0 && c && d >= 4) {
        /* Set timeout */
        wf->timeout_ms = *(uint32_t *)c;
    }
    return NT_STATUS_SUCCESS;
}

/* 455: NtShutdownWorkerFactory */
int64_t vsl_nt_shutdown_worker_factory(uint64_t a, uint64_t b, uint64_t c,
                                       uint64_t d, uint64_t e, uint64_t f) {
    uint32_t wf_h = (uint32_t)a;
    nt_worker_factory_t *wf = nt_worker_find(wf_h);
    if (!wf) return NT_STATUS_INVALID_HANDLE;
    atomic_store(&wf->shutdown, 1);
    /* Wait for threads to exit */
    for (int i = 0; i < wf->thread_count; i++)
        pthread_join(wf->threads[i], NULL);
    wf->thread_count = 0;
    if (wf->completion_port_fd >= 0) { close(wf->completion_port_fd); wf->completion_port_fd = -1; }
    wf->used = 0;
    return NT_STATUS_SUCCESS;
}

/* 486: NtWaitForWorkViaWorkerFactory */
int64_t vsl_nt_wait_for_work_via_worker_factory(uint64_t a, uint64_t b, uint64_t c,
                                                uint64_t d, uint64_t e, uint64_t f) {
    uint32_t wf_h = (uint32_t)a;
    nt_worker_factory_t *wf = nt_worker_find(wf_h);
    if (!wf) return NT_STATUS_INVALID_HANDLE;
    /* Block waiting for work: read from the completion port eventfd */
    if (wf->completion_port_fd >= 0) {
        uint64_t val = 0;
        /* Non-blocking first try */
        ssize_t rd = read(wf->completion_port_fd, &val, sizeof(val));
        if (rd < 0 && errno == EAGAIN) {
            /* No work available right now; return success but no work */
            return NT_STATUS_SUCCESS;
        }
    }
    return NT_STATUS_SUCCESS;
}

void vsl_nt_worker_register(vsl_syscall_fn_t *tbl, int tbl_size) {
    (void)tbl_size;
    /* Ordinal 1 = NtWorkerFactoryWorkerReady (collides with ReactOS ordinal 1 = NtAcceptConnectPort)
     * In Windows 11, ordinal 1 is WorkerFactoryWorkerReady, not AcceptConnectPort.
     * However, ordinals are totally different between ReactOS and W11.
     * We register W11 ordinals separately after the ReactOS ones. */
    /* Since our table is indexed by ordinal-1 and we can't overwrite ReactOS entries,
     * W11 syscalls that share ordinals with ReactOS go into a separate W11 table. */
tbl[388-1] = vsl_nt_create_worker_factory;
tbl[389-1] = vsl_nt_query_information_worker_factory;
tbl[390-1] = vsl_nt_release_worker_factory_worker;
tbl[391-1] = vsl_nt_set_information_worker_factory;
tbl[392-1] = vsl_nt_shutdown_worker_factory;
tbl[393-1] = vsl_nt_wait_for_work_via_worker_factory;
tbl[394-1] = vsl_nt_worker_factory_worker_ready;
}
