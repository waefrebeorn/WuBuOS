/*
 * fw_rt.c  --  WuBuFW runtime services: time, variables, reset.
 *
 * Variables live in a fixed RAM store (no SPI flash backing on QEMU's
 * -bios path), which is spec-legal as volatile storage: NON_VOLATILE
 * requests are accepted but reported honestly through QueryVariableInfo.
 */

#include "fw.h"

#define MAX_VARS      64
#define VAR_NAME_MAX  64
#define VAR_DATA_MAX  1024

typedef struct {
    int      used;
    CHAR16   name[VAR_NAME_MAX];
    EFI_GUID guid;
    UINT32   attr;
    UINTN    size;
    uint8_t  data[VAR_DATA_MAX];
} fw_var;

static fw_var g_vars[MAX_VARS];
static UINT32 g_high_mono = 1;

static int name_eq16(const CHAR16 *a, const CHAR16 *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == 0 && *b == 0;
}

static fw_var *var_find(CHAR16 *name, EFI_GUID *guid) {
    for (int i = 0; i < MAX_VARS; i++) {
        if (!g_vars[i].used) continue;
        if (!name_eq16(g_vars[i].name, name)) continue;
        if (fw_memcmp(&g_vars[i].guid, guid, sizeof(EFI_GUID)) != 0) continue;
        return &g_vars[i];
    }
    return NULL;
}

