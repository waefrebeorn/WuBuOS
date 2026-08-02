/*
 * fw_bs_proto.c  --  Boot services: protocol database + image services.
 */

#include "fw.h"

EFI_STATUS fw_efi_uninstall(EFI_HANDLE h, EFI_GUID *guid);
EFI_STATUS fw_efi_lookup(EFI_HANDLE h, EFI_GUID *guid, void **out);
int        fw_efi_enum(EFI_GUID *guid, EFI_HANDLE *out, int max);
int        fw_efi_handle_protocols(EFI_HANDLE h, EFI_GUID **out, int max);
void       fw_events_signal_exit(void);
extern EFI_SYSTEM_TABLE *g_systab;

static int g_bs_active = 1;
int fw_efi_boot_services_active(void) { return g_bs_active; }

/* -- protocol handlers ---------------------------------------------- */

EFI_STATUS EFIAPI fw_bs_install_protocol(EFI_HANDLE *h, EFI_GUID *g,
                                         EFI_INTERFACE_TYPE t, VOID *iface) {
    if (!h || !g) return EFI_INVALID_PARAMETER;
    if (t != EFI_NATIVE_INTERFACE) return EFI_INVALID_PARAMETER;
    if (!*h) { *h = fw_efi_new_handle(); if (!*h) return EFI_OUT_OF_RESOURCES; }
    return fw_efi_install(*h, g, iface);
}

EFI_STATUS EFIAPI fw_bs_reinstall_protocol(EFI_HANDLE h, EFI_GUID *g, VOID *old, VOID *new_) {
    (void)old;
    return fw_efi_install(h, g, new_);
}

EFI_STATUS EFIAPI fw_bs_uninstall_protocol(EFI_HANDLE h, EFI_GUID *g, VOID *iface) {
    (void)iface;
    return fw_efi_uninstall(h, g);
}

EFI_STATUS EFIAPI fw_bs_handle_protocol(EFI_HANDLE h, EFI_GUID *g, VOID **iface) {
    return fw_efi_lookup(h, g, iface);
}

EFI_STATUS EFIAPI fw_bs_open_protocol(EFI_HANDLE h, EFI_GUID *g, VOID **iface,
                                      EFI_HANDLE agent, EFI_HANDLE ctrl, UINT32 attr) {
    (void)agent; (void)ctrl; (void)attr;
    if (!iface) {                       /* TEST_PROTOCOL form */
        void *tmp;
        return fw_efi_lookup(h, g, &tmp);
    }
    return fw_efi_lookup(h, g, iface);
}

