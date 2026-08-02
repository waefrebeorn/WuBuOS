/*
 * fw_table.c  --  Assemble the EFI system / boot / runtime service tables.
 */

#include "fw.h"

/* console protocol instances */
extern EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL g_conout;
extern EFI_SIMPLE_TEXT_INPUT_PROTOCOL  g_conin;
extern EFI_EVENT g_wait_for_key;

/* boot services (fw_bs_mem.c) */
EFI_TPL    EFIAPI fw_bs_raise_tpl(EFI_TPL);
VOID       EFIAPI fw_bs_restore_tpl(EFI_TPL);
EFI_STATUS EFIAPI fw_bs_alloc_pages(EFI_ALLOCATE_TYPE, EFI_MEMORY_TYPE, UINTN, EFI_PHYSICAL_ADDRESS *);
EFI_STATUS EFIAPI fw_bs_free_pages(EFI_PHYSICAL_ADDRESS, UINTN);
EFI_STATUS EFIAPI fw_bs_get_memory_map(UINTN *, EFI_MEMORY_DESCRIPTOR *, UINTN *, UINTN *, UINT32 *);
EFI_STATUS EFIAPI fw_bs_alloc_pool(EFI_MEMORY_TYPE, UINTN, VOID **);
EFI_STATUS EFIAPI fw_bs_free_pool(VOID *);
EFI_STATUS EFIAPI fw_bs_create_event(UINT32, EFI_TPL, EFI_EVENT_NOTIFY, VOID *, EFI_EVENT *);
EFI_STATUS EFIAPI fw_bs_create_event_ex(UINT32, EFI_TPL, EFI_EVENT_NOTIFY, const VOID *, const EFI_GUID *, EFI_EVENT *);
EFI_STATUS EFIAPI fw_bs_set_timer(EFI_EVENT, EFI_TIMER_DELAY, UINT64);
EFI_STATUS EFIAPI fw_bs_wait_for_event(UINTN, EFI_EVENT *, UINTN *);
EFI_STATUS EFIAPI fw_bs_signal_event(EFI_EVENT);
EFI_STATUS EFIAPI fw_bs_close_event(EFI_EVENT);
EFI_STATUS EFIAPI fw_bs_check_event(EFI_EVENT);
EFI_STATUS EFIAPI fw_bs_get_mono(UINT64 *);
EFI_STATUS EFIAPI fw_bs_stall(UINTN);
EFI_STATUS EFIAPI fw_bs_set_watchdog(UINTN, UINT64, UINTN, CHAR16 *);
VOID       EFIAPI fw_bs_copy_mem(VOID *, VOID *, UINTN);
VOID       EFIAPI fw_bs_set_mem(VOID *, UINTN, UINT8);
EFI_STATUS EFIAPI fw_bs_crc32(VOID *, UINTN, UINT32 *);

/* boot services (fw_bs_proto.c) */
EFI_STATUS EFIAPI fw_bs_install_protocol(EFI_HANDLE *, EFI_GUID *, EFI_INTERFACE_TYPE, VOID *);
EFI_STATUS EFIAPI fw_bs_reinstall_protocol(EFI_HANDLE, EFI_GUID *, VOID *, VOID *);
EFI_STATUS EFIAPI fw_bs_uninstall_protocol(EFI_HANDLE, EFI_GUID *, VOID *);
EFI_STATUS EFIAPI fw_bs_handle_protocol(EFI_HANDLE, EFI_GUID *, VOID **);
EFI_STATUS EFIAPI fw_bs_register_notify(EFI_GUID *, EFI_EVENT, VOID **);
EFI_STATUS EFIAPI fw_bs_locate_handle(EFI_LOCATE_SEARCH_TYPE, EFI_GUID *, VOID *, UINTN *, EFI_HANDLE *);
EFI_STATUS EFIAPI fw_bs_locate_device_path(EFI_GUID *, EFI_DEVICE_PATH_PROTOCOL **, EFI_HANDLE *);
EFI_STATUS EFIAPI fw_bs_install_config_table(EFI_GUID *, VOID *);
EFI_STATUS EFIAPI fw_bs_load_image(BOOLEAN, EFI_HANDLE, EFI_DEVICE_PATH_PROTOCOL *, VOID *, UINTN, EFI_HANDLE *);
EFI_STATUS EFIAPI fw_bs_start_image(EFI_HANDLE, UINTN *, CHAR16 **);
EFI_STATUS EFIAPI fw_bs_exit(EFI_HANDLE, EFI_STATUS, UINTN, CHAR16 *);
EFI_STATUS EFIAPI fw_bs_unload_image(EFI_HANDLE);
EFI_STATUS EFIAPI fw_bs_exit_boot_services(EFI_HANDLE, UINTN);
EFI_STATUS EFIAPI fw_bs_connect_controller(EFI_HANDLE, EFI_HANDLE *, EFI_DEVICE_PATH_PROTOCOL *, BOOLEAN);
EFI_STATUS EFIAPI fw_bs_disconnect_controller(EFI_HANDLE, EFI_HANDLE, EFI_HANDLE);
EFI_STATUS EFIAPI fw_bs_open_protocol(EFI_HANDLE, EFI_GUID *, VOID **, EFI_HANDLE, EFI_HANDLE, UINT32);
EFI_STATUS EFIAPI fw_bs_close_protocol(EFI_HANDLE, EFI_GUID *, EFI_HANDLE, EFI_HANDLE);
EFI_STATUS EFIAPI fw_bs_open_protocol_info(EFI_HANDLE, EFI_GUID *, VOID **, UINTN *);
EFI_STATUS EFIAPI fw_bs_protocols_per_handle(EFI_HANDLE, EFI_GUID ***, UINTN *);
EFI_STATUS EFIAPI fw_bs_locate_handle_buffer(EFI_LOCATE_SEARCH_TYPE, EFI_GUID *, VOID *, UINTN *, EFI_HANDLE **);
EFI_STATUS EFIAPI fw_bs_locate_protocol(EFI_GUID *, VOID *, VOID **);
EFI_STATUS EFIAPI fw_bs_install_multiple(EFI_HANDLE *, ...);
EFI_STATUS EFIAPI fw_bs_uninstall_multiple(EFI_HANDLE, ...);