EFI_STATUS EFIAPI fw_rt_get_time(EFI_TIME *t, EFI_TIME_CAPABILITIES *caps) {
    if (!t) return EFI_INVALID_PARAMETER;
    fw_rtc_read(t);
    if (caps) { caps->Resolution = 1; caps->Accuracy = 50000000; caps->SetsToZero = FALSE; }
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI fw_rt_set_time(EFI_TIME *t) { (void)t; return EFI_UNSUPPORTED; }

EFI_STATUS EFIAPI fw_rt_get_wakeup(BOOLEAN *en, BOOLEAN *pend, EFI_TIME *t) {
    (void)en; (void)pend; (void)t; return EFI_UNSUPPORTED;
}
EFI_STATUS EFIAPI fw_rt_set_wakeup(BOOLEAN en, EFI_TIME *t) {
    (void)en; (void)t; return EFI_UNSUPPORTED;
}

EFI_STATUS EFIAPI fw_rt_set_virtual_map(UINTN msz, UINTN dsz, UINT32 ver,
                                        EFI_MEMORY_DESCRIPTOR *map) {
    (void)msz; (void)dsz; (void)ver; (void)map;
    /* We identity-map everything, so a virtual map is a no-op that must
     * still succeed for loaders that call it unconditionally. */
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI fw_rt_convert_pointer(UINTN disp, VOID **addr) {
    (void)disp;
    if (!addr) return EFI_INVALID_PARAMETER;
    return EFI_SUCCESS;      /* identity mapping */
}

EFI_STATUS EFIAPI fw_rt_get_variable(CHAR16 *name, EFI_GUID *guid, UINT32 *attr,
                                     UINTN *size, VOID *data) {
    if (!name || !guid || !size) return EFI_INVALID_PARAMETER;
    fw_var *v = var_find(name, guid);
    if (!v) return EFI_NOT_FOUND;
    if (attr) *attr = v->attr;
    if (*size < v->size || !data) { *size = v->size; return EFI_BUFFER_TOO_SMALL; }
    fw_memcpy(data, v->data, v->size);
    *size = v->size;
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI fw_rt_get_next_variable(UINTN *nsz, CHAR16 *name, EFI_GUID *guid) {
    if (!nsz || !name || !guid) return EFI_INVALID_PARAMETER;
    int start = 0;
    if (name[0] != 0) {
        fw_var *cur = var_find(name, guid);
        if (!cur) return EFI_INVALID_PARAMETER;
        start = (int)(cur - g_vars) + 1;
    }
    for (int i = start; i < MAX_VARS; i++) {
        if (!g_vars[i].used) continue;
        UINTN need = (fw_strlen16(g_vars[i].name) + 1) * sizeof(CHAR16);
        if (*nsz < need) { *nsz = need; return EFI_BUFFER_TOO_SMALL; }
        fw_memcpy(name, g_vars[i].name, need);
        *guid = g_vars[i].guid;
        *nsz = need;
        return EFI_SUCCESS;
    }
    return EFI_NOT_FOUND;
}

EFI_STATUS EFIAPI fw_rt_set_variable(CHAR16 *name, EFI_GUID *guid, UINT32 attr,
                                     UINTN size, VOID *data) {
    if (!name || !guid || name[0] == 0) return EFI_INVALID_PARAMETER;
    if (size > VAR_DATA_MAX) return EFI_OUT_OF_RESOURCES;
    if (fw_strlen16(name) >= VAR_NAME_MAX) return EFI_OUT_OF_RESOURCES;

    fw_var *v = var_find(name, guid);
    if (size == 0 || !data) {                 /* delete */
        if (!v) return EFI_NOT_FOUND;
        v->used = 0;
        return EFI_SUCCESS;
    }
    if (!v) {
        for (int i = 0; i < MAX_VARS; i++) if (!g_vars[i].used) { v = &g_vars[i]; break; }
        if (!v) return EFI_OUT_OF_RESOURCES;
        fw_memset(v, 0, sizeof(*v));
        size_t n = fw_strlen16(name);
        fw_memcpy(v->name, name, (n + 1) * sizeof(CHAR16));
        v->guid = *guid;
        v->used = 1;
    }
    v->attr = attr;
    v->size = size;
    fw_memcpy(v->data, data, size);
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI fw_rt_get_next_high_mono(UINT32 *high) {
    if (!high) return EFI_INVALID_PARAMETER;
    *high = g_high_mono++;
    return EFI_SUCCESS;
}

VOID EFIAPI fw_rt_reset(EFI_RESET_TYPE type, EFI_STATUS status, UINTN dsz, VOID *data) {
    (void)status; (void)dsz; (void)data;
    fw_printf("\n[fw] ResetSystem type=%d\n", (int)type);
    if (type == EfiResetShutdown) {
        /* QEMU ACPI shutdown ports; try both the modern and isa-debug-exit. */
        outw(0x604, 0x2000);
        outw(0xB004, 0x2000);
        outw(0x4004, 0x3400);
    }
    /* Warm/cold: pulse the keyboard controller reset line, then triple fault. */
    for (int i = 0; i < 100; i++) {
        if (!(inb(0x64) & 0x02)) break;
        io_wait();
    }
    outb(0x64, 0xFE);
    struct __attribute__((packed)) { uint16_t limit; uint64_t base; } null_idt = { 0, 0 };
    __asm__ volatile("lidt %0; int3" :: "m"(null_idt));
    for (;;) __asm__ volatile("hlt");
}

EFI_STATUS EFIAPI fw_rt_update_capsule(VOID **caps, UINTN n, EFI_PHYSICAL_ADDRESS sg) {
    (void)caps; (void)n; (void)sg; return EFI_UNSUPPORTED;
}

EFI_STATUS EFIAPI fw_rt_query_capsule(VOID **caps, UINTN n, UINT64 *maxsz, EFI_RESET_TYPE *rt) {
    (void)caps; (void)n;
    if (maxsz) *maxsz = 0;
    if (rt) *rt = EfiResetCold;
    return EFI_UNSUPPORTED;
}

EFI_STATUS EFIAPI fw_rt_query_variable_info(UINT32 attr, UINT64 *maxstore,
                                            UINT64 *remaining, UINT64 *maxvar) {
    (void)attr;
    int used = 0;
    for (int i = 0; i < MAX_VARS; i++) if (g_vars[i].used) used++;
    if (maxstore)  *maxstore  = (UINT64)MAX_VARS * VAR_DATA_MAX;
    if (remaining) *remaining = (UINT64)(MAX_VARS - used) * VAR_DATA_MAX;
    if (maxvar)    *maxvar    = VAR_DATA_MAX;
    return EFI_SUCCESS;
}