EFI_STATUS EFIAPI fw_bs_close_protocol(EFI_HANDLE h, EFI_GUID *g, EFI_HANDLE a, EFI_HANDLE c) {
    (void)h; (void)g; (void)a; (void)c;
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI fw_bs_open_protocol_info(EFI_HANDLE h, EFI_GUID *g, VOID **buf, UINTN *cnt) {
    (void)h; (void)g;
    if (buf) *buf = NULL;
    if (cnt) *cnt = 0;
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI fw_bs_register_notify(EFI_GUID *g, EFI_EVENT e, VOID **reg) {
    (void)g; (void)e;
    if (reg) *reg = NULL;
    return EFI_UNSUPPORTED;
}

EFI_STATUS EFIAPI fw_bs_locate_handle(EFI_LOCATE_SEARCH_TYPE type, EFI_GUID *g, VOID *key,
                                      UINTN *bufsize, EFI_HANDLE *buf) {
    if (!bufsize) return EFI_INVALID_PARAMETER;
    (void)key;
    EFI_GUID *filter = (type == ByProtocol) ? g : NULL;
    if (type == ByRegisterNotify) return EFI_UNSUPPORTED;

    int n = fw_efi_enum(filter, NULL, 0);
    UINTN need = (UINTN)n * sizeof(EFI_HANDLE);
    if (n == 0) { *bufsize = 0; return EFI_NOT_FOUND; }
    if (*bufsize < need || !buf) { *bufsize = need; return EFI_BUFFER_TOO_SMALL; }
    fw_efi_enum(filter, buf, n);
    *bufsize = need;
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI fw_bs_locate_handle_buffer(EFI_LOCATE_SEARCH_TYPE type, EFI_GUID *g, VOID *key,
                                             UINTN *nhandles, EFI_HANDLE **buf) {
    if (!nhandles || !buf) return EFI_INVALID_PARAMETER;
    (void)key;
    EFI_GUID *filter = (type == ByProtocol) ? g : NULL;
    int n = fw_efi_enum(filter, NULL, 0);
    if (n == 0) { *nhandles = 0; *buf = NULL; return EFI_NOT_FOUND; }
    EFI_HANDLE *out = fw_pool_alloc((size_t)n * sizeof(EFI_HANDLE), EfiBootServicesData);
    if (!out) return EFI_OUT_OF_RESOURCES;
    fw_efi_enum(filter, out, n);
    *nhandles = (UINTN)n;
    *buf = out;
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI fw_bs_locate_protocol(EFI_GUID *g, VOID *reg, VOID **iface) {
    (void)reg;
    if (!g || !iface) return EFI_INVALID_PARAMETER;
    EFI_HANDLE h[16];
    int n = fw_efi_enum(g, h, 16);
    if (n <= 0) return EFI_NOT_FOUND;
    return fw_efi_lookup(h[0], g, iface);
}

EFI_STATUS EFIAPI fw_bs_locate_device_path(EFI_GUID *g, EFI_DEVICE_PATH_PROTOCOL **dp,
                                           EFI_HANDLE *dev) {
    if (!g || !dp || !dev) return EFI_INVALID_PARAMETER;
    EFI_HANDLE h[16];
    int n = fw_efi_enum(g, h, 16);
    if (n <= 0) return EFI_NOT_FOUND;
    *dev = h[0];
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI fw_bs_protocols_per_handle(EFI_HANDLE h, EFI_GUID ***out, UINTN *cnt) {
    if (!out || !cnt) return EFI_INVALID_PARAMETER;
    EFI_GUID *tmp[8];
    int n = fw_efi_handle_protocols(h, tmp, 8);
    if (n < 0) return EFI_INVALID_PARAMETER;
    EFI_GUID **buf = fw_pool_alloc((size_t)n * sizeof(EFI_GUID *), EfiBootServicesData);
    if (!buf) return EFI_OUT_OF_RESOURCES;
    for (int i = 0; i < n; i++) buf[i] = tmp[i];
    *out = buf;
    *cnt = (UINTN)n;
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI fw_bs_install_multiple(EFI_HANDLE *h, ...) {
    /* Variadic ms_abi with GUID/iface pairs terminated by NULL. */
    __builtin_va_list ap;
    if (!h) return EFI_INVALID_PARAMETER;
    if (!*h) { *h = fw_efi_new_handle(); if (!*h) return EFI_OUT_OF_RESOURCES; }
    __builtin_va_start(ap, h);
    for (;;) {
        EFI_GUID *g = __builtin_va_arg(ap, EFI_GUID *);
        if (!g) break;
        void *iface = __builtin_va_arg(ap, void *);
        EFI_STATUS s = fw_efi_install(*h, g, iface);
        if (EFI_ERROR(s)) { __builtin_va_end(ap); return s; }
    }
    __builtin_va_end(ap);
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI fw_bs_uninstall_multiple(EFI_HANDLE h, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, h);
    for (;;) {
        EFI_GUID *g = __builtin_va_arg(ap, EFI_GUID *);
        if (!g) break;
        (void)__builtin_va_arg(ap, void *);
        fw_efi_uninstall(h, g);
    }
    __builtin_va_end(ap);
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI fw_bs_install_config_table(EFI_GUID *g, VOID *table) {
    if (!g || !g_systab) return EFI_INVALID_PARAMETER;
    EFI_CONFIGURATION_TABLE *ct = g_systab->ConfigurationTable;
    for (UINTN i = 0; i < g_systab->NumberOfTableEntries; i++) {
        if (fw_memcmp(&ct[i].VendorGuid, g, sizeof(EFI_GUID)) == 0) {
            ct[i].VendorTable = table;
            return EFI_SUCCESS;
        }
    }
    if (g_systab->NumberOfTableEntries >= 16) return EFI_OUT_OF_RESOURCES;
    ct[g_systab->NumberOfTableEntries].VendorGuid  = *g;
    ct[g_systab->NumberOfTableEntries].VendorTable = table;
    g_systab->NumberOfTableEntries++;
    return EFI_SUCCESS;
}

/* -- controllers (no UEFI driver model: we bind devices eagerly) ----- */

EFI_STATUS EFIAPI fw_bs_connect_controller(EFI_HANDLE c, EFI_HANDLE *d,
                                           EFI_DEVICE_PATH_PROTOCOL *r, BOOLEAN rec) {
    (void)c; (void)d; (void)r; (void)rec;
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI fw_bs_disconnect_controller(EFI_HANDLE c, EFI_HANDLE d, EFI_HANDLE ch) {
    (void)c; (void)d; (void)ch;
    return EFI_SUCCESS;
}

/* -- image services --------------------------------------------------- */

#define MAX_IMAGES 8

typedef struct {
    int used;
    EFI_HANDLE handle;
    fw_pe_image pe;
    EFI_LOADED_IMAGE_PROTOCOL li;
    EFI_STATUS exit_status;
    int exited;
} fw_image;

static fw_image g_images[MAX_IMAGES];

static fw_image *img_of(EFI_HANDLE h) {
    for (int i = 0; i < MAX_IMAGES; i++)
        if (g_images[i].used && g_images[i].handle == h) return &g_images[i];
    return NULL;
}

EFI_STATUS fw_image_create(void *buf, uint64_t size, EFI_HANDLE device, EFI_HANDLE *out) {
    fw_image *im = NULL;
    for (int i = 0; i < MAX_IMAGES; i++) if (!g_images[i].used) { im = &g_images[i]; break; }
    if (!im) return EFI_OUT_OF_RESOURCES;
    fw_memset(im, 0, sizeof(*im));

    if (fw_pe_load(buf, size, &im->pe) != 0) return EFI_LOAD_ERROR;

    im->handle = fw_efi_new_handle();
    if (!im->handle) return EFI_OUT_OF_RESOURCES;

    im->li.Revision      = 0x1000;
    im->li.ParentHandle  = NULL;
    im->li.SystemTable   = g_systab;
    im->li.DeviceHandle  = device;
    im->li.FilePath      = NULL;
    im->li.ImageBase     = im->pe.base;
    im->li.ImageSize     = im->pe.size;
    im->li.ImageCodeType = EfiLoaderCode;
    im->li.ImageDataType = EfiLoaderData;
    im->used = 1;

    fw_efi_install(im->handle, &gEfiLoadedImageProtocolGuid, &im->li);
    if (out) *out = im->handle;
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI fw_bs_load_image(BOOLEAN policy, EFI_HANDLE parent,
                                   EFI_DEVICE_PATH_PROTOCOL *dp, VOID *src, UINTN srcsz,
                                   EFI_HANDLE *out) {
    (void)policy; (void)parent; (void)dp;
    if (!src || !srcsz || !out) return EFI_INVALID_PARAMETER;   /* no DP-based load yet */
    return fw_image_create(src, srcsz, NULL, out);
}

EFI_STATUS EFIAPI fw_bs_start_image(EFI_HANDLE h, UINTN *exitsz, CHAR16 **exitdata) {
    fw_image *im = img_of(h);
    if (!im) return EFI_INVALID_PARAMETER;
    if (exitsz) *exitsz = 0;
    if (exitdata) *exitdata = NULL;
    EFI_IMAGE_ENTRY_POINT entry = (EFI_IMAGE_ENTRY_POINT)(uintptr_t)im->pe.entry;
    fw_printf("[fw] starting payload image (entry=0x%lx, st=%p)\n",
              (unsigned long)im->pe.entry, (void *)g_systab);
    return entry(h, g_systab);
}

EFI_STATUS EFIAPI fw_bs_exit(EFI_HANDLE h, EFI_STATUS status, UINTN dsz, CHAR16 *data) {
    (void)dsz; (void)data;
    fw_image *im = img_of(h);
    if (im) { im->exited = 1; im->exit_status = status; }
    fw_printf("\n[fw] image exited: status=0x%lx\n", (uint64_t)status);
    for (;;) __asm__ volatile("cli; hlt");
}

EFI_STATUS EFIAPI fw_bs_unload_image(EFI_HANDLE h) {
    fw_image *im = img_of(h);
    if (!im) return EFI_INVALID_PARAMETER;
    im->used = 0;
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI fw_bs_exit_boot_services(EFI_HANDLE h, UINTN key) {
    (void)h;
    if ((uint64_t)key != fw_mem_map_key()) return EFI_INVALID_PARAMETER;
    fw_events_signal_exit();
    g_bs_active = 0;
    /* Silence the PIT and mask legacy interrupts before handing over. */
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
    fw_puts("[fw] ExitBootServices: control handed to OS loader\n");
    return EFI_SUCCESS;
}
