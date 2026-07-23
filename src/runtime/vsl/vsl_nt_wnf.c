/*
 * vsl_nt_wnf.c -- Windows 11 WNF (Windows Notification Facility) syscalls.
 *
 * WNF is state-name based, scoped notification for system-wide and
 * per-process events. In WuBuOS we back WNF state names with real
 * files under /tmp/wubu_wnf/<name> and use inotify for subscriptions.
 *
 * 10 syscalls (Windows 11 24H2 ordinals 212-481).
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
#include <sys/inotify.h>

/* ------------------------------------------------------------------- */
/* WNF state table                                                     */
/* ------------------------------------------------------------------- */

#define NT_WNF_MAX         128
#define NT_WNF_NAME_MAX    256
#define NT_WNF_DATA_MAX    4096
#define NT_WNF_SUB_MAX     64

typedef struct {
    int      used;
    uint32_t handle;
    char     state_name[NT_WNF_NAME_MAX];   /* WNF state name string   */
    char     data_file[512];               /* backing file path       */
    size_t   data_size;
    int      inotify_wd;                   /* inotify watch (-1=none) */
} nt_wnf_state_t;

typedef struct {
    int      used;
    uint32_t handle;
    uint32_t state_handle;  /* which WNF state we subscribe to */
    int      inotify_fd;
    int      pending;       /* 1 = change happened, not yet read */
} nt_wnf_subscription_t;

static nt_wnf_state_t       g_nt_wnf_states[NT_WNF_MAX];
static nt_wnf_subscription_t g_nt_wnf_subs[NT_WNF_SUB_MAX];
static int g_nt_wnf_inited = 0;
static int g_nt_wnf_inotify_fd = -1;
static char g_nt_wnf_root[512] = {0};

static void nt_wnf_ensure_init(void) {
    if (g_nt_wnf_inited) return;
    memset(g_nt_wnf_states, 0, sizeof(g_nt_wnf_states));
    memset(g_nt_wnf_subs, 0, sizeof(g_nt_wnf_subs));
    snprintf(g_nt_wnf_root, sizeof(g_nt_wnf_root), "/tmp/wubu_wnf_%d", (int)getpid());
    mkdir(g_nt_wnf_root, 0755);
    g_nt_wnf_inotify_fd = inotify_init1(IN_NONBLOCK);
    g_nt_wnf_inited = 1;
}

static nt_wnf_state_t *nt_wnf_find(uint32_t handle) {
    for (int i = 0; i < NT_WNF_MAX; i++)
        if (g_nt_wnf_states[i].used && g_nt_wnf_states[i].handle == handle)
            return &g_nt_wnf_states[i];
    return NULL;
}

static nt_wnf_state_t *nt_wnf_find_by_name(const char *name) {
    for (int i = 0; i < NT_WNF_MAX; i++)
        if (g_nt_wnf_states[i].used &&
            strcmp(g_nt_wnf_states[i].state_name, name) == 0)
            return &g_nt_wnf_states[i];
    return NULL;
}

