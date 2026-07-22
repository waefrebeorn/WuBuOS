/*
 * vsl_macho.h  --  VSL Mach-O Binary Loader
 *
 * Loads and validates macOS Mach-O binaries for execution
 * within the VSL guest environment. Supports both 32-bit and
 * 64-bit Mach-O, universal (FAT) binaries, and the main
 * Mach-O object file format variants (MH_OBJECT, MH_EXECUTE,
 * MH_DYLIB, MH_BUNDLE, MH_DYLINKER, MH_KEXT_BUNDLE).
 */
#ifndef WUBUOS_VSL_MACHO_H
#define WUBUOS_VSL_MACHO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* -- Mach-O Magic Numbers ----------------------------------------- */
#define MH_MAGIC             0xFEEDFACEu  /* 32-bit Mach-O */
#define MH_CIGAM             0xCEFAEDFEu  /* 32-bit byte-swapped */
#define MH_MAGIC_64          0xFEEDFACFu  /* 64-bit Mach-O */
#define MH_CIGAM_64          0xCFFAEDFEu  /* 64-bit byte-swapped */
#define FAT_MAGIC            0xCAFEBABEu  /* Universal binary */
#define FAT_CIGAM            0xBEBAFECAu  /* Universal byte-swapped */
#define FAT_MAGIC_64         0xCAFEBABFu  /* 64-bit universal */
#define FAT_CIGAM_64         0xBFBAFECAu  /* 64-bit universal byte-swapped */

/* -- CPU Types ---------------------------------------------------- */
#define CPU_ARCH_MASK         0xFF000000
#define CPU_ARCH_ABI64        0x01000000
#define CPU_ARCH_ABI64_32     0x02000000

#define CPU_TYPE_X86          7
#define CPU_TYPE_I386         CPU_TYPE_X86
#define CPU_TYPE_X86_64       (CPU_TYPE_X86 | CPU_ARCH_ABI64)
#define CPU_TYPE_ARM          12
#define CPU_TYPE_ARM64        (CPU_TYPE_ARM | CPU_ARCH_ABI64)
#define CPU_TYPE_POWERPC      18
#define CPU_TYPE_POWERPC64    (CPU_TYPE_POWERPC | CPU_ARCH_ABI64)

#define CPU_SUBTYPE_X86_ALL    3
#define CPU_SUBTYPE_X86_64_ALL 3
#define CPU_SUBTYPE_ARM64_ALL  0

/* -- Mach-O File Types -------------------------------------------- */
#define MH_OBJECT             0x1   /* Relocatable object file */
#define MH_EXECUTE            0x2   /* Executable binary */
#define MH_FVMLIB             0x3   /* Fixed VM library */
#define MH_CORE               0x4   /* Core dump */
#define MH_PRELOAD            0x5   /* Preloaded executable */
#define MH_DYLIB              0x6   /* Dynamic library */
#define MH_DYLINKER           0x7   /* Dynamic linker */
#define MH_BUNDLE             0x8   /* Bundle (.bundle) */
#define MH_DYLIB_STUB         0x9   /* Dynamic library stub */
#define MH_DSYM               0xA   /* Debug symbols companion */
#define MH_KEXT_BUNDLE        0xB   /* Kext bundle */

/* -- Load Commands ------------------------------------------------- */
#define LC_SEGMENT            0x1
#define LC_SYMTAB             0x2
#define LC_DYSYMTAB           0xB
#define LC_LOAD_DYLINKER      0xE
#define LC_UUID               0x1B
#define LC_SEGMENT_64         0x19
#define LC_LOAD_DYLIB         0xC
#define LC_LOAD_WEAK_DYLIB    0x18
#define LC_REEXPORT_DYLIB     0x1F | 0x80000000
#define LC_MAIN               0x28  /* LC_UNIXTHREAD replacement */
#define LC_CODE_SIGNATURE     0x1D
#define LC_ENCRYPTION_INFO    0x21
#define LC_ENCRYPTION_INFO_64 0x2C
#define LC_DYLD_INFO          0x22
#define LC_DYLD_INFO_ONLY     0x80000022
#define LC_VERSION_MIN_MACOSX 0x24
#define LC_VERSION_MIN_IPHONEOS 0x25
#define LC_SOURCE_VERSION     0x2A
#define LC_DATA_IN_CODE       0x29
#define LC_FUNCTION_STARTS    0x26
#define LC_DYLIB_CODE_SIGN_DRS 0x2B
#define LC_LINKER_OPTIMIZATION_HINT 0x2E

/* -- Header Structures (packed for direct mapping) ----------------- */

/* 32-bit Mach-O header */
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t cputype;
    uint32_t cpusubtype;
    uint32_t filetype;
    uint32_t ncmds;
    uint32_t sizeofcmds;
    uint32_t flags;
    uint32_t reserved;  /* only in 64-bit, but we pad for simplicity */
} macho_header_32_t;

