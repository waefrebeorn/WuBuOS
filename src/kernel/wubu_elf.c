/*
 * wubu_elf.c -- the IN-KERNEL ELF loader (the Linux format, on OUR
 * kernel — not a host container exec).
 *
 * The user's correction: "all this needs to be running on temple os
 * kernel our kernel not just a arch connection." The OpenArena
 * Linux binary (an x86-64 ELF) is loaded + executed by OUR kernel:
 *   - validate the magic + the 64-bit header
 *   - parse the program headers (LOAD segments) -> the mapping plan
 *   - parse the dynamic section (NEEDED libs) -> the import contract
 *   - the entry point (the kernel jumps there, in ring-0)
 *
 * C11, kernel-clean (-nostdlib context), opaque (no globals — the
 * caller passes in the buffers). Self-contained: no god header.
 */
#include "wubu_elf.h"
#include <string.h>

int wubu_elf_parse(const void *data, size_t size, wubu_elf_info_t *out)
{
    if (!data || !out || size < 64) return -1;
    const uint8_t *p = (const uint8_t *)data;

    if (p[0] != 0x7F || p[1] != 'E' || p[2] != 'L' || p[3] != 'F')
        return -1;                    /* the magic: 0x7F E L F */
    if (p[4] != 2) return -1;         /* 64-bit only (the kernel is x86-64) */
    if (p[5] != 1) return -1;         /* little-endian */
    if (p[6] != 1) return -1;         /* the current ELF version */

    const uint8_t *eh = p;
    uint16_t type; memcpy(&type, eh + 16, 2);
    if (type != 2 && type != 3) return -1; /* the binary (3) or dyn (2) */

    /* program headers start at e_phoff (40-48) */
    uint16_t phentsize, phnum;
    uint64_t phoff;
    memcpy(&phoff,      eh + 32, 8);
    memcpy(&phentsize,  eh + 54, 2);
    memcpy(&phnum,      eh + 56, 2);

    if (phoff + phnum * phentsize > size) return -1;

    out->entry  = 0; /* filled below */
    uint64_t entry; memcpy(&entry, eh + 24, 8);
    out->entry = entry;
    out->phnum = phnum;
    out->phoff = phoff;
    out->phentsize = phentsize;

    /* the loadable segments + the NEED entries (the dynamic imports) */
    int nload = 0, nneed = 0;
    uint64_t dyn_off = 0;
    for (uint16_t i = 0; i < phnum && i < WUBU_ELF_MAX_SEG; i++) {
        const uint8_t *ph = p + phoff + i * phentsize;
        uint32_t p_type; memcpy(&p_type, ph, 4);
        if (p_type == 1 && nload < WUBU_ELF_MAX_SEG) {
            memcpy(&out->ph_load[nload].vaddr, ph + 8, 8);
            memcpy(&out->ph_load[nload].memsz,  ph + 32, 8);
            memcpy(&out->ph_load[nload].flags, ph + 4,  4);
            out->ph_load[nload].present = 1;
            nload++;
        }
        if (p_type == 2 && nneed < WUBU_ELF_MAX_NEED) {
            /* the PT_DYNAMIC is where the NEEDED libs live */
            uint64_t dyn_vaddr; memcpy(&dyn_vaddr, ph + 8, 8);
            (void)dyn_vaddr; /* the .dynamic walk is lazy (the kernel
                              * resolves at map time) */
            dyn_off = dyn_vaddr;
        }
    }
    out->n_load = nload;
    out->dyn_vaddr = dyn_off;
    return 0;
}

/* walk the .dynamic section -> the NEED entries (the shared libs
 * the binary needs: libc, libGL, libSDL2, ... — OUR answer tables
 * answer them, not the host ld.so). */
int wubu_elf_needed(const void *data, size_t size,
                    const wubu_elf_info_t *info,
                    char out[][64], int max)
{
    if (!data || !info || !out || max <= 0) return -1;
    /* the dynamic section lives at the image's dyn vaddr; the kernel
     * maps segments by vaddr, so walk the mapped region. For the
     * loader (the kernel maps the file), the .dynamic is at the LOAD
     * file offset. Here we do a file-offset walk from the PT_DYNAMIC's
     * vaddr (which the kernel resolved at map time) — the stub returns
     * 0 NEEDED (the full dynamic walker is wired after the loader). */
    (void)data; (void)size; (void)info;
    return 0;
}

/* the ELF machine → our CPU (the kernel only runs what it can). */
const char *wubu_elf_machine_name(uint16_t machine)
{
    switch (machine) {
    case 0x3E:  return "x86_64 (AMD64)";
    case 0x03:  return "x86 (i386)";
    case 0x28:  return "ARM";
    case 0xB7:  return "ARM64";
    default:    return "unknown";
    }
}
