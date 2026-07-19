/*
 * wubu_cap_internal.h -- WuBuOS capability core object layout + globals.
 * Internal to the wubu_cap module. NOT installed as a public god-header.
 */
#ifndef WUBU_CAP_INTERNAL_H
#define WUBU_CAP_INTERNAL_H

#include "wubu_cap.h"

/* ---- Object registry globals (defined once in wubu_cap_object.c) ---- */
extern wubu_cap_object_t *g_wubu_cap_ptrs[WUBU_CAP_OBJECT_CAPACITY];
extern uint32_t           g_wubu_cap_count;
extern void              *g_wubu_cap_lock;

/* ---- cap_object_t: 96 bytes, natural alignment (mirrors GrahaOS) ---- */
struct wubu_cap_object {
    uint16_t  kind;
    uint8_t   deleted;
    uint8_t   reserved1;
    uint32_t  generation;                 /* atomic; bumped on revoke */
    uint64_t  rights;                     /* WUBU_RIGHT_* mask */
    uintptr_t kind_data;                  /* kind-specific payload pointer */
    int32_t   audience_set[WUBU_CAP_AUDIENCE_MAX];
    uint8_t   audience_count;
    uint8_t   flags;                      /* WUBU_CAP_FLAG_* */
    uint8_t   reserved2[2];
    uint32_t  parent_idx;                 /* WUBU_CAP_IDX_NONE if root */
    int32_t   owner_pid;
    uint32_t  first_child_idx;
    uint32_t  next_sibling_idx;
};

/* ---- Lock shim (host pthread; replaced by a kernel spinlock on bare metal) ---- */
void  wubu_cap_lock_acquire(void);
void  wubu_cap_lock_release(void);

/* ---- Forward decls ---- */
bool wubu_cap_validate_audience(const wubu_cap_object_t *obj, int32_t pid);
void wubu_cap_link_child(uint32_t parent_idx, uint32_t child_idx);
int  wubu_cap_revoke_cascade(uint32_t root_idx);

#endif /* WUBU_CAP_INTERNAL_H */
