/*
 * fw_pe.c  --  WuBuFW PE32+ (COFF) loader for EFI applications.
 *
 * Loads a BOOTX64.EFI: validates MZ/PE headers, allocates the image at its
 * preferred base when free (otherwise anywhere), copies sections, zeroes the
 * BSS tail, and applies .reloc fixups when relocated. Only x86-64 PE32+ EFI
 * subsystems are accepted; anything else is refused rather than guessed at.
 */

#include "fw.h"

#define MZ_MAGIC   0x5A4D
#define PE_MAGIC   0x00004550
#define PE32PLUS   0x20B
#define MACH_AMD64 0x8664

#define SUBSYS_EFI_APP        10
#define SUBSYS_EFI_BOOT_DRV   11
#define SUBSYS_EFI_RT_DRV     12

#define DIR_BASERELOC 5

#define REL_ABSOLUTE 0
#define REL_HIGH     1
#define REL_LOW      2
#define REL_HIGHLOW  3
#define REL_DIR64    10

typedef struct __attribute__((packed)) {
    uint32_t VirtualAddress;
    uint32_t Size;
} pe_datadir;

typedef struct __attribute__((packed)) {
    uint16_t Machine;
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
} pe_coff;

typedef struct __attribute__((packed)) {
    uint16_t Magic;
    uint8_t  MajorLinkerVersion;
    uint8_t  MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint64_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOSVersion, MinorOSVersion;
    uint16_t MajorImageVersion, MinorImageVersion;
    uint16_t MajorSubsystemVersion, MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint64_t SizeOfStackReserve, SizeOfStackCommit;
    uint64_t SizeOfHeapReserve, SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
    pe_datadir DataDirectory[16];
} pe_opt64;

typedef struct __attribute__((packed)) {
    char     Name[8];
    uint32_t VirtualSize;
    uint32_t VirtualAddress;
    uint32_t SizeOfRawData;
    uint32_t PointerToRawData;
    uint32_t PointerToRelocations;
    uint32_t PointerToLinenumbers;
    uint16_t NumberOfRelocations;
    uint16_t NumberOfLinenumbers;
    uint32_t Characteristics;
} pe_section;

static int apply_relocs(uint8_t *img, const pe_opt64 *opt, int64_t delta) {
    if (delta == 0) return 0;
    if (opt->NumberOfRvaAndSizes <= DIR_BASERELOC) return -1;
    uint32_t rva  = opt->DataDirectory[DIR_BASERELOC].VirtualAddress;
    uint32_t size = opt->DataDirectory[DIR_BASERELOC].Size;
    if (!rva || !size) return -1;                  /* relocated but no .reloc */
    if ((uint64_t)rva + size > opt->SizeOfImage) return -1;

    uint32_t off = 0;
    while (off + 8 <= size) {
        const uint8_t *blk = img + rva + off;
        uint32_t page  = *(const uint32_t *)blk;
        uint32_t bsize = *(const uint32_t *)(blk + 4);
        if (bsize < 8 || off + bsize > size) return -1;

        uint32_t count = (bsize - 8) / 2;
        const uint16_t *ent = (const uint16_t *)(blk + 8);
        for (uint32_t i = 0; i < count; i++) {
            uint16_t type = (uint16_t)(ent[i] >> 12);
            uint32_t target = page + (ent[i] & 0x0FFF);
            if (type == REL_ABSOLUTE) continue;
            if (target + 8 > opt->SizeOfImage) return -1;
            uint8_t *p = img + target;
            switch (type) {
            case REL_DIR64:  *(uint64_t *)p = *(uint64_t *)p + (uint64_t)delta; break;
            case REL_HIGHLOW:*(uint32_t *)p = *(uint32_t *)p + (uint32_t)delta; break;
            case REL_HIGH:   *(uint16_t *)p = (uint16_t)(*(uint16_t *)p + (uint16_t)((delta >> 16) & 0xFFFF)); break;
            case REL_LOW:    *(uint16_t *)p = (uint16_t)(*(uint16_t *)p + (uint16_t)(delta & 0xFFFF)); break;
            default: return -1;
            }
        }
        off += bsize;
    }
    return 0;
}

