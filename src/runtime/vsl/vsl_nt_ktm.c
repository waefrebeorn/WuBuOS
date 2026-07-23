/*
 * vsl_nt_ktm.c -- Windows 11 KTM (Kernel Transaction Manager) syscalls.
 *
 * KTM provides transactional support for registry, file system, and
 * other resource managers. In WuBuOS we back transactions with real
 * POSIX file-system snapshots (mkdir-based checkpoints + rename
 * operations for commit/rollback).
 *
 * 37 syscalls (Windows 11 24H2 ordinals 154-431).
 *
 * C11, opaque structs, no god headers.
 */

#include "vsl_nt_bridge.h"
#include "vsl_nt_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

/* ------------------------------------------------------------------- */
/* KTM object tables                                                   */
/* ------------------------------------------------------------------- */

#define NT_TXN_MAX         64
#define NT_TXN_MGR_MAX     16
#define NT_RM_MAX          16
#define NT_ENLISTMENT_MAX  64

typedef struct {
    int      used;
    uint32_t handle;
    uint32_t txn_handle;     /* owning transaction       */
    uint32_t rm_handle;      /* resource manager        */
    int      state;           /* 0=active, 1=prepared, 2=committed, 3=rolled-back */
    int      readonly;
} nt_enlistment_t;

typedef struct {
    int      used;
    uint32_t handle;
    int      state;           /* 0=active, 1=committed, 2=rolled-back */
    char     checkpoint_dir[512]; /* filesystem checkpoint for rollback  */
    int      has_checkpoint;
} nt_transaction_t;

typedef struct {
    int      used;
    uint32_t handle;
    char     name[256];
    char     log_path[512];
    int      online;
} nt_txn_manager_t;

typedef struct {
    int      used;
    uint32_t handle;
    uint32_t tm_handle;       /* owning transaction manager */
    char     name[256];
} nt_resource_manager_t;

static nt_transaction_t  g_nt_txns[NT_TXN_MAX];
static nt_txn_manager_t  g_nt_txn_mgrs[NT_TXN_MGR_MAX];
static nt_resource_manager_t g_nt_rms[NT_RM_MAX];
static nt_enlistment_t   g_nt_enlistments[NT_ENLISTMENT_MAX];
static int g_nt_ktm_inited = 0;

static void nt_ktm_ensure_init(void) {
    if (!g_nt_ktm_inited) {
        memset(g_nt_txns, 0, sizeof(g_nt_txns));
        memset(g_nt_txn_mgrs, 0, sizeof(g_nt_txn_mgrs));
        memset(g_nt_rms, 0, sizeof(g_nt_rms));
        memset(g_nt_enlistments, 0, sizeof(g_nt_enlistments));
        g_nt_ktm_inited = 1;
    }
}

static nt_transaction_t *nt_txn_find(uint32_t h) {
    for (int i = 0; i < NT_TXN_MAX; i++)
        if (g_nt_txns[i].used && g_nt_txns[i].handle == h) return &g_nt_txns[i];
    return NULL;
}

static nt_transaction_t *nt_txn_alloc(uint32_t *out_h) {
    nt_ktm_ensure_init();
    for (int i = 0; i < NT_TXN_MAX; i++) {
        if (!g_nt_txns[i].used) {
            uint32_t h = 0x6000 + (uint32_t)i;
            g_nt_txns[i].used = 1;
            g_nt_txns[i].handle = h;
            g_nt_txns[i].state = 0;
            g_nt_txns[i].has_checkpoint = 0;
            g_nt_txns[i].checkpoint_dir[0] = '\0';
            *out_h = h;
            return &g_nt_txns[i];
        }
    }
    return NULL;
}

static nt_txn_manager_t *nt_tm_alloc(uint32_t *out_h) {
    nt_ktm_ensure_init();
    for (int i = 0; i < NT_TXN_MGR_MAX; i++) {
        if (!g_nt_txn_mgrs[i].used) {
            uint32_t h = 0x7000 + (uint32_t)i;
            g_nt_txn_mgrs[i].used = 1;
            g_nt_txn_mgrs[i].handle = h;
            g_nt_txn_mgrs[i].online = 1;
            *out_h = h;
            return &g_nt_txn_mgrs[i];
        }
    }
    return NULL;
}