/* 64-bit Mach-O header */
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t cputype;
    uint32_t cpusubtype;
    uint32_t filetype;
    uint32_t ncmds;
    uint32_t sizeofcmds;
    uint32_t flags;
    uint32_t reserved;
} macho_header_64_t;

/* Load command header (generic prefix) */
typedef struct __attribute__((packed)) {
    uint32_t cmd;       /* LC_* constant */
    uint32_t cmdsize;   /* total size of this command */
} macho_load_command_t;

/* 32-bit segment command */
typedef struct __attribute__((packed)) {
    uint32_t cmd;       /* LC_SEGMENT */
    uint32_t cmdsize;
    char     segname[16];
    uint32_t vmaddr;
    uint32_t vmsize;
    uint32_t fileoff;
    uint32_t filesize;
    uint32_t maxprot;
    uint32_t initprot;
    uint32_t nsects;
    uint32_t flags;
} macho_segment_command_32_t;

/* 64-bit segment command */
typedef struct __attribute__((packed)) {
    uint32_t cmd;       /* LC_SEGMENT_64 */
    uint32_t cmdsize;
    char     segname[16];
    uint64_t vmaddr;
    uint64_t vmsize;
    uint64_t fileoff;
    uint64_t filesize;
    uint32_t maxprot;
    uint32_t initprot;
    uint32_t nsects;
    uint32_t flags;
} macho_segment_command_64_t;

/* Section (32-bit) */
typedef struct __attribute__((packed)) {
    char     sectname[16];
    char     segname[16];
    uint32_t addr;
    uint32_t size;
    uint32_t offset;
    uint32_t align;
    uint32_t reloff;
    uint32_t nreloc;
    uint32_t flags;
    uint32_t reserved1;
    uint32_t reserved2;
} macho_section_32_t;

/* Section (64-bit) */
typedef struct __attribute__((packed)) {
    char     sectname[16];
    char     segname[16];
    uint64_t addr;
    uint64_t size;
    uint32_t offset;
    uint32_t align;
    uint32_t reloff;
    uint32_t nreloc;
    uint32_t flags;
    uint32_t reserved1;
    uint32_t reserved2;
    uint32_t reserved3;
} macho_section_64_t;

/* LC_MAIN (entry point for modern Mach-O) */
typedef struct __attribute__((packed)) {
    uint32_t cmd;       /* LC_MAIN */
    uint32_t cmdsize;
    uint64_t entry_offset;  /* offset of entry from start of __TEXT segment */
    uint64_t stack_size;    /* initial stack size (0 = default) */
} macho_entry_point_command_t;

/* LC_UUID */
typedef struct __attribute__((packed)) {
    uint32_t cmd;
    uint32_t cmdsize;
    uint8_t  uuid[16];
} macho_uuid_command_t;

/* LC_DYLD_INFO / LC_DYLD_INFO_ONLY */
typedef struct __attribute__((packed)) {
    uint32_t cmd;
    uint32_t cmdsize;
    uint32_t rebase_off;
    uint32_t rebase_size;
    uint32_t bind_off;
    uint32_t bind_size;
    uint32_t weak_bind_off;
    uint32_t weak_bind_size;
    uint32_t lazy_bind_off;
    uint32_t lazy_bind_size;
    uint32_t export_off;
    uint32_t export_size;
} macho_dyld_info_command_t;

/* -- FAT Binary Structures ---------------------------------------- */
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t nfat_arch;
} fat_header_t;

typedef struct __attribute__((packed)) {
    uint32_t cputype;
    uint32_t cpusubtype;
    uint32_t offset;    /* offset to this architecture's Mach-O */
    uint32_t size;
    uint32_t align;
} fat_arch_t;

/* ==================================================================
 *  Public API
 * ================================================================== */

/* Validate Mach-O binary and extract entry point + segment info */
int vsl_macho_validate(const void *macho_data, size_t macho_size,
                       uint64_t *out_entry, uint32_t *out_filetype);

/* Load a Mach-O binary into VSL address space (mmap segments) */
int vsl_macho_load(const void *macho_data, size_t macho_size,
                   uint64_t load_base, uint64_t *out_entry,
                   uint64_t *out_stack_size);

/* Find a specific load command in a Mach-O */
const void *vsl_macho_find_command(const void *macho_data, size_t macho_size,
                                   uint32_t cmd_type);

/* Check if data is a valid Mach-O */
bool vsl_macho_is_macho(const void *data, size_t size);

/* Resolve FAT binary to specific architecture */
const void *vsl_macho_fat_extract(const void *data, size_t size,
                                  uint32_t cputype, uint32_t cpusubtype,
                                  size_t *out_macho_size);

#endif /* WUBUOS_VSL_MACHO_H */
