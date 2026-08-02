/*
 * fw.h  --  WuBuFW internal firmware interfaces.
 *
 * Everything the firmware needs that is NOT part of the UEFI ABI lives here:
 * serial/VGA console, page allocator, GPT/FAT32 media stack, PE loader.
 * SysV ABI internally; only the protocol thunks are EFIAPI.
 */

#ifndef WUBUFW_FW_H
#define WUBUFW_FW_H

#include "efi.h"

/* -- Physical memory layout (firmware-owned) --------------------------- */
#define FW_LOAD_BASE     0x00200000ULL   /* firmware image copied here     */
#define FW_STACK_TOP     0x0009F000ULL   /* boot stack (below EBDA)        */
#define FW_PT_BASE       0x00070000ULL   /* PML4/PDPT/PDs                  */
#define FW_HEAP_BASE     0x00400000ULL   /* page allocator arena start     */
#define FW_HEAP_LIMIT    0x08000000ULL   /* 128MB ceiling for the arena    */
#define FW_IMAGE_BASE    0x01000000ULL   /* loaded EFI images live here    */

/* -- I/O --------------------------------------------------------------- */
static inline void outb(uint16_t p, uint8_t v)  { __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }
static inline void outw(uint16_t p, uint16_t v) { __asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p)); }
static inline void outl(uint16_t p, uint32_t v) { __asm__ volatile("outl %0,%1"::"a"(v),"Nd"(p)); }
static inline uint8_t  inb(uint16_t p) { uint8_t v;  __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p)); return v; }
static inline uint16_t inw(uint16_t p) { uint16_t v; __asm__ volatile("inw %1,%0":"=a"(v):"Nd"(p)); return v; }
static inline uint32_t inl(uint16_t p) { uint32_t v; __asm__ volatile("inl %1,%0":"=a"(v):"Nd"(p)); return v; }
static inline void io_wait(void) { outb(0x80, 0); }

/*
 * Read a fixed low physical address (BDA, EBDA, RSDP scan region). GCC's
 * -Warray-bounds treats a literal near-null address as a bogus object, so
 * the address is laundered through an asm barrier that is opaque to it.
 * This is a real read, not a workaround for a real bug.
 */
static inline uintptr_t fw_phys(uintptr_t a) {
    __asm__("" : "+r"(a));
    return a;
}
static inline uint16_t fw_read16_phys(uintptr_t a) {
    return *(volatile uint16_t *)fw_phys(a);
}
static inline uint8_t fw_read8_phys(uintptr_t a) {
    return *(volatile uint8_t *)fw_phys(a);
}
static inline uint64_t rdtsc(void) {
    uint32_t lo, hi; __asm__ volatile("rdtsc":"=a"(lo),"=d"(hi)); return ((uint64_t)hi << 32) | lo;
}

/* -- Console ----------------------------------------------------------- */
void fw_con_init(void);
void fw_putc(char c);
void fw_puts(const char *s);
void fw_puthex(uint64_t v);
void fw_putdec(uint64_t v);
void fw_printf(const char *fmt, ...);
int  fw_getc_nb(void);
int  fw_getc(void);            /* blocking */            /* -1 when no key pending */

/* -- Minimal freestanding string/mem ----------------------------------- */
void  *fw_memset(void *d, int c, size_t n);
void  *fw_memcpy(void *d, const void *s, size_t n);
int    fw_memcmp(const void *a, const void *b, size_t n);
size_t fw_strlen(const char *s);
int    fw_strcmp(const char *a, const char *b);
size_t fw_strlen16(const CHAR16 *s);
int    fw_stricmp16(const CHAR16 *a, const CHAR16 *b);

/* -- Page allocator ---------------------------------------------------- */
void   fw_mem_init(void);
void  *fw_alloc_pages(size_t pages, EFI_MEMORY_TYPE type);
void  *fw_alloc_pages_at(uint64_t addr, size_t pages, EFI_MEMORY_TYPE type);
/* Alignment stronger than a page: DMA rings (AHCI command lists, NVMe
 * queues, xHCI DCBAA) have hardware alignment requirements. */