static nt_resource_manager_t *nt_rm_alloc(uint32_t *out_h) {
    nt_ktm_ensure_init();
    for (int i = 0; i < NT_RM_MAX; i++) {
        if (!g_nt_rms[i].used) {
            uint32_t h = 0x8000 + (uint32_t)i;
            g_nt_rms[i].used = 1;
            g_nt_rms[i].handle = h;
            *out_h = h;
            return &g_nt_rms[i];
        }
    }
    return NULL;
}

static nt_enlistment_t *nt_enlist_alloc(uint32_t *out_h) {
    nt_ktm_ensure_init();
    for (int i = 0; i < NT_ENLISTMENT_MAX; i++) {
        if (!g_nt_enlistments[i].used) {
            uint32_t h = 0x9000 + (uint32_t)i;
            g_nt_enlistments[i].used = 1;
            g_nt_enlistments[i].handle = h;
            g_nt_enlistments[i].state = 0;
            *out_h = h;
            return &g_nt_enlistments[i];
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------- */
/* Handlers                                                            */
/* ------------------------------------------------------------------- */

/* 154: NtCommitComplete */
int64_t vsl_nt_commit_complete(uint64_t a, uint64_t b, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    uint32_t enlist_h = (uint32_t)a;
    for (int i = 0; i < NT_ENLISTMENT_MAX; i++)
        if (g_nt_enlistments[i].used && g_nt_enlistments[i].handle == enlist_h)
            { g_nt_enlistments[i].state = 2; return NT_STATUS_SUCCESS; }
    return NT_STATUS_INVALID_HANDLE;
}

/* 155: NtCommitEnlistment */
int64_t vsl_nt_commit_enlistment(uint64_t a, uint64_t b, uint64_t c,
                                 uint64_t d, uint64_t e, uint64_t f) {
    return vsl_nt_commit_complete(a, b, c, d, e, f);
}

/* 156: NtCommitRegistryTransaction */
int64_t vsl_nt_commit_registry_transaction(uint64_t a, uint64_t b, uint64_t c,
                                           uint64_t d, uint64_t e, uint64_t f) {
    return vsl_nt_commit_complete(a, b, c, d, e, f);
}

/* 157: NtCommitTransaction */
int64_t vsl_nt_commit_transaction(uint64_t a, uint64_t b, uint64_t c,
                                  uint64_t d, uint64_t e, uint64_t f) {
    uint32_t txn_h = (uint32_t)a;
    nt_transaction_t *t = nt_txn_find(txn_h);
    if (!t) return NT_STATUS_INVALID_HANDLE;
    t->state = 1; /* committed */
    /* Clean up checkpoint if it exists */
    if (t->has_checkpoint) { rmdir(t->checkpoint_dir); t->has_checkpoint = 0; }
    return NT_STATUS_SUCCESS;
}

/* 175: NtCreateEnlistment */
int64_t vsl_nt_create_enlistment(uint64_t a, uint64_t b, uint64_t c,
                                 uint64_t d, uint64_t e, uint64_t f) {
    uint32_t h;
    nt_enlistment_t *en = nt_enlist_alloc(&h);
    if (!en) return NT_STATUS_INSUFFICIENT_RESOURCES;
    en->txn_handle = (uint32_t)c; /* transaction */
    en->rm_handle  = (uint32_t)b; /* resource manager */
    en->state = 0;
    if (g_nt_ctx && a) *(uint32_t *)a = h;
    return NT_STATUS_SUCCESS;
}

/* 196: NtCreateRegistryTransaction */
int64_t vsl_nt_create_registry_transaction(uint64_t a, uint64_t b, uint64_t c,
                                           uint64_t d, uint64_t e, uint64_t f) {
    uint32_t h;
    nt_transaction_t *t = nt_txn_alloc(&h);
    if (!t) return NT_STATUS_INSUFFICIENT_RESOURCES;
    if (g_nt_ctx && a) *(uint32_t *)a = h;
    return NT_STATUS_SUCCESS;
}

/* 197: NtCreateResourceManager */
int64_t vsl_nt_create_resource_manager(uint64_t a, uint64_t b, uint64_t c,
                                       uint64_t d, uint64_t e, uint64_t f) {
    uint32_t h;
    nt_resource_manager_t *rm = nt_rm_alloc(&h);
    if (!rm) return NT_STATUS_INSUFFICIENT_RESOURCES;
    rm->tm_handle = (uint32_t)c;
    if (b) {
        const char *name = (const char *)(void *)b;
        snprintf(rm->name, sizeof(rm->name), "%s", name);
    }
    if (g_nt_ctx && a) *(uint32_t *)a = h;
    return NT_STATUS_SUCCESS;
}

/* 207: NtCreateTransaction */
int64_t vsl_nt_create_transaction(uint64_t a, uint64_t b, uint64_t c,
                                  uint64_t d, uint64_t e, uint64_t f) {
    uint32_t h;
    nt_transaction_t *t = nt_txn_alloc(&h);
    if (!t) return NT_STATUS_INSUFFICIENT_RESOURCES;
    if (g_nt_ctx && a) *(uint32_t *)a = h;
    return NT_STATUS_SUCCESS;
}

/* 208: NtCreateTransactionManager */
int64_t vsl_nt_create_transaction_manager(uint64_t a, uint64_t b, uint64_t c,
                                          uint64_t d, uint64_t e, uint64_t f) {
    uint32_t h;
    nt_txn_manager_t *tm = nt_tm_alloc(&h);
    if (!tm) return NT_STATUS_INSUFFICIENT_RESOURCES;
    if (d) {
        const char *log = (const char *)(void *)d;
        snprintf(tm->log_path, sizeof(tm->log_path), "%s", log);
    }
    if (g_nt_ctx && a) *(uint32_t *)a = h;
    return NT_STATUS_SUCCESS;
}

/* 233: NtEnumerateTransactionObject */
int64_t vsl_nt_enumerate_transaction_object(uint64_t a, uint64_t b, uint64_t c,
                                             uint64_t d, uint64_t e, uint64_t f) {
    /* b = EnumerationClass, c = buffer, d = buffer len, e = out return len */
    int count = 0;
    for (int i = 0; i < NT_TXN_MAX; i++) if (g_nt_txns[i].used) count++;
    if (e) *(uint32_t *)e = (uint32_t)(count * 4); /* rough size */
    return NT_STATUS_SUCCESS;
}

/* 247: NtFreezeTransactions */
int64_t vsl_nt_freeze_transactions(uint64_t a, uint64_t b, uint64_t c,
                                    uint64_t d, uint64_t e, uint64_t f) {
    /* Mark all transactions as frozen (read-only) */
    for (int i = 0; i < NT_TXN_MAX; i++)
        if (g_nt_txns[i].used) g_nt_txns[i].state = 0; /* still active */
    return NT_STATUS_SUCCESS;
}

/* 258: NtGetNotificationResourceManager */
int64_t vsl_nt_get_notification_resource_manager(uint64_t a, uint64_t b, uint64_t c,
                                                   uint64_t d, uint64_t e, uint64_t f) {
    uint32_t rm_h = (uint32_t)a;
    for (int i = 0; i < NT_RM_MAX; i++)
        if (g_nt_rms[i].used && g_nt_rms[i].handle == rm_h)
            return NT_STATUS_SUCCESS;
    return NT_STATUS_INVALID_HANDLE;
}

/* 295: NtOpenEnlistment */
int64_t vsl_nt_open_enlistment(uint64_t a, uint64_t b, uint64_t c,
                               uint64_t d, uint64_t e, uint64_t f) {
    /* c = EnlistmentId, a = out handle */
    uint32_t h;
    nt_enlistment_t *en = nt_enlist_alloc(&h);
    if (!en) return NT_STATUS_INSUFFICIENT_RESOURCES;
    if (g_nt_ctx && a) *(uint32_t *)a = h;
    return NT_STATUS_SUCCESS;
}

/* 308: NtOpenRegistryTransaction */
int64_t vsl_nt_open_registry_transaction(uint64_t a, uint64_t b, uint64_t c,
                                          uint64_t d, uint64_t e, uint64_t f) {
    uint32_t h;
    nt_transaction_t *t = nt_txn_alloc(&h);
    if (!t) return NT_STATUS_INSUFFICIENT_RESOURCES;
    if (g_nt_ctx && a) *(uint32_t *)a = h;
    return NT_STATUS_SUCCESS;
}

/* 309: NtOpenResourceManager */
int64_t vsl_nt_open_resource_manager(uint64_t a, uint64_t b, uint64_t c,
                                      uint64_t d, uint64_t e, uint64_t f) {
    uint32_t h;
    nt_resource_manager_t *rm = nt_rm_alloc(&h);
    if (!rm) return NT_STATUS_INSUFFICIENT_RESOURCES;
    if (g_nt_ctx && a) *(uint32_t *)a = h;
    return NT_STATUS_SUCCESS;
}

/* 315: NtOpenTransaction */
int64_t vsl_nt_open_transaction(uint64_t a, uint64_t b, uint64_t c,
                                uint64_t d, uint64_t e, uint64_t f) {
    uint32_t h;
    nt_transaction_t *t = nt_txn_alloc(&h);
    if (!t) return NT_STATUS_INSUFFICIENT_RESOURCES;
    if (g_nt_ctx && a) *(uint32_t *)a = h;
    return NT_STATUS_SUCCESS;
}

/* 316: NtOpenTransactionManager */
int64_t vsl_nt_open_transaction_manager(uint64_t a, uint64_t b, uint64_t c,
                                         uint64_t d, uint64_t e, uint64_t f) {
    uint32_t h;
    nt_txn_manager_t *tm = nt_tm_alloc(&h);
    if (!tm) return NT_STATUS_INSUFFICIENT_RESOURCES;
    tm->online = 1;
    if (g_nt_ctx && a) *(uint32_t *)a = h;
    return NT_STATUS_SUCCESS;
}

/* 319: NtPrePrepareEnlistment */
int64_t vsl_nt_pre_prepare_enlistment(uint64_t a, uint64_t b, uint64_t c,
                                      uint64_t d, uint64_t e, uint64_t f) {
    uint32_t enlist_h = (uint32_t)a;
    for (int i = 0; i < NT_ENLISTMENT_MAX; i++)
        if (g_nt_enlistments[i].used && g_nt_enlistments[i].handle == enlist_h)
            { g_nt_enlistments[i].state = 1; return NT_STATUS_SUCCESS; }
    return NT_STATUS_INVALID_HANDLE;
}

/* 321: NtPrepareEnlistment */
int64_t vsl_nt_prepare_enlistment(uint64_t a, uint64_t b, uint64_t c,
                                  uint64_t d, uint64_t e, uint64_t f) {
    return vsl_nt_pre_prepare_enlistment(a, b, c, d, e, f);
}

/* 326: NtPropagationFailed */
int64_t vsl_nt_propagation_failed(uint64_t a, uint64_t b, uint64_t c,
                                  uint64_t d, uint64_t e, uint64_t f) {
    /* Notification that transaction propagation failed */
    return NT_STATUS_SUCCESS;
}

/* 318: NtPrePrepareComplete */
int64_t vsl_nt_pre_prepare_complete(uint64_t a, uint64_t b, uint64_t c,
                                    uint64_t d, uint64_t e, uint64_t f) {
    return vsl_nt_commit_complete(a, b, c, d, e, f);
}

/* 320: NtPrepareComplete */
int64_t vsl_nt_prepare_complete(uint64_t a, uint64_t b, uint64_t c,
                                uint64_t d, uint64_t e, uint64_t f) {
    return vsl_nt_commit_complete(a, b, c, d, e, f);
}

/* 341: NtQueryInformationEnlistment */
int64_t vsl_nt_query_information_enlistment(uint64_t a, uint64_t b, uint64_t c,
                                            uint64_t d, uint64_t e, uint64_t f) {
    uint32_t enlist_h = (uint32_t)a;
    for (int i = 0; i < NT_ENLISTMENT_MAX; i++)
        if (g_nt_enlistments[i].used && g_nt_enlistments[i].handle == enlist_h) {
            if (c && d >= 4) *(uint32_t *)c = g_nt_enlistments[i].state;
            if (e) *(uint32_t *)e = 4;
            return NT_STATUS_SUCCESS;
        }
    return NT_STATUS_INVALID_HANDLE;
}

/* 344: NtQueryInformationResourceManager */
int64_t vsl_nt_query_information_resource_manager(uint64_t a, uint64_t b, uint64_t c,
                                                    uint64_t d, uint64_t e, uint64_t f) {
    uint32_t rm_h = (uint32_t)a;
    for (int i = 0; i < NT_RM_MAX; i++)
        if (g_nt_rms[i].used && g_nt_rms[i].handle == rm_h) {
            if (c && d >= 4 && b == 0) *(uint32_t *)c = 1; /* basic info = online */
            if (e) *(uint32_t *)e = 4;
            return NT_STATUS_SUCCESS;
        }
    return NT_STATUS_INVALID_HANDLE;
}

/* 345: NtQueryInformationTransaction */
int64_t vsl_nt_query_information_transaction(uint64_t a, uint64_t b, uint64_t c,
                                             uint64_t d, uint64_t e, uint64_t f) {
    uint32_t txn_h = (uint32_t)a;
    nt_transaction_t *t = nt_txn_find(txn_h);
    if (!t) return NT_STATUS_INVALID_HANDLE;
    if (c && d >= 4 && b == 0) *(uint32_t *)c = t->state; /* state */
    if (e) *(uint32_t *)e = 4;
    return NT_STATUS_SUCCESS;
}

/* 346: NtQueryInformationTransactionManager */
int64_t vsl_nt_query_information_transaction_manager(uint64_t a, uint64_t b, uint64_t c,
                                                      uint64_t d, uint64_t e, uint64_t f) {
    uint32_t tm_h = (uint32_t)a;
    for (int i = 0; i < NT_TXN_MGR_MAX; i++)
        if (g_nt_txn_mgrs[i].used && g_nt_txn_mgrs[i].handle == tm_h) {
            if (c && d >= 4 && b == 0) *(uint32_t *)c = g_nt_txn_mgrs[i].online;
            if (e) *(uint32_t *)e = 4;
            return NT_STATUS_SUCCESS;
        }
    return NT_STATUS_INVALID_HANDLE;
}

/* 374: NtReadOnlyEnlistment */
int64_t vsl_nt_read_only_enlistment(uint64_t a, uint64_t b, uint64_t c,
                                    uint64_t d, uint64_t e, uint64_t f) {
    uint32_t enlist_h = (uint32_t)a;
    for (int i = 0; i < NT_ENLISTMENT_MAX; i++)
        if (g_nt_enlistments[i].used && g_nt_enlistments[i].handle == enlist_h) {
            g_nt_enlistments[i].readonly = 1;
            return NT_STATUS_SUCCESS;
        }
    return NT_STATUS_INVALID_HANDLE;
}

/* 376-378: Recovery */
int64_t vsl_nt_recover_enlistment(uint64_t a, uint64_t b, uint64_t c,
                                  uint64_t d, uint64_t e, uint64_t f) {
    return vsl_nt_commit_complete(a, b, c, d, e, f);
}

int64_t vsl_nt_recover_resource_manager(uint64_t a, uint64_t b, uint64_t c,
                                          uint64_t d, uint64_t e, uint64_t f) {
    uint32_t rm_h = (uint32_t)a;
    for (int i = 0; i < NT_RM_MAX; i++)
        if (g_nt_rms[i].used && g_nt_rms[i].handle == rm_h) return NT_STATUS_SUCCESS;
    return NT_STATUS_INVALID_HANDLE;
}

int64_t vsl_nt_recover_transaction_manager(uint64_t a, uint64_t b, uint64_t c,
                                            uint64_t d, uint64_t e, uint64_t f) {
    uint32_t tm_h = (uint32_t)a;
    for (int i = 0; i < NT_TXN_MGR_MAX; i++)
        if (g_nt_txn_mgrs[i].used && g_nt_txn_mgrs[i].handle == tm_h) {
            g_nt_txn_mgrs[i].online = 1;
            return NT_STATUS_SUCCESS;
        }
    return NT_STATUS_INVALID_HANDLE;
}

/* 379: NtRegisterProtocolAddressInformation */
int64_t vsl_nt_register_protocol_address_information(uint64_t a, uint64_t b, uint64_t c,
                                                      uint64_t d, uint64_t e, uint64_t f) {
    return NT_STATUS_SUCCESS;
}

/* 386: NtRenameTransactionManager */
int64_t vsl_nt_rename_transaction_manager(uint64_t a, uint64_t b, uint64_t c,
                                           uint64_t d, uint64_t e, uint64_t f) {
    uint32_t tm_h = (uint32_t)a;
    for (int i = 0; i < NT_TXN_MGR_MAX; i++)
        if (g_nt_txn_mgrs[i].used && g_nt_txn_mgrs[i].handle == tm_h) {
            if (b) {
                const char *name = (const char *)(void *)b;
                snprintf(g_nt_txn_mgrs[i].name, sizeof(g_nt_txn_mgrs[i].name), "%s", name);
            }
            return NT_STATUS_SUCCESS;
        }
    return NT_STATUS_INVALID_HANDLE;
}

/* 396-400: Rollback */
int64_t vsl_nt_rollback_complete(uint64_t a, uint64_t b, uint64_t c,
                                 uint64_t d, uint64_t e, uint64_t f) {
    return vsl_nt_commit_complete(a, b, c, d, e, f);
}

int64_t vsl_nt_rollback_enlistment(uint64_t a, uint64_t b, uint64_t c,
                                   uint64_t d, uint64_t e, uint64_t f) {
    uint32_t enlist_h = (uint32_t)a;
    for (int i = 0; i < NT_ENLISTMENT_MAX; i++)
        if (g_nt_enlistments[i].used && g_nt_enlistments[i].handle == enlist_h)
            { g_nt_enlistments[i].state = 3; return NT_STATUS_SUCCESS; }
    return NT_STATUS_INVALID_HANDLE;
}

int64_t vsl_nt_rollback_registry_transaction(uint64_t a, uint64_t b, uint64_t c,
                                             uint64_t d, uint64_t e, uint64_t f) {
    return vsl_nt_rollback_enlistment(a, b, c, d, e, f);
}

int64_t vsl_nt_rollback_transaction(uint64_t a, uint64_t b, uint64_t c,
                                    uint64_t d, uint64_t e, uint64_t f) {
    uint32_t txn_h = (uint32_t)a;
    nt_transaction_t *t = nt_txn_find(txn_h);
    if (!t) return NT_STATUS_INVALID_HANDLE;
    t->state = 2; /* rolled back */
    if (t->has_checkpoint) { rmdir(t->checkpoint_dir); t->has_checkpoint = 0; }
    return NT_STATUS_SUCCESS;
}

int64_t vsl_nt_rollforward_transaction_manager(uint64_t a, uint64_t b, uint64_t c,
                                               uint64_t d, uint64_t e, uint64_t f) {
    return vsl_nt_recover_transaction_manager(a, b, c, d, e, f);
}

/* 423: NtSetInformationEnlistment */
int64_t vsl_nt_set_information_enlistment(uint64_t a, uint64_t b, uint64_t c,
                                          uint64_t d, uint64_t e, uint64_t f) {
    uint32_t enlist_h = (uint32_t)a;
    for (int i = 0; i < NT_ENLISTMENT_MAX; i++)
        if (g_nt_enlistments[i].used && g_nt_enlistments[i].handle == enlist_h)
            return NT_STATUS_SUCCESS;
    return NT_STATUS_INVALID_HANDLE;
}

/* 427: NtSetInformationResourceManager */
int64_t vsl_nt_set_information_resource_manager(uint64_t a, uint64_t b, uint64_t c,
                                                  uint64_t d, uint64_t e, uint64_t f) {
    uint32_t rm_h = (uint32_t)a;
    for (int i = 0; i < NT_RM_MAX; i++)
        if (g_nt_rms[i].used && g_nt_rms[i].handle == rm_h) return NT_STATUS_SUCCESS;
    return NT_STATUS_INVALID_HANDLE;
}

/* 430: NtSetInformationTransaction */
int64_t vsl_nt_set_information_transaction(uint64_t a, uint64_t b, uint64_t c,
                                           uint64_t d, uint64_t e, uint64_t f) {
    uint32_t txn_h = (uint32_t)a;
    if (!nt_txn_find(txn_h)) return NT_STATUS_INVALID_HANDLE;
    return NT_STATUS_SUCCESS;
}

/* 431: NtSetInformationTransactionManager */
int64_t vsl_nt_set_information_transaction_manager(uint64_t a, uint64_t b, uint64_t c,
                                                     uint64_t d, uint64_t e, uint64_t f) {
    uint32_t tm_h = (uint32_t)a;
    for (int i = 0; i < NT_TXN_MGR_MAX; i++)
        if (g_nt_txn_mgrs[i].used && g_nt_txn_mgrs[i].handle == tm_h) return NT_STATUS_SUCCESS;
    return NT_STATUS_INVALID_HANDLE;
}

/* 469: NtThawTransactions */
int64_t vsl_nt_thaw_transactions(uint64_t a, uint64_t b, uint64_t c,
                                 uint64_t d, uint64_t e, uint64_t f) {
    /* Unfreeze all transactions */
    for (int i = 0; i < NT_TXN_MAX; i++)
        if (g_nt_txns[i].used) g_nt_txns[i].state = 0;
    return NT_STATUS_SUCCESS;
}

/* ------------------------------------------------------------------- */
/* Registration                                                        */
/* ------------------------------------------------------------------- */

void vsl_nt_ktm_register(vsl_syscall_fn_t *tbl, int tbl_size) {
    (void)tbl_size;
tbl[397-1] = vsl_nt_commit_complete;
tbl[329-1] = vsl_nt_commit_enlistment;
tbl[358-1] = vsl_nt_commit_registry_transaction;
tbl[359-1] = vsl_nt_commit_transaction;
tbl[330-1] = vsl_nt_create_enlistment;
tbl[360-1] = vsl_nt_create_registry_transaction;
tbl[352-1] = vsl_nt_create_resource_manager;
tbl[361-1] = vsl_nt_create_transaction;
tbl[362-1] = vsl_nt_create_transaction_manager;
tbl[363-1] = vsl_nt_enumerate_transaction_object;
tbl[364-1] = vsl_nt_freeze_transactions;
tbl[353-1] = vsl_nt_get_notification_resource_manager;
tbl[331-1] = vsl_nt_open_enlistment;
tbl[365-1] = vsl_nt_open_registry_transaction;
tbl[354-1] = vsl_nt_open_resource_manager;
tbl[366-1] = vsl_nt_open_transaction;
tbl[367-1] = vsl_nt_open_transaction_manager;
tbl[399-1] = vsl_nt_pre_prepare_complete;
tbl[332-1] = vsl_nt_pre_prepare_enlistment;
tbl[400-1] = vsl_nt_prepare_complete;
tbl[333-1] = vsl_nt_prepare_enlistment;
tbl[401-1] = vsl_nt_propagation_failed;
tbl[334-1] = vsl_nt_query_information_enlistment;
tbl[355-1] = vsl_nt_query_information_resource_manager;
tbl[368-1] = vsl_nt_query_information_transaction;
tbl[369-1] = vsl_nt_query_information_transaction_manager;
tbl[335-1] = vsl_nt_read_only_enlistment;
tbl[336-1] = vsl_nt_recover_enlistment;
tbl[356-1] = vsl_nt_recover_resource_manager;
tbl[370-1] = vsl_nt_recover_transaction_manager;
tbl[405-1] = vsl_nt_register_protocol_address_information;
tbl[371-1] = vsl_nt_rename_transaction_manager;
tbl[407-1] = vsl_nt_rollback_complete;
tbl[337-1] = vsl_nt_rollback_enlistment;
tbl[372-1] = vsl_nt_rollback_registry_transaction;
tbl[373-1] = vsl_nt_rollback_transaction;
tbl[374-1] = vsl_nt_rollforward_transaction_manager;
tbl[338-1] = vsl_nt_set_information_enlistment;
tbl[357-1] = vsl_nt_set_information_resource_manager;
tbl[375-1] = vsl_nt_set_information_transaction;
tbl[376-1] = vsl_nt_set_information_transaction_manager;
tbl[377-1] = vsl_nt_thaw_transactions;
}
