/*
 * vsl_nt_enclave.c -- Windows 11 Enclave (VBS/SGX) syscalls.
 *
 * Enclaves are secure memory regions for VBS (Virtualization Based
 * Security). In WuBuOS we back enclaves with memfd_create + mmap
 * with PROT_EXEC for code enclaves.
 *
 * 5 syscalls (Windows 11 24H2 ordinals 147-465).
 *
 * C11, no nested functions.
 */

#include "vsl_nt_bridge.h"
#include "vsl_nt_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
#include <linux/memfd.h>

#define NT_ENCLAVE_MAX  16

typedef struct {
    int      used;
    uint32_t handle;
    void    *base;        /* mmap'd memory          */
    size_t   size;
    int      initialized;
} nt_enclave_t;

static nt_enclave_t g_nt_enclaves[NT_ENCLAVE_MAX];

static nt_enclave_t *nt_encl_find(uint32_t h) {
    for (int i = 0; i < NT_ENCLAVE_MAX; i++)
        if (g_nt_enclaves[i].used && g_nt_enclaves[i].handle == h)
            return &g_nt_enclaves[i];
    return NULL;
}

static nt_enclave_t *nt_encl_alloc(uint32_t *out_h) {
    for (int i = 0; i < NT_ENCLAVE_MAX; i++) {
        if (!g_nt_enclaves[i].used) {
            uint32_t h = 0xC000 + (uint32_t)i;
            g_nt_enclaves[i].used = 1;
            g_nt_enclaves[i].handle = h;
            g_nt_enclaves[i].base = NULL;
            g_nt_enclaves[i].size = 0;
            g_nt_enclaves[i].initialized = 0;
            *out_h = h;
            return &g_nt_enclaves[i];
        }
    }
    return NULL;
}

/* 174: NtCreateEnclave */
int64_t vsl_nt_create_enclave(uint64_t a, uint64_t b, uint64_t c,
                              uint64_t d, uint64_t e, uint64_t f) {
    /* a = out handle, d = size */
    uint32_t h;
    nt_enclave_t *en = nt_encl_alloc(&h);
    if (!en) return NT_STATUS_INSUFFICIENT_RESOURCES;
    size_t sz = (size_t)d;
    if (sz == 0) sz = 4096;
    /* Create a memfd to back the enclave memory */
    int mfd = memfd_create("wubu_enclave", MFD_CLOEXEC);
    if (mfd < 0) { en->used = 0; return NT_STATUS_NO_MEMORY; }
    if (ftruncate(mfd, sz) < 0) { close(mfd); en->used = 0; return NT_STATUS_NO_MEMORY; }
    void *mem = mmap(NULL, sz, PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_SHARED, mfd, 0);
    close(mfd);
    if (mem == MAP_FAILED) { en->used = 0; return NT_STATUS_NO_MEMORY; }
    en->base = mem;
    en->size = sz;
    if (g_nt_ctx && a) *(uint32_t *)a = h;
    return NT_STATUS_SUCCESS;
}

/* 262: NtInitializeEnclave */
int64_t vsl_nt_initialize_enclave(uint64_t a, uint64_t b, uint64_t c,
                                  uint64_t d, uint64_t e, uint64_t f) {
    uint32_t encl_h = (uint32_t)a;
    nt_enclave_t *en = nt_encl_find(encl_h);
    if (!en) return NT_STATUS_INVALID_HANDLE;
    /* b = enclave image data, c = image size */
    if (b && c > 0 && en->base) {
        size_t copy_sz = (size_t)c;
        if (copy_sz > en->size) copy_sz = en->size;
        memcpy(en->base, (void *)b, copy_sz);
    }
    en->initialized = 1;
    return NT_STATUS_SUCCESS;
}

/* 270: NtLoadEnclaveData */
int64_t vsl_nt_load_enclave_data(uint64_t a, uint64_t b, uint64_t c,
                                 uint64_t d, uint64_t e, uint64_t f) {
    uint32_t encl_h = (uint32_t)a;
    nt_enclave_t *en = nt_encl_find(encl_h);
    if (!en) return NT_STATUS_INVALID_HANDLE;
    /* b = source data, c = source size, d = destination offset */
    if (b && c > 0 && en->base) {
        size_t off = (size_t)d;
        size_t copy_sz = (size_t)c;
        if (off + copy_sz > en->size) copy_sz = en->size - off;
        memcpy((char *)en->base + off, (void *)b, copy_sz);
        if (e) *(uint64_t *)e = copy_sz; /* out bytes loaded */
    }
    return NT_STATUS_SUCCESS;
}

/* 465: NtTerminateEnclave */
int64_t vsl_nt_terminate_enclave(uint64_t a, uint64_t b, uint64_t c,
                                 uint64_t d, uint64_t e, uint64_t f) {
    uint32_t encl_h = (uint32_t)a;
    nt_enclave_t *en = nt_encl_find(encl_h);
    if (!en) return NT_STATUS_INVALID_HANDLE;
    if (en->base) { munmap(en->base, en->size); en->base = NULL; }
    en->used = 0;
    return NT_STATUS_SUCCESS;
}

/* 147: NtCallEnclave */
int64_t vsl_nt_call_enclave(uint64_t a, uint64_t b, uint64_t c,
                            uint64_t d, uint64_t e, uint64_t f) {
    uint32_t encl_h = (uint32_t)a;
    nt_enclave_t *en = nt_encl_find(encl_h);
    if (!en || !en->initialized) return NT_STATUS_INVALID_HANDLE;
    /* b = enclave routine offset, c = input parameter */
    /* In a real implementation we'd set up the VBS calling convention
     * and jump into enclave memory. For now we return the parameter as-is. */
    void (*routine)(void) = (void (*)(void))((char *)en->base + (size_t)b);
    (void)routine; /* can't safely call arbitrary code in our model */
    if (d) *(uint64_t *)d = c; /* pass through the input as output */
    return NT_STATUS_SUCCESS;
}

void vsl_nt_enclave_register(vsl_syscall_fn_t *tbl, int tbl_size) {
    (void)tbl_size;
    tbl[174-1] = vsl_nt_create_enclave;
    tbl[262-1] = vsl_nt_initialize_enclave;
    tbl[270-1] = vsl_nt_load_enclave_data;
    tbl[465-1] = vsl_nt_terminate_enclave;
    tbl[147-1] = vsl_nt_call_enclave;
}
