/*
 * vsl_macho.c  --  VSL Mach-O Binary Loader Implementation
 *
 * Validates and loads macOS Mach-O binaries into the VSL guest
 * address space. Supports 64-bit Mach-O executables, FAT universal
 * binaries, and provides segment/section iteration.
 */
#include "vsl/vsl_internal.h"
#include "vsl/vsl_macho.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>

/* Global state */
extern VSL_STATE g_vsl;

bool vsl_macho_is_macho(const void *data, size_t size) {
    if (!data || size < 4) return false;
    uint32_t magic = *(const uint32_t*)data;
    return (magic == MH_MAGIC || magic == MH_CIGAM ||
            magic == MH_MAGIC_64 || magic == MH_CIGAM_64 ||
            magic == FAT_MAGIC || magic == FAT_CIGAM ||
            magic == FAT_MAGIC_64 || magic == FAT_CIGAM_64);
}

const void *vsl_macho_fat_extract(const void *data, size_t size,
                                   uint32_t cputype, uint32_t cpusubtype,
                                   size_t *out_macho_size) {
    if (!data || size < sizeof(fat_header_t)) return NULL;

    uint32_t magic = *(const uint32_t*)data;
    bool swap = (magic == FAT_CIGAM || magic == FAT_CIGAM_64);

    if (magic != FAT_MAGIC && magic != FAT_CIGAM &&
        magic != FAT_MAGIC_64 && magic != FAT_CIGAM_64) {
        /* Not a FAT binary; return data as-is if it's a Mach-O */
        if (vsl_macho_is_macho(data, size)) {
            if (out_macho_size) *out_macho_size = size;
            return data;
        }
        return NULL;
    }

    const fat_header_t *fh = (const fat_header_t*)data;
    uint32_t nfat = fh->nfat_arch;
    if (swap) {
        nfat = __builtin_bswap32(nfat);
    }

    if (size < sizeof(fat_header_t) + nfat * sizeof(fat_arch_t))
        return NULL;

    /* Find matching architecture */
    const fat_arch_t *archs = (const fat_arch_t*)(fh + 1);
    for (uint32_t i = 0; i < nfat; i++) {
        uint32_t fcputype = archs[i].cputype;
        uint32_t fcpusubtype = archs[i].cpusubtype;
        uint32_t foffset = archs[i].offset;
        uint32_t fsize = archs[i].size;

        if (swap) {
            fcputype = __builtin_bswap32(fcputype);
            fcpusubtype = __builtin_bswap32(fcpusubtype);
            foffset = __builtin_bswap32(foffset);
            fsize = __builtin_bswap32(fsize);
        }

        if (fcputype == cputype &&
            (fcpusubtype == cpusubtype || cpusubtype == ~0u)) {
            if (foffset + fsize > size) return NULL;
            if (out_macho_size) *out_macho_size = fsize;
            return (const uint8_t*)data + foffset;
        }
    }

    return NULL;  /* Architecture not found */
}

int vsl_macho_validate(const void *macho_data, size_t macho_size,
                       uint64_t *out_entry, uint32_t *out_filetype) {
    if (!macho_data || macho_size < sizeof(macho_header_32_t))
        return -1;

    uint32_t magic = *(const uint32_t*)macho_data;

    /* Handle FAT binaries by extracting x86-64 slice */
    size_t inner_size = 0;
    const void *inner = vsl_macho_fat_extract(macho_data, macho_size,
                                               CPU_TYPE_X86_64,
                                               CPU_SUBTYPE_X86_64_ALL,
                                               &inner_size);
    if (inner && inner != macho_data) {
        return vsl_macho_validate(inner, inner_size, out_entry, out_filetype);
    }

    /* Check for 64-bit Mach-O */
    if (magic != MH_MAGIC_64 && magic != MH_CIGAM_64) {
        if (magic == MH_MAGIC || magic == MH_CIGAM) {
            /* 32-bit Mach-O: not supported yet */
            if (out_filetype) {
                const macho_header_32_t *h32 = (const macho_header_32_t*)macho_data;
                *out_filetype = h32->filetype;
            }
            return -2;  /* 32-bit not supported */
        }
        return -1;  /* Not a valid Mach-O */
    }

    if (macho_size < sizeof(macho_header_64_t)) return -1;

    const macho_header_64_t *h = (const macho_header_64_t*)macho_data;

    if (h->filetype != MH_EXECUTE) {
        if (out_filetype) *out_filetype = h->filetype;
        return -3;  /* Not an executable */
    }

    if (out_filetype) *out_filetype = MH_EXECUTE;

    /* Find LC_MAIN (entry point) */
    const macho_entry_point_command_t *ep =
        (const macho_entry_point_command_t*)
            vsl_macho_find_command(macho_data, macho_size, LC_MAIN);

    if (ep) {
        /* Entry point is offset from __TEXT segment base.
         * We need to find the __TEXT segment vmaddr. */
        const macho_segment_command_64_t *text_seg =
            (const macho_segment_command_64_t*)
                vsl_macho_find_command(macho_data, macho_size, LC_SEGMENT_64);
        if (text_seg && strncmp(text_seg->segname, "__TEXT", 16) == 0) {
            if (out_entry) *out_entry = text_seg->vmaddr + ep->entry_offset;
            return 0;
        }
    }

    /* No LC_MAIN; return offset 0 (caller handles) */
    if (out_entry) *out_entry = 0;
    return 0;
}