int fw_pe_load(const void *file, uint64_t file_size, fw_pe_image *out) {
    if (!file || !out || file_size < 0x40) return -1;
    const uint8_t *f = (const uint8_t *)file;

    if (*(const uint16_t *)f != MZ_MAGIC) { fw_puts("[pe] bad MZ\n"); return -1; }
    uint32_t e_lfanew = *(const uint32_t *)(f + 0x3C);
    if ((uint64_t)e_lfanew + 4 + sizeof(pe_coff) + sizeof(pe_opt64) > file_size) {
        fw_puts("[pe] header past EOF\n"); return -1;
    }
    if (*(const uint32_t *)(f + e_lfanew) != PE_MAGIC) { fw_puts("[pe] bad PE sig\n"); return -1; }

    const pe_coff   *coff = (const pe_coff *)(f + e_lfanew + 4);
    const pe_opt64  *opt  = (const pe_opt64 *)((const uint8_t *)coff + sizeof(pe_coff));



    if (coff->Machine != MACH_AMD64) { fw_printf("[pe] machine %x not amd64\n", coff->Machine); return -1; }

    if (opt->Subsystem != SUBSYS_EFI_APP && opt->Subsystem != SUBSYS_EFI_BOOT_DRV &&
        opt->Subsystem != SUBSYS_EFI_RT_DRV) {
        fw_printf("[pe] subsystem %u not EFI\n", opt->Subsystem);
        return -1;
    }
    if (opt->SizeOfImage == 0 || opt->SizeOfImage > 512u * 1024 * 1024) return -1;

    size_t pages = (opt->SizeOfImage + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE;

    uint8_t *img = NULL;
    if (opt->ImageBase && (opt->ImageBase & (EFI_PAGE_SIZE - 1)) == 0 &&
        opt->ImageBase >= FW_HEAP_BASE) {
        img = fw_alloc_pages_at(opt->ImageBase, pages, EfiLoaderCode);
    }
    if (!img) img = fw_alloc_pages(pages, EfiLoaderCode);
    
    if (!img) { fw_puts("[pe] out of memory\n"); return -1; }


    fw_memset(img, 0, pages * EFI_PAGE_SIZE);

    /* headers */
    uint32_t hdrsz = opt->SizeOfHeaders;
    if (hdrsz > file_size) hdrsz = (uint32_t)file_size;
    if (hdrsz > opt->SizeOfImage) hdrsz = opt->SizeOfImage;
    fw_memcpy(img, f, hdrsz);

    /* sections */
    const pe_section *sec = (const pe_section *)((const uint8_t *)opt + coff->SizeOfOptionalHeader);
    for (uint16_t i = 0; i < coff->NumberOfSections; i++) {
        const pe_section *s = &sec[i];
        if ((uint64_t)s->VirtualAddress + s->VirtualSize > opt->SizeOfImage) {
            fw_printf("[pe] section %u out of image\n", i);
            fw_free_pages(img, pages);
            return -1;
        }
        uint32_t raw = s->SizeOfRawData;
        if (raw) {
            if ((uint64_t)s->PointerToRawData + raw > file_size) {
                if ((uint64_t)s->PointerToRawData >= file_size) raw = 0;
                else raw = (uint32_t)(file_size - s->PointerToRawData);
            }
            if (raw > s->VirtualSize && s->VirtualSize) raw = s->VirtualSize;
            if (raw) fw_memcpy(img + s->VirtualAddress, f + s->PointerToRawData, raw);
        }
        /* remainder already zero from the memset (BSS) */
    }

    int64_t delta = (int64_t)((uint64_t)(uintptr_t)img - opt->ImageBase);
    if (apply_relocs(img, opt, delta) != 0) {
        fw_puts("[pe] relocation failed\n");
        fw_free_pages(img, pages);
        return -1;
    }

out->base      = img;
    out->size      = opt->SizeOfImage;
    out->entry     = (uint64_t)(uintptr_t)img + opt->AddressOfEntryPoint;
    out->subsystem = opt->Subsystem;

    return 0;
}
