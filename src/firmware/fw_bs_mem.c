/*
 * fw_bs_mem.c  --  Boot services: TPL, memory, events, timers, misc.
 */

#include "fw.h"

extern EFI_EVENT g_wait_for_key;

static EFI_TPL g_tpl = TPL_APPLICATION;
static UINT64  g_mono = 1;

EFI_TPL EFIAPI fw_bs_raise_tpl(EFI_TPL n) {
    EFI_TPL old = g_tpl;
    if (n > g_tpl) g_tpl = n;
    return old;
}

VOID EFIAPI fw_bs_restore_tpl(EFI_TPL old) { g_tpl = old; }

EFI_STATUS EFIAPI fw_bs_alloc_pages(EFI_ALLOCATE_TYPE type, EFI_MEMORY_TYPE mt,
                                    UINTN pages, EFI_PHYSICAL_ADDRESS *mem) {
    if (!mem || !pages) return EFI_INVALID_PARAMETER;
    void *p = NULL;
    switch (type) {
    case AllocateAnyPages:
        p = fw_alloc_pages(pages, mt);
        break;
    case AllocateMaxAddress:
        p = fw_alloc_pages(pages, mt);
        if (p && (uint64_t)(uintptr_t)p + pages * EFI_PAGE_SIZE > *mem) {
            fw_free_pages(p, pages);
            p = NULL;
        }
        break;
    case AllocateAddress:
        p = fw_alloc_pages_at(*mem, pages, mt);
        break;
    default:
        return EFI_INVALID_PARAMETER;
    }
    if (!p) return EFI_OUT_OF_RESOURCES;
    *mem = (EFI_PHYSICAL_ADDRESS)(uintptr_t)p;
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI fw_bs_free_pages(EFI_PHYSICAL_ADDRESS mem, UINTN pages) {
    if (!mem || !pages) return EFI_INVALID_PARAMETER;
    fw_free_pages((void *)(uintptr_t)mem, pages);
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI fw_bs_get_memory_map(UINTN *size, EFI_MEMORY_DESCRIPTOR *map,
                                       UINTN *key, UINTN *dsize, UINT32 *dver) {
    if (!size) return EFI_INVALID_PARAMETER;
    size_t need_entries = fw_mem_map_build(NULL, 0);
    UINTN desc = sizeof(EFI_MEMORY_DESCRIPTOR);
    UINTN need = need_entries * desc;

    if (dsize) *dsize = desc;
    if (dver)  *dver  = 1;
    if (key)   *key   = (UINTN)fw_mem_map_key();

    if (*size < need || !map) { *size = need; return EFI_BUFFER_TOO_SMALL; }
    fw_mem_map_build(map, need_entries);
    *size = need;
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI fw_bs_alloc_pool(EFI_MEMORY_TYPE type, UINTN size, VOID **buf) {
    if (!buf) return EFI_INVALID_PARAMETER;
    if (!size) { *buf = NULL; return EFI_SUCCESS; }
    void *p = fw_pool_alloc(size, type);
    if (!p) return EFI_OUT_OF_RESOURCES;
    *buf = p;
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI fw_bs_free_pool(VOID *buf) {
    fw_pool_free(buf);
    return EFI_SUCCESS;
}

/* -- events --------------------------------------------------------- */

#define MAX_EVENTS 32

typedef struct {
    int              used;
    int              signaled;
    UINT32           type;
    EFI_TPL          tpl;
    EFI_EVENT_NOTIFY notify;
    void            *ctx;
    uint64_t         deadline_us;   /* 0 = no timer */
    uint64_t         period_us;
} fw_event;

static fw_event g_events[MAX_EVENTS];

static fw_event *ev_of(EFI_EVENT e) {
    for (int i = 0; i < MAX_EVENTS; i++)
        if (g_events[i].used && (EFI_EVENT)&g_events[i] == e) return &g_events[i];
    return NULL;
}

/* Exposed so ExitBootServices can fire its notification group. */
void fw_events_signal_exit(void) {
    for (int i = 0; i < MAX_EVENTS; i++) {
        fw_event *e = &g_events[i];
        if (e->used && (e->type & EVT_SIGNAL_EXIT_BOOT_SERVICES) == EVT_SIGNAL_EXIT_BOOT_SERVICES
            && e->notify) {
            e->signaled = 1;
            e->notify((EFI_EVENT)e, e->ctx);
        }
    }
}

EFI_STATUS EFIAPI fw_bs_create_event(UINT32 type, EFI_TPL tpl, EFI_EVENT_NOTIFY fn,
                                     VOID *ctx, EFI_EVENT *out) {
    if (!out) return EFI_INVALID_PARAMETER;
    for (int i = 0; i < MAX_EVENTS; i++) {
        if (g_events[i].used) continue;
        fw_memset(&g_events[i], 0, sizeof(fw_event));
        g_events[i].used   = 1;
        g_events[i].type   = type;
        g_events[i].tpl    = tpl;
        g_events[i].notify = fn;
        g_events[i].ctx    = ctx;
        *out = (EFI_EVENT)&g_events[i];
        return EFI_SUCCESS;
    }
    return EFI_OUT_OF_RESOURCES;
}

EFI_STATUS EFIAPI fw_bs_create_event_ex(UINT32 type, EFI_TPL tpl, EFI_EVENT_NOTIFY fn,
                                        const VOID *ctx, const EFI_GUID *group, EFI_EVENT *out) {
    (void)group;
    return fw_bs_create_event(type, tpl, fn, (VOID *)(uintptr_t)ctx, out);
}

EFI_STATUS EFIAPI fw_bs_close_event(EFI_EVENT e) {
    fw_event *ev = ev_of(e);
    if (!ev) return EFI_INVALID_PARAMETER;
    ev->used = 0;
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI fw_bs_signal_event(EFI_EVENT e) {
    fw_event *ev = ev_of(e);
    if (!ev) return EFI_INVALID_PARAMETER;
    ev->signaled = 1;
    if (ev->notify && (ev->type & EVT_NOTIFY_SIGNAL)) ev->notify(e, ev->ctx);
    return EFI_SUCCESS;
}

/* Real elapsed microseconds since the first call, from the TSC calibrated
 * in fw_time_init(). A synthetic counter here would make every SetTimer
 * deadline meaningless, so this reads the hardware. */
static uint64_t now_us(void) {
    static uint64_t t0;
    static uint64_t per_us;
    if (!per_us) {
        uint64_t a = rdtsc();
        fw_stall_us(1000);
        uint64_t b = rdtsc();
        per_us = (b - a) / 1000;
        if (!per_us) per_us = 1;
        t0 = b;
    }
    uint64_t now = rdtsc();
    return (now - t0) / per_us;
}

EFI_STATUS EFIAPI fw_bs_set_timer(EFI_EVENT e, EFI_TIMER_DELAY type, UINT64 trigger) {
    fw_event *ev = ev_of(e);
    if (!ev) return EFI_INVALID_PARAMETER;
    uint64_t us = trigger / 10;      /* 100ns units -> us */
    switch (type) {
    case TimerCancel:   ev->deadline_us = 0; ev->period_us = 0; break;
    case TimerRelative: ev->deadline_us = now_us() + us; ev->period_us = 0; break;
    case TimerPeriodic: ev->deadline_us = now_us() + us; ev->period_us = us; break;
    default: return EFI_INVALID_PARAMETER;
    }
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI fw_bs_check_event(EFI_EVENT e) {
    if (e == g_wait_for_key) return EFI_NOT_READY;   /* polled via ConIn */
    fw_event *ev = ev_of(e);
    if (!ev) return EFI_INVALID_PARAMETER;
    if (ev->deadline_us && now_us() >= ev->deadline_us) {
        ev->signaled = 1;
        ev->deadline_us = ev->period_us ? now_us() + ev->period_us : 0;
    }
    if (ev->signaled) { ev->signaled = 0; return EFI_SUCCESS; }
    if (ev->notify && (ev->type & EVT_NOTIFY_WAIT)) ev->notify(e, ev->ctx);
    return EFI_NOT_READY;
}

EFI_STATUS EFIAPI fw_bs_wait_for_event(UINTN n, EFI_EVENT *evs, UINTN *index) {
    if (!n || !evs) return EFI_INVALID_PARAMETER;
    for (;;) {
        for (UINTN i = 0; i < n; i++) {
            if (evs[i] == g_wait_for_key) {
                if (fw_getc_nb() >= 0) { if (index) *index = i; return EFI_SUCCESS; }
                continue;
            }
            if (fw_bs_check_event(evs[i]) == EFI_SUCCESS) {
                if (index) *index = i;
                return EFI_SUCCESS;
            }
        }
        __asm__ volatile("pause");
    }
}

/* -- misc ----------------------------------------------------------- */

EFI_STATUS EFIAPI fw_bs_get_mono(UINT64 *count) {
    if (!count) return EFI_INVALID_PARAMETER;
    *count = g_mono++;
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI fw_bs_stall(UINTN us) {
    fw_stall_us(us);
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI fw_bs_set_watchdog(UINTN t, UINT64 code, UINTN dsz, CHAR16 *data) {
    (void)t; (void)code; (void)dsz; (void)data;
    return EFI_SUCCESS;      /* no watchdog: never spuriously reset a boot */
}

VOID EFIAPI fw_bs_copy_mem(VOID *d, VOID *s, UINTN n) { fw_memcpy(d, s, n); }
VOID EFIAPI fw_bs_set_mem(VOID *b, UINTN n, UINT8 v)  { fw_memset(b, v, n); }

EFI_STATUS EFIAPI fw_bs_crc32(VOID *data, UINTN size, UINT32 *out) {
    if (!data || !out || !size) return EFI_INVALID_PARAMETER;
    static uint32_t table[256];
    static int init;
    if (!init) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        init = 1;
    }
    uint32_t crc = 0xFFFFFFFFu;
    const uint8_t *p = (const uint8_t *)data;
    for (UINTN i = 0; i < size; i++) crc = table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    *out = crc ^ 0xFFFFFFFFu;
    return EFI_SUCCESS;
}