/* runtime services */
EFI_STATUS EFIAPI fw_rt_get_time(EFI_TIME *, EFI_TIME_CAPABILITIES *);
EFI_STATUS EFIAPI fw_rt_set_time(EFI_TIME *);
EFI_STATUS EFIAPI fw_rt_get_wakeup(BOOLEAN *, BOOLEAN *, EFI_TIME *);
EFI_STATUS EFIAPI fw_rt_set_wakeup(BOOLEAN, EFI_TIME *);
EFI_STATUS EFIAPI fw_rt_set_virtual_map(UINTN, UINTN, UINT32, EFI_MEMORY_DESCRIPTOR *);
EFI_STATUS EFIAPI fw_rt_convert_pointer(UINTN, VOID **);
EFI_STATUS EFIAPI fw_rt_get_variable(CHAR16 *, EFI_GUID *, UINT32 *, UINTN *, VOID *);
EFI_STATUS EFIAPI fw_rt_get_next_variable(UINTN *, CHAR16 *, EFI_GUID *);
EFI_STATUS EFIAPI fw_rt_set_variable(CHAR16 *, EFI_GUID *, UINT32, UINTN, VOID *);
EFI_STATUS EFIAPI fw_rt_get_next_high_mono(UINT32 *);
VOID       EFIAPI fw_rt_reset(EFI_RESET_TYPE, EFI_STATUS, UINTN, VOID *);
EFI_STATUS EFIAPI fw_rt_update_capsule(VOID **, UINTN, EFI_PHYSICAL_ADDRESS);
EFI_STATUS EFIAPI fw_rt_query_capsule(VOID **, UINTN, UINT64 *, EFI_RESET_TYPE *);
EFI_STATUS EFIAPI fw_rt_query_variable_info(UINT32, UINT64 *, UINT64 *, UINT64 *);

static EFI_BOOT_SERVICES       g_bs;
static EFI_RUNTIME_SERVICES    g_rt;
static EFI_SYSTEM_TABLE        g_st;
static EFI_CONFIGURATION_TABLE g_cfg[16];
static CHAR16 g_vendor[] = { 'W','u','B','u','F','W',0 };

EFI_SYSTEM_TABLE *g_systab;

static void crc_table(EFI_TABLE_HEADER *h, UINTN size) {
    h->HeaderSize = (UINT32)size;
    h->CRC32 = 0;
    UINT32 crc = 0;
    fw_bs_crc32((VOID *)h, size, &crc);
    h->CRC32 = crc;
}