const void *vsl_macho_find_command(const void *macho_data, size_t macho_size,
                                    uint32_t cmd_type) {
    if (!macho_data || macho_size < sizeof(macho_header_64_t))
        return NULL;

    uint32_t magic = *(const uint32_t*)macho_data;
    bool is_64 = (magic == MH_MAGIC_64 || magic == MH_CIGAM_64);

    uint32_t ncmds;
    uint32_t sizeofcmds;
    const uint8_t *cmds_start;
    bool swap = (magic == MH_CIGAM || magic == MH_CIGAM_64);

    if (is_64) {
        const macho_header_64_t *h = (const macho_header_64_t*)macho_data;
        ncmds = h->ncmds;
        sizeofcmds = h->sizeofcmds;
        cmds_start = (const uint8_t*)(h + 1);
    } else {
        const macho_header_32_t *h = (const macho_header_32_t*)macho_data;
        ncmds = h->ncmds;
        sizeofcmds = h->sizeofcmds;
        cmds_start = (const uint8_t*)(h + 1);
    }

    if (swap) {
        ncmds = __builtin_bswap32(ncmds);
        sizeofcmds = __builtin_bswap32(sizeofcmds);
    }

    if ((size_t)(cmds_start - (const uint8_t*)macho_data) + sizeofcmds > macho_size)
        return NULL;

    const uint8_t *p = cmds_start;
    const uint8_t *end = p + sizeofcmds;

    for (uint32_t i = 0; i < ncmds && p + sizeof(macho_load_command_t) <= end; i++) {
        const macho_load_command_t *lc = (const macho_load_command_t*)p;
        uint32_t cmd = lc->cmd;
        uint32_t cmdsize = lc->cmdsize;

        if (swap) {
            cmd = __builtin_bswap32(cmd);
            cmdsize = __builtin_bswap32(cmdsize);
        }

        if (cmdsize < sizeof(macho_load_command_t)) break;

        if (cmd == cmd_type) return p;

        p += cmdsize;
    }

    return NULL;
}