void  *fw_alloc_pages_aligned(size_t pages, size_t align);

/* Driver manager + measured boot hooks (fw_drivers.c). */
int   fw_drivers_init(void);
void  fw_measure_gpt(void);
void  fw_measure_secureboot(int enabled, int setup_mode);
void   fw_free_pages(void *p, size_t pages);
void  *fw_pool_alloc(size_t bytes, EFI_MEMORY_TYPE type);
void   fw_pool_free(void *p);
size_t fw_mem_map_build(EFI_MEMORY_DESCRIPTOR *out, size_t max_entries);
uint64_t fw_mem_map_key(void);

/* -- Timing ------------------------------------------------------------ */
void fw_time_init(void);
void fw_stall_us(uint64_t us);
void fw_rtc_read(EFI_TIME *t);

/* -- ATA PIO block device (QEMU IDE, primary/secondary master/slave) ---- */
typedef struct fw_ata_dev fw_ata_dev;
int         fw_ata_init(void);
int         fw_ata_count(void);
fw_ata_dev *fw_ata_get(int index);
uint64_t    fw_ata_sectors(fw_ata_dev *d);
int         fw_ata_read(fw_ata_dev *d, uint64_t lba, uint32_t count, void *buf);
int         fw_ata_write(fw_ata_dev *d, uint64_t lba, uint32_t count, const void *buf);

/* -- Media stack: GPT partition scan + FAT (12/16/32) reader ----------- */
typedef struct fw_volume fw_volume;
typedef struct fw_openfile fw_openfile;
int         fw_media_init(void);          /* scan disks -> ESP volumes    */
int         fw_media_count(void);
fw_volume  *fw_media_get(int i);
/* Read whole file by 8.3/LFN path with '\' or '/' separators. */
int         fw_vol_read_file(fw_volume *v, const char *path, void **out, uint64_t *size);
int         fw_vol_stat(fw_volume *v, const char *path, uint64_t *size, uint32_t *attr);

/* -- PE32+ loader ------------------------------------------------------ */
typedef struct {
    void    *base;        /* loaded image base (page aligned)             */
    uint64_t size;        /* virtual size in bytes                        */
    uint64_t entry;       /* absolute entry point address                 */
    uint16_t subsystem;
} fw_pe_image;
int fw_pe_load(const void *file, uint64_t file_size, fw_pe_image *out);

/* -- EFI table construction ------------------------------------------- */
EFI_SYSTEM_TABLE *fw_efi_build_tables(void);
void              fw_efi_register_media(void);   /* install FS/BlockIO handles */
EFI_HANDLE        fw_efi_new_handle(void);
EFI_STATUS        fw_efi_install(EFI_HANDLE h, EFI_GUID *guid, void *iface);
int               fw_efi_boot_services_active(void);

/* Simple File System instance bound to a fw_volume (fw_fsproto.c). */
EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fw_fsproto_create(fw_volume *v);

/* Volume metadata + shell-facing directory iteration. */
typedef struct {
    char     name[64];
    uint32_t size;
    uint8_t  attr;
    int      is_dir;
    int      valid;
} fw_dirent;
void        fw_volume_reset(int vol);
fw_dirent  *fw_volume_next(int vol);
fw_openfile *fw_volume_open(fw_volume *v, const char *path);
uint32_t     fw_openfile_pread(fw_openfile *f, uint64_t off, uint32_t count, void *buf);
uint64_t    fw_openfile_size(fw_openfile *f);
EFI_STATUS  fw_image_start(EFI_HANDLE handle);
const char *fw_volume_label(int vol);

/* memory-map summary helpers (shell `mem`) */
int   fw_mem_count(void);
uint64_t fw_mem_free_mb(void);

/* image loading used by the boot manager and shell */
EFI_STATUS fw_image_create_from_path(const char *path, EFI_HANDLE *out);
EFI_STATUS fw_boot_image(EFI_HANDLE handle);

/* interactive firmware shell */
void fw_shell_run(void);

/* Boot manager: find and run \EFI\BOOT\BOOTX64.EFI */
int fw_boot_run(void);

#endif /* WUBUFW_FW_H */
