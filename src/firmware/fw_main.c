/*
 * fw_main.c  --  WuBuFW entry point and boot manager.
 *
 * Called from reset.S once the CPU is in long mode with a flat identity map.
 * Brings up console, memory, timing, storage, EFI tables, then loads and
 * starts \EFI\BOOT\BOOTX64.EFI from the first volume that has one.
 */

#include "fw.h"
#include "fw_pci.h"
#include "fw_acpi.h"
#include "fw_tpm.h"
#include "fw_fwcfg.h"
#include "fw_block.h"
#include "fw_agi.h"
#include "fw_secureboot.h"

extern EFI_SYSTEM_TABLE *g_systab;
EFI_STATUS fw_image_create(void *buf, uint64_t size, EFI_HANDLE device, EFI_HANDLE *out);
EFI_STATUS EFIAPI fw_bs_start_image(EFI_HANDLE, UINTN *, CHAR16 **);

/* Per-volume BlockIO backing (installed alongside the FS protocol). */
typedef struct {
    EFI_BLOCK_IO_PROTOCOL proto;
    EFI_BLOCK_IO_MEDIA    media;
    int                   blk;
} fw_blkio;

static fw_blkio g_blk[4];
static int g_nblk;

static EFI_STATUS EFIAPI blk_reset(EFI_BLOCK_IO_PROTOCOL *This, BOOLEAN ext) {
    (void)This; (void)ext; return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI blk_read(EFI_BLOCK_IO_PROTOCOL *This, UINT32 mid, EFI_LBA lba,
                                  UINTN size, VOID *buf) {
    fw_blkio *b = (fw_blkio *)This;
    if (!b || !buf) return EFI_INVALID_PARAMETER;
    if (mid != b->media.MediaId) return EFI_INVALID_PARAMETER;
    if (size % 512) return EFI_BAD_BUFFER_SIZE;
    if (fw_block_read(b->blk, lba, (uint32_t)(size / 512), buf) != 0) return EFI_DEVICE_ERROR;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI blk_write(EFI_BLOCK_IO_PROTOCOL *This, UINT32 mid, EFI_LBA lba,
                                   UINTN size, VOID *buf) {
    fw_blkio *b = (fw_blkio *)This;
    if (!b || !buf) return EFI_INVALID_PARAMETER;
    if (mid != b->media.MediaId) return EFI_INVALID_PARAMETER;
    if (size % 512) return EFI_BAD_BUFFER_SIZE;
    if (fw_block_write(b->blk, lba, (uint32_t)(size / 512), buf) != 0) return EFI_DEVICE_ERROR;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI blk_flush(EFI_BLOCK_IO_PROTOCOL *This) { (void)This; return EFI_SUCCESS; }

static void install_blockio(EFI_HANDLE h, int blk) {
    if (g_nblk >= 4) return;
    fw_blkio *b = &g_blk[g_nblk++];
    b->blk = blk;
    b->media.MediaId          = 1;
    b->media.RemovableMedia   = FALSE;
    b->media.MediaPresent     = TRUE;
    b->media.LogicalPartition = FALSE;
    b->media.ReadOnly         = FALSE;
    b->media.WriteCaching     = FALSE;
    b->media.BlockSize        = 512;
    b->media.IoAlign          = 1;
    b->media.LastBlock        = fw_block_get(blk)->sectors - 1;
    b->proto.Revision    = 0x10000;
    b->proto.Media       = &b->media;
    b->proto.Reset       = blk_reset;
    b->proto.ReadBlocks  = blk_read;
    b->proto.WriteBlocks = blk_write;
    b->proto.FlushBlocks = blk_flush;
    fw_efi_install(h, &gEfiBlockIoProtocolGuid, &b->proto);
}

EFI_HANDLE g_vol_handle[8];

void fw_efi_register_media(void) {
    int n = fw_media_count();
    for (int i = 0; i < n; i++) {
        fw_volume *v = fw_media_get(i);
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = fw_fsproto_create(v);
        if (!fs) continue;
        EFI_HANDLE h = fw_efi_new_handle();
        if (!h) continue;
        fw_efi_install(h, &gEfiSimpleFileSystemProtocolGuid, fs);
        g_vol_handle[i] = h;
    }
    for (int i = 0; i < fw_block_count() && i < 4; i++) {
        EFI_HANDLE h = fw_efi_new_handle();
        if (h) install_blockio(h, i);
    }
}

/* -- boot manager ----------------------------------------------------- */

static const char *g_boot_paths[] = {
    "\\EFI\\BOOT\\BOOTX64.EFI",
    "\\EFI\\boot\\bootx64.efi",
    "\\EFI\\WUBUOS\\WUBUOS.EFI",
    "\\BOOTX64.EFI",
};


/* Load a PE image from an on-volume path into an image handle. Used by the
 * shell boot command. Reads the file through the block+volume layer and
 * reuses the same PE loader as the normal boot path. */
EFI_STATUS fw_image_create_from_path(const char *path, EFI_HANDLE *out) {
    for (int i = 0; i < fw_media_count(); i++) {
        fw_volume *vol = fw_media_get(i);
        fw_openfile *f = fw_volume_open(vol, path);
        if (!f) continue;
        uint64_t sz = fw_openfile_size(f);
        uint8_t *buf = fw_pool_alloc((size_t)sz, EfiBootServicesData);
        if (!buf) return EFI_OUT_OF_RESOURCES;
        if (fw_openfile_pread(f, 0, (uint32_t)sz, buf) != sz) {
            fw_pool_free(buf);
            return EFI_DEVICE_ERROR;
        }
        EFI_STATUS s = fw_image_create(buf, sz, g_vol_handle[i], out);
        fw_pool_free(buf);
        return s;
    }
    return EFI_NOT_FOUND;
}

EFI_STATUS fw_boot_image(EFI_HANDLE handle) { return fw_bs_start_image(handle, NULL, NULL); }

/* Verified image load: measure + (if enforcement on) authenticate before
 * handing control to the image. This is the Secure Boot gate. */

/* Attested boot manager: verify+measure (PCR4) + load + start, routed through
 * the firmware attestation channel (fw_agi.c). The boot manager probes each
 * candidate path on each volume; the first image that clears the Secure Boot
 * gate + TCG measurement wins. */
int fw_boot_run(void) {
    int nvol = fw_media_count();
    for (int i = 0; i < nvol; i++) {
        fw_volume *v = fw_media_get(i);
        for (size_t p = 0; p < sizeof(g_boot_paths) / sizeof(g_boot_paths[0]); p++) {
            void *data = NULL; uint64_t size = 0;
            const char *path = g_boot_paths[p];
            if (fw_vol_read_file(v, path, &data, &size) != 0) continue;
            fw_printf("[boot] vol%d %s (%lu bytes)\n", i, path, (unsigned long)size);
            if (data) fw_free_pages(data, (uint32_t)((size + 4095) / 4096));
            /* fw_agi_attest_and_boot re-reads, verifies (Authenticode),
             * extends PCR4, publishes a fresh attestation table, loads, boots. */
            if (fw_agi_attest_and_boot(path) == 0) return 0;
            fw_printf("[boot] attested boot of %s refused\n", path);
        }
    }
    return -1;
}

/* -- entry ------------------------------------------------------------ */

void fw_main(void) {
    fw_con_init();
    fw_puts("\n");
    fw_puts("=========================================\n");
    fw_puts(" WuBuFW  --  WuBuOS UEFI firmware (C11)\n");
    fw_puts(" UEFI 2.10 services, no EDK2, no OVMF\n");
    fw_puts("=========================================\n");

    fw_mem_init();
    fw_time_init();
    fw_pci_init();                 /* enumerate PCI devices */
    fw_pci_assign_resources();     /* program BARs (QEMU leaves them at zero) */
    fw_acpi_init();                /* discover tables via fw_cfg + publish RSDP */
    fw_ata_init();                 /* legacy IDE boot disk (if any) */
    fw_drivers_init();             /* bind AHCI/NVMe/XHCI/e1000/GOP to PCI device */
    fw_media_init();               /* scan block → FAT volumes */
    fw_efi_build_tables();         /* assemble EFI system table (sets g_systab) */
    fw_efi_register_media();       /* install FS + BlockIO protocol handles */
    fw_measure_gpt();              /* PCR5 = partition table / boot config */
    fw_measure_secureboot(0, 1);   /* PCR7 = Secure Boot policy state */
    fw_agi_publish_attest();       /* publish PCR0-7 + SB snapshot */

    fw_tpm_init();                 /* TPM startup + self-test (software fallback) */
    fw_tpm_selftest_self();
    fw_sb_selftest();
    fw_sb_selftest_pe();
    fw_tpm_log_dump();

    /* Always-on interactive shell; `boot` launches the OS, `exit` auto-boots. */
    fw_shell_run();
    if (fw_boot_run() != 0) {
        fw_puts("\n[fw] no bootable EFI application found; starting shell.\n");
        fw_shell_run();
    }

    fw_puts("[fw] halted.\n");
    for (;;) __asm__ volatile("cli; hlt");
}