static nt_wnf_state_t *nt_wnf_alloc(uint32_t *out_h) {
    nt_wnf_ensure_init();
    for (int i = 0; i < NT_WNF_MAX; i++) {
        if (!g_nt_wnf_states[i].used) {
            uint32_t h = 0xA000 + (uint32_t)i;
            g_nt_wnf_states[i].used = 1;
            g_nt_wnf_states[i].handle = h;
            g_nt_wnf_states[i].inotify_wd = -1;
            g_nt_wnf_states[i].data_size = 0;
            *out_h = h;
            return &g_nt_wnf_states[i];
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------- */
/* Handlers                                                            */
/* ------------------------------------------------------------------- */

/* 212: NtCreateWnfStateName */
int64_t vsl_nt_create_wnf_state_name(uint64_t a, uint64_t b, uint64_t c,
                                     uint64_t d, uint64_t e, uint64_t f) {
    /* a = out state name handle, b = StateName (WNF_STATE_NAME struct ptr)
     * c = TypeName, d = PersistScope, e = SecurityDescriptor, f = out StateHandle */
    uint32_t h;
    nt_wnf_state_t *s = nt_wnf_alloc(&h);
    if (!s) return NT_STATUS_INSUFFICIENT_RESOURCES;
    if (b) {
        /* State name is a binary blob; stringify it as hex */
        const uint8_t *name_bytes = (const uint8_t *)(void *)b;
        snprintf(s->state_name, sizeof(s->state_name), "wnf_%02x%02x%02x%02x",
                 name_bytes[0], name_bytes[1], name_bytes[2], name_bytes[3]);
    } else {
        snprintf(s->state_name, sizeof(s->state_name), "wnf_anon_%u", h);
    }
    snprintf(s->data_file, sizeof(s->data_file), "%s/%s", g_nt_wnf_root, s->state_name);
    /* Create the backing file */
    int fd = open(s->data_file, O_CREAT | O_WRONLY, 0644);
    if (fd >= 0) close(fd);
    /* Set up inotify watch for subscriptions */
    if (g_nt_wnf_inotify_fd >= 0) {
        s->inotify_wd = inotify_add_watch(g_nt_wnf_inotify_fd, s->data_file,
                                           IN_MODIFY | IN_CLOSE_WRITE);
    }
    if (g_nt_ctx && (a || f)) {
        if (a) *(uint32_t *)a = h;
        else if (f) *(uint32_t *)f = h;
    }
    return NT_STATUS_SUCCESS;
}

/* 224: NtDeleteWnfStateData */
int64_t vsl_nt_delete_wnf_state_data(uint64_t a, uint64_t b, uint64_t c,
                                     uint64_t d, uint64_t e, uint64_t f) {
    /* a = StateHandle (or StateName) */
    uint32_t state_h = (uint32_t)a;
    nt_wnf_state_t *s = nt_wnf_find(state_h);
    if (!s) {
        /* Maybe it's a name-based lookup */
        if (b) {
            const uint8_t *nb = (const uint8_t *)(void *)b;
            char name[64];
            snprintf(name, sizeof(name), "wnf_%02x%02x%02x%02x", nb[0], nb[1], nb[2], nb[3]);
            s = nt_wnf_find_by_name(name);
        }
        if (!s) return NT_STATUS_INVALID_HANDLE;
    }
    /* Truncate the data file */
    int fd = open(s->data_file, O_TRUNC | O_WRONLY, 0644);
    if (fd >= 0) close(fd);
    s->data_size = 0;
    return NT_STATUS_SUCCESS;
}

/* 225: NtDeleteWnfStateName */
int64_t vsl_nt_delete_wnf_state_name(uint64_t a, uint64_t b, uint64_t c,
                                     uint64_t d, uint64_t e, uint64_t f) {
    uint32_t state_h = (uint32_t)a;
    nt_wnf_state_t *s = nt_wnf_find(state_h);
    if (!s) return NT_STATUS_INVALID_HANDLE;
    /* Remove backing file */
    unlink(s->data_file);
    if (s->inotify_wd >= 0 && g_nt_wnf_inotify_fd >= 0)
        inotify_rm_watch(g_nt_wnf_inotify_fd, s->inotify_wd);
    s->used = 0;
    return NT_STATUS_SUCCESS;
}

/* 249: NtGetCompleteWnfStateSubscription */
int64_t vsl_nt_get_complete_wnf_state_subscription(uint64_t a, uint64_t b, uint64_t c,
                                                    uint64_t d, uint64_t e, uint64_t f) {
    /* Return all subscriptions for a state name */
    if (e) *(uint32_t *)e = 0; /* no subscriptions in our simple model */
    return NT_STATUS_SUCCESS;
}

/* 368: NtQueryWnfStateData */
int64_t vsl_nt_query_wnf_state_data(uint64_t a, uint64_t b, uint64_t c,
                                    uint64_t d, uint64_t e, uint64_t f) {
    /* a = StateName, b = out buffer, c = buffer size, d = out bytes */
    uint32_t state_h = (uint32_t)a;
    nt_wnf_state_t *s = nt_wnf_find(state_h);
    if (!s) {
        if (a) {
            const uint8_t *nb = (const uint8_t *)(void *)a;
            char name[64];
            snprintf(name, sizeof(name), "wnf_%02x%02x%02x%02x", nb[0], nb[1], nb[2], nb[3]);
            s = nt_wnf_find_by_name(name);
        }
        if (!s) return NT_STATUS_OBJECT_NAME_NOT_FOUND;
    }
    /* Read the data from the backing file */
    int fd = open(s->data_file, O_RDONLY);
    if (fd < 0) return NT_STATUS_OBJECT_NAME_NOT_FOUND;
    ssize_t rd = read(fd, (void *)b, (size_t)c);
    close(fd);
    if (rd < 0) return NT_STATUS_UNSUCCESSFUL;
    if (d) *(uint32_t *)d = (uint32_t)rd;
    return NT_STATUS_SUCCESS;
}

/* 369: NtQueryWnfStateNameInformation */
int64_t vsl_nt_query_wnf_state_name_information(uint64_t a, uint64_t b, uint64_t c,
                                                uint64_t d, uint64_t e, uint64_t f) {
    uint32_t state_h = (uint32_t)a;
    nt_wnf_state_t *s = nt_wnf_find(state_h);
    if (!s) return NT_STATUS_INVALID_HANDLE;
    if (c && d >= 4) {
        *(uint32_t *)c = (uint32_t)s->data_size;
    }
    if (e) *(uint32_t *)e = 4;
    return NT_STATUS_SUCCESS;
}

/* 453: NtSetWnfProcessNotificationEvent */
int64_t vsl_nt_set_wnf_process_notification_event(uint64_t a, uint64_t b, uint64_t c,
                                                   uint64_t d, uint64_t e, uint64_t f) {
    /* Set the per-process WNF notification event handle */
    if (g_nt_ctx && a) g_nt_ctx->wnf_notify_event = (uint32_t)a;
    return NT_STATUS_SUCCESS;
}

/* 461: NtSubscribeWnfStateChange */
int64_t vsl_nt_subscribe_wnf_state_change(uint64_t a, uint64_t b, uint64_t c,
                                          uint64_t d, uint64_t e, uint64_t f) {
    /* a = StateName, e = out subscription handle */
    uint32_t state_h = (uint32_t)a;
    nt_wnf_state_t *s = nt_wnf_find(state_h);
    if (!s) return NT_STATUS_INVALID_HANDLE;
    /* Allocate a subscription */
    nt_wnf_ensure_init();
    for (int i = 0; i < NT_WNF_SUB_MAX; i++) {
        if (!g_nt_wnf_subs[i].used) {
            uint32_t sub_h = 0xB000 + (uint32_t)i;
            g_nt_wnf_subs[i].used = 1;
            g_nt_wnf_subs[i].handle = sub_h;
            g_nt_wnf_subs[i].state_handle = state_h;
            g_nt_wnf_subs[i].inotify_fd = g_nt_wnf_inotify_fd;
            g_nt_wnf_subs[i].pending = 0;
            if (e) *(uint32_t *)e = sub_h;
            return NT_STATUS_SUCCESS;
        }
    }
    return NT_STATUS_INSUFFICIENT_RESOURCES;
}

/* 480: NtUnsubscribeWnfStateChange */
int64_t vsl_nt_unsubscribe_wnf_state_change(uint64_t a, uint64_t b, uint64_t c,
                                            uint64_t d, uint64_t e, uint64_t f) {
    uint32_t sub_h = (uint32_t)a;
    for (int i = 0; i < NT_WNF_SUB_MAX; i++) {
        if (g_nt_wnf_subs[i].used && g_nt_wnf_subs[i].handle == sub_h) {
            g_nt_wnf_subs[i].used = 0;
            return NT_STATUS_SUCCESS;
        }
    }
    return NT_STATUS_INVALID_HANDLE;
}

/* 481: NtUpdateWnfStateData */
int64_t vsl_nt_update_wnf_state_data(uint64_t a, uint64_t b, uint64_t c,
                                     uint64_t d, uint64_t e, uint64_t f) {
    /* a = StateName/handle, b = buffer, c = buffer size */
    uint32_t state_h = (uint32_t)a;
    nt_wnf_state_t *s = nt_wnf_find(state_h);
    if (!s) {
        if (a) {
            const uint8_t *nb = (const uint8_t *)(void *)a;
            char name[64];
            snprintf(name, sizeof(name), "wnf_%02x%02x%02x%02x", nb[0], nb[1], nb[2], nb[3]);
            s = nt_wnf_find_by_name(name);
        }
        if (!s) return NT_STATUS_OBJECT_NAME_NOT_FOUND;
    }
    /* Write the data to the backing file */
    int fd = open(s->data_file, O_TRUNC | O_WRONLY, 0644);
    if (fd < 0) return NT_STATUS_UNSUCCESSFUL;
    ssize_t wr = write(fd, (void *)b, (size_t)c);
    close(fd);
    if (wr < 0) return NT_STATUS_UNSUCCESSFUL;
    s->data_size = (size_t)wr;
    /* Mark any subscriptions as pending */
    for (int i = 0; i < NT_WNF_SUB_MAX; i++)
        if (g_nt_wnf_subs[i].used && g_nt_wnf_subs[i].state_handle == state_h)
            g_nt_wnf_subs[i].pending = 1;
    return NT_STATUS_SUCCESS;
}

/* ------------------------------------------------------------------- */
/* Registration                                                        */
/* ------------------------------------------------------------------- */

void vsl_nt_wnf_register(vsl_syscall_fn_t *tbl, int tbl_size) {
    (void)tbl_size;
tbl[378-1] = vsl_nt_create_wnf_state_name;
tbl[379-1] = vsl_nt_delete_wnf_state_data;
tbl[380-1] = vsl_nt_delete_wnf_state_name;
tbl[381-1] = vsl_nt_get_complete_wnf_state_subscription;
tbl[382-1] = vsl_nt_query_wnf_state_data;
tbl[383-1] = vsl_nt_query_wnf_state_name_information;
tbl[384-1] = vsl_nt_set_wnf_process_notification_event;
tbl[385-1] = vsl_nt_subscribe_wnf_state_change;
tbl[386-1] = vsl_nt_unsubscribe_wnf_state_change;
tbl[387-1] = vsl_nt_update_wnf_state_data;
}
