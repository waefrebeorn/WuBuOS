/*
 * fw_handle.c  --  WuBuFW handle + protocol database.
 *
 * UEFI handles are opaque pointers; ours point into a fixed table so that
 * HandleProtocol/LocateHandle can validate them instead of dereferencing
 * whatever an image hands us.
 */

#include "fw.h"

#define MAX_HANDLES   32
#define MAX_IFACES    8

typedef struct {
    EFI_GUID guid;
    void    *iface;
} fw_iface;

typedef struct {
    uint32_t magic;
    fw_iface ifaces[MAX_IFACES];
    int      n;
    int      used;
} fw_handle_slot;

#define HANDLE_MAGIC 0x57554248u   /* "WUBH" */

static fw_handle_slot g_handles[MAX_HANDLES];
static int g_nhandles;

static int guid_eq(const EFI_GUID *a, const EFI_GUID *b) {
    return fw_memcmp(a, b, sizeof(EFI_GUID)) == 0;
}

EFI_HANDLE fw_efi_new_handle(void) {
    if (g_nhandles >= MAX_HANDLES) return NULL;
    fw_handle_slot *s = &g_handles[g_nhandles++];
    fw_memset(s, 0, sizeof(*s));
    s->magic = HANDLE_MAGIC;
    s->used = 1;
    return (EFI_HANDLE)s;
}

static fw_handle_slot *slot_of(EFI_HANDLE h) {
    for (int i = 0; i < g_nhandles; i++)
        if ((EFI_HANDLE)&g_handles[i] == h && g_handles[i].magic == HANDLE_MAGIC)
            return &g_handles[i];
    return NULL;
}

EFI_STATUS fw_efi_install(EFI_HANDLE h, EFI_GUID *guid, void *iface) {
    fw_handle_slot *s = slot_of(h);
    if (!s || !guid) return EFI_INVALID_PARAMETER;
    for (int i = 0; i < s->n; i++) {
        if (guid_eq(&s->ifaces[i].guid, guid)) { s->ifaces[i].iface = iface; return EFI_SUCCESS; }
    }
    if (s->n >= MAX_IFACES) return EFI_OUT_OF_RESOURCES;
    s->ifaces[s->n].guid  = *guid;
    s->ifaces[s->n].iface = iface;
    s->n++;
    return EFI_SUCCESS;
}

EFI_STATUS fw_efi_uninstall(EFI_HANDLE h, EFI_GUID *guid) {
    fw_handle_slot *s = slot_of(h);
    if (!s || !guid) return EFI_INVALID_PARAMETER;
    for (int i = 0; i < s->n; i++) {
        if (guid_eq(&s->ifaces[i].guid, guid)) {
            for (int j = i; j + 1 < s->n; j++) s->ifaces[j] = s->ifaces[j + 1];
            s->n--;
            return EFI_SUCCESS;
        }
    }
    return EFI_NOT_FOUND;
}

EFI_STATUS fw_efi_lookup(EFI_HANDLE h, EFI_GUID *guid, void **out) {
    fw_handle_slot *s = slot_of(h);
    if (!s || !guid || !out) return EFI_INVALID_PARAMETER;
    for (int i = 0; i < s->n; i++) {
        if (guid_eq(&s->ifaces[i].guid, guid)) { *out = s->ifaces[i].iface; return EFI_SUCCESS; }
    }
    return EFI_UNSUPPORTED;
}

/* Enumerate handles; when guid is NULL every handle matches. */
int fw_efi_enum(EFI_GUID *guid, EFI_HANDLE *out, int max) {
    int n = 0;
    for (int i = 0; i < g_nhandles; i++) {
        fw_handle_slot *s = &g_handles[i];
        if (!s->used) continue;
        int hit = (guid == NULL);
        for (int j = 0; !hit && j < s->n; j++)
            if (guid_eq(&s->ifaces[j].guid, guid)) hit = 1;
        if (!hit) continue;
        if (out && n < max) out[n] = (EFI_HANDLE)s;
        n++;
    }
    return n;
}

int fw_efi_handle_count(void) { return g_nhandles; }

int fw_efi_handle_protocols(EFI_HANDLE h, EFI_GUID **out, int max) {
    fw_handle_slot *s = slot_of(h);
    if (!s) return -1;
    for (int i = 0; i < s->n && i < max; i++) out[i] = &s->ifaces[i].guid;
    return s->n;
}