int vsl_macho_load(const void *macho_data, size_t macho_size,
                    uint64_t load_base, uint64_t *out_entry,
                    uint64_t *out_stack_size) {
    if (!macho_data || macho_size < sizeof(macho_header_64_t))
        return -1;

    uint32_t magic = *(const uint32_t*)macho_data;

    /* Handle FAT binaries */
    size_t inner_size = 0;
    const void *inner = vsl_macho_fat_extract(macho_data, macho_size,
                                               CPU_TYPE_X86_64,
                                               CPU_SUBTYPE_X86_64_ALL,
                                               &inner_size);
    if (inner && inner != macho_data) {
        return vsl_macho_load(inner, inner_size, load_base, out_entry, out_stack_size);
    }

    if (magic != MH_MAGIC_64 && magic != MH_CIGAM_64)
        return -1;

    const macho_header_64_t *h = (const macho_header_64_t*)macho_data;
    bool swap = (magic == MH_CIGAM_64);

    uint32_t ncmds = swap ? __builtin_bswap32(h->ncmds) : h->ncmds;
    uint32_t sizeofcmds = swap ? __builtin_bswap32(h->sizeofcmds) : h->sizeofcmds;

    const uint8_t *p = (const uint8_t*)(h + 1);
    const uint8_t *end = p + sizeofcmds;

    uint64_t entry = 0;
    uint64_t stack_size = 0;

    /* First pass: map all segments */
    for (uint32_t i = 0; i < ncmds && p + sizeof(macho_load_command_t) <= end; i++) {
        const macho_load_command_t *lc = (const macho_load_command_t*)p;
        uint32_t cmd = swap ? __builtin_bswap32(lc->cmd) : lc->cmd;
        uint32_t cmdsize = swap ? __builtin_bswap32(lc->cmdsize) : lc->cmdsize;

        switch (cmd) {
            case LC_SEGMENT_64: {
                const macho_segment_command_64_t *seg =
                    (const macho_segment_command_64_t*)p;
                uint64_t vmaddr = swap ? __builtin_bswap64(seg->vmaddr) : seg->vmaddr;
                uint64_t vmsize = swap ? __builtin_bswap64(seg->vmsize) : seg->vmsize;
                uint64_t fileoff = swap ? __builtin_bswap64(seg->fileoff) : seg->fileoff;
                uint64_t filesize = swap ? __builtin_bswap64(seg->filesize) : seg->filesize;
                uint32_t initprot = swap ? __builtin_bswap32(seg->initprot) : seg->initprot;
                uint32_t maxprot = swap ? __builtin_bswap32(seg->maxprot) : seg->maxprot;

                /* Map the segment into VSL address space */
                uint64_t map_addr = load_base + vmaddr;
                int prot = 0;
                if (initprot & 1) prot |= PROT_READ;
                if (initprot & 2) prot |= PROT_WRITE;
                if (initprot & 4) prot |= PROT_EXEC;

                if (fileoff + filesize > macho_size) {
                    /* Zero-fill instead */
                    if (vmsize > 0) {
                        void *r = mmap((void*)(uintptr_t)map_addr, vmsize,
                                        prot, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
                                        -1, 0);
                        if (r == MAP_FAILED) return -1;
                    }
                } else if (filesize > 0) {
                    /* Map from file data */
                    // We need to copy the data since Mach-O is in memory
                    // In a real VSL this would do proper mmap
                    void *r = mmap((void*)(uintptr_t)map_addr, vmsize,
                                    prot, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
                                    -1, 0);
                    if (r == MAP_FAILED) return -1;
                    if (filesize > 0) {
                        memcpy(r, (const uint8_t*)macho_data + fileoff,
                               filesize < vmsize ? filesize : vmsize);
                    }
                }

                /* Map __LINKEDIT specially for dyld */
                if (strncmp(seg->segname, "__LINKEDIT", 16) == 0) {
                    /* LINKEDIT is mapped but not directly used;
                     * dyld uses it for symbol tables. */
                }
                break;
            }

            case LC_MAIN: {
                const macho_entry_point_command_t *ep =
                    (const macho_entry_point_command_t*)p;
                uint64_t ep_off = swap ? __builtin_bswap64(ep->entry_offset) : ep->entry_offset;
                uint64_t ss = swap ? __builtin_bswap64(ep->stack_size) : ep->stack_size;

                /* Find __TEXT segment to compute absolute entry */
                const void *text_cmd = vsl_macho_find_command(macho_data, macho_size,
                                                              LC_SEGMENT_64);
                if (text_cmd) {
                    const macho_segment_command_64_t *text_seg =
                        (const macho_segment_command_64_t*)text_cmd;
                    if (strncmp(text_seg->segname, "__TEXT", 16) == 0) {
                        uint64_t text_vmaddr = swap
                            ? __builtin_bswap64(text_seg->vmaddr)
                            : text_seg->vmaddr;
                        entry = load_base + text_vmaddr + ep_off;
                    }
                }
                stack_size = ss;
                break;
            }
        }

        p += cmdsize;
    }

    if (out_entry) *out_entry = entry;
    if (out_stack_size) *out_stack_size = stack_size;
    return (entry != 0) ? 0 : -1;
}