EFI_SYSTEM_TABLE *fw_efi_build_tables(void) {
    fw_memset(&g_bs, 0, sizeof(g_bs));
    fw_memset(&g_rt, 0, sizeof(g_rt));
    fw_memset(&g_st, 0, sizeof(g_st));

    g_bs.RaiseTPL   = fw_bs_raise_tpl;
    g_bs.RestoreTPL = fw_bs_restore_tpl;
    g_bs.AllocatePages = fw_bs_alloc_pages;
    g_bs.FreePages     = fw_bs_free_pages;
    g_bs.GetMemoryMap  = fw_bs_get_memory_map;
    g_bs.AllocatePool  = fw_bs_alloc_pool;
    g_bs.FreePool      = fw_bs_free_pool;
    g_bs.CreateEvent   = fw_bs_create_event;
    g_bs.SetTimer      = fw_bs_set_timer;
    g_bs.WaitForEvent  = fw_bs_wait_for_event;
    g_bs.SignalEvent   = fw_bs_signal_event;
    g_bs.CloseEvent    = fw_bs_close_event;
    g_bs.CheckEvent    = fw_bs_check_event;
    g_bs.InstallProtocolInterface   = fw_bs_install_protocol;
    g_bs.ReinstallProtocolInterface = fw_bs_reinstall_protocol;
    g_bs.UninstallProtocolInterface = fw_bs_uninstall_protocol;
    g_bs.HandleProtocol             = fw_bs_handle_protocol;
    g_bs.Reserved                   = NULL;
    g_bs.RegisterProtocolNotify     = fw_bs_register_notify;
    g_bs.LocateHandle               = fw_bs_locate_handle;
    g_bs.LocateDevicePath           = fw_bs_locate_device_path;
    g_bs.InstallConfigurationTable  = fw_bs_install_config_table;
    g_bs.LoadImage        = fw_bs_load_image;
    g_bs.StartImage       = fw_bs_start_image;
    g_bs.Exit             = fw_bs_exit;
    g_bs.UnloadImage      = fw_bs_unload_image;
    g_bs.ExitBootServices = fw_bs_exit_boot_services;
    g_bs.GetNextMonotonicCount = fw_bs_get_mono;
    g_bs.Stall                 = fw_bs_stall;
    g_bs.SetWatchdogTimer      = fw_bs_set_watchdog;
    g_bs.ConnectController     = fw_bs_connect_controller;
    g_bs.DisconnectController  = fw_bs_disconnect_controller;
    g_bs.OpenProtocol            = fw_bs_open_protocol;
    g_bs.CloseProtocol           = fw_bs_close_protocol;
    g_bs.OpenProtocolInformation = fw_bs_open_protocol_info;
    g_bs.ProtocolsPerHandle      = fw_bs_protocols_per_handle;
    g_bs.LocateHandleBuffer      = fw_bs_locate_handle_buffer;
    g_bs.LocateProtocol          = fw_bs_locate_protocol;
    g_bs.InstallMultipleProtocolInterfaces   = fw_bs_install_multiple;
    g_bs.UninstallMultipleProtocolInterfaces = fw_bs_uninstall_multiple;
    g_bs.CalculateCrc32 = fw_bs_crc32;
    g_bs.CopyMem        = fw_bs_copy_mem;
    g_bs.SetMem         = fw_bs_set_mem;
    g_bs.CreateEventEx  = fw_bs_create_event_ex;

    g_bs.Hdr.Signature = EFI_BOOT_SERVICES_SIGNATURE;
    g_bs.Hdr.Revision  = EFI_2_100_SYSTEM_TABLE_REVISION;
    crc_table(&g_bs.Hdr, sizeof(g_bs));

    g_rt.GetTime = fw_rt_get_time;
    g_rt.SetTime = fw_rt_set_time;
    g_rt.GetWakeupTime = fw_rt_get_wakeup;
    g_rt.SetWakeupTime = fw_rt_set_wakeup;
    g_rt.SetVirtualAddressMap = fw_rt_set_virtual_map;
    g_rt.ConvertPointer       = fw_rt_convert_pointer;
    g_rt.GetVariable          = fw_rt_get_variable;
    g_rt.GetNextVariableName  = fw_rt_get_next_variable;
    g_rt.SetVariable          = fw_rt_set_variable;
    g_rt.GetNextHighMonotonicCount = fw_rt_get_next_high_mono;
    g_rt.ResetSystem   = fw_rt_reset;
    g_rt.UpdateCapsule = fw_rt_update_capsule;
    g_rt.QueryCapsuleCapabilities = fw_rt_query_capsule;
    g_rt.QueryVariableInfo        = fw_rt_query_variable_info;

    g_rt.Hdr.Signature = EFI_RUNTIME_SERVICES_SIGNATURE;
    g_rt.Hdr.Revision  = EFI_2_100_SYSTEM_TABLE_REVISION;
    crc_table(&g_rt.Hdr, sizeof(g_rt));

    g_conin.WaitForKey = g_wait_for_key;

    EFI_HANDLE hout = fw_efi_new_handle();
    EFI_HANDLE hin  = fw_efi_new_handle();
    fw_efi_install(hout, &gEfiSimpleTextOutProtocolGuid, &g_conout);
    fw_efi_install(hin,  &gEfiSimpleTextInProtocolGuid,  &g_conin);

    g_st.Hdr.Signature = EFI_SYSTEM_TABLE_SIGNATURE;
    g_st.Hdr.Revision  = EFI_2_100_SYSTEM_TABLE_REVISION;
    g_st.FirmwareVendor   = g_vendor;
    g_st.FirmwareRevision = 0x00010000;
    g_st.ConsoleInHandle  = hin;
    g_st.ConIn            = &g_conin;
    g_st.ConsoleOutHandle = hout;
    g_st.ConOut           = &g_conout;
    g_st.StandardErrorHandle = hout;
    g_st.StdErr              = &g_conout;
    g_st.RuntimeServices     = &g_rt;
    g_st.BootServices        = &g_bs;
    g_st.NumberOfTableEntries = 0;
    g_st.ConfigurationTable   = g_cfg;
    crc_table(&g_st.Hdr, sizeof(g_st));

    g_systab = &g_st;
    return &g_st;
}
