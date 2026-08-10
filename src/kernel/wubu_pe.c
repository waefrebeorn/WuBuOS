/*
 * wubu_pe.c -- the IN-KERNEL PE loader (the Windows format, on OUR
 * kernel — not an Arch/Wine connection).
 *
 * The user's correction: "all this needs to be running on temple os
 * kernel our kernel not just a arch connection." The container path
 * (wubu_exec_win_pe -> host Wine) is the scaffold; the target is OUR
 * kernel loading the Windows binary directly.
 *
 * This is the kernel-side PE loader:
 *   - validate the MZ + the PE signature
 *   - parse the COFF header (machine/subsystem/entry)
 *   - walk the SECTION table (the mapped image layout)
 *   - walk the IMPORT table (the DLLs + the functions the game
 *     needs) — the import manifest OUR kernel's personality tables
 *     must answer (D3D8/DSOUND/WINMM/WS2_32 are OUR tables, not
 *     host Wine's)
 *
 * The image is not host-exec'd — the loader produces the in-kernel
 * mapping plan + the import contract. C11, kernel-clean (no libc
 * heap: the caller provides the buffers).
 */
#include "wubu_pe.h"

#include <string.h>

/* PE constants */
#define IMAGE_DOS_SIGNATURE  0x5A4D     /* MZ */
#define IMAGE_NT_SIGNATURE   0x00004550 /* PE\0\0 */
#define IMAGE_DIR_IMPORT     1

/* PE1: validate + parse the headers. Returns 0 on success. */
int wubu_pe_parse(const void *data, size_t size, wubu_pe_info_t *out)
{
    if (!data || !out || size < 0x40) return -1;
    const uint8_t *p = (const uint8_t *)data;

    uint16_t dos_magic;
    memcpy(&dos_magic, p, 2);
    if (dos_magic != IMAGE_DOS_SIGNATURE) return -1;

    uint32_t pe_off;
    memcpy(&pe_off, p + 0x3C, 4);
    if (pe_off + 24 > size) return -1;
    const uint8_t *pe = p + pe_off;

    uint32_t nt_sig;
    memcpy(&nt_sig, pe, 4);
    if (nt_sig != IMAGE_NT_SIGNATURE) return -1;

    /* COFF header: machine(2) nsects(2) timestamp(4) symptr(4)
     * nsyms(4) optsize(2) chars(2) */
    memcpy(&out->machine, pe + 4, 2);
    memcpy(&out->num_sections, pe + 6, 2);
    memcpy(&out->optional_size, pe + 20, 2);

    const uint8_t *opt = pe + 24;
    if (pe_off + 24 + out->optional_size > size) return -1;

    /* optional header: magic(2) ... entry(16) ... subsystem(68+2) */
    memcpy(&out->entry_rva, opt + 16, 4);
    memcpy(&out->subsystem, opt + 68, 2);
    memcpy(&out->image_base, opt + 28, 4);
    if (out->optional_size >= 96) {
        /* the data directory: import is the 2nd entry */
        memcpy(&out->import_rva, opt + 104, 4);
        memcpy(&out->import_size, opt + 108, 4);
    } else {
        out->import_rva = 0;
        out->import_size = 0;
    }
    out->pe_offset = pe_off;
    return 0;
}

/* PE2: walk the section table. */
int wubu_pe_sections(const void *data, size_t size,
                     const wubu_pe_info_t *info,
                     wubu_pe_section_t *out, int max)
{
    if (!data || !info || !out || max <= 0) return -1;
    const uint8_t *p = (const uint8_t *)data;
    const uint8_t *sect =
        p + info->pe_offset + 24 + info->optional_size;
    int n = info->num_sections < max ? info->num_sections : max;
    for (int i = 0; i < n; i++) {
        const uint8_t *s = sect + i * 40;
        /* name(8) vsize(8) vaddr(12) rawsize(16) rawptr(20) */
        memcpy(out[i].name, s, 8);
        out[i].name[8] = '\0';
        memcpy(&out[i].virtual_size, s + 8, 4);
        memcpy(&out[i].virtual_addr, s + 12, 4);
        memcpy(&out[i].raw_size, s + 16, 4);
        memcpy(&out[i].raw_ptr, s + 20, 4);
        out[i].characteristics = 0;
        memcpy(&out[i].characteristics, s + 36, 4);
    }
    return n;
}

/* PE3: resolve an RVA to the raw file offset (the mapping plan). */
int wubu_pe_rva_to_raw(const wubu_pe_section_t *sects, int n,
                       uint32_t rva)
{
    for (int i = 0; i < n; i++) {
        uint32_t va = sects[i].virtual_addr;
        if (rva >= va && rva < va + sects[i].virtual_size) {
            uint32_t delta = rva - va;
            if (delta < sects[i].raw_size)
                return (int)(sects[i].raw_ptr + delta);
            return -1;   /* in the virtual tail (zero-filled) */
        }
    }
    return -1;
}

/* PE4: walk the import descriptors — the DLLs the game needs. */
int wubu_pe_imports(const void *data, size_t size,
                    const wubu_pe_info_t *info,
                    const wubu_pe_section_t *sects, int nsects,
                    wubu_pe_import_t *out, int max)
{
    if (!data || !info || !out || max <= 0) return -1;
    if (!info->import_rva) return 0;
    int off = wubu_pe_rva_to_raw(sects, nsects, info->import_rva);
    if (off < 0) return -1;
    const uint8_t *p = (const uint8_t *)data;
    int count = 0;
    /* each descriptor: ilt(0) ts(4) fwd(8) name_rva(12) iat(16) */
    for (int i = 0; i < max; i++) {
        const uint8_t *d = p + off + i * 20;
        uint32_t name_rva = 0, ilt_rva = 0;
        memcpy(&name_rva, d + 12, 4);
        memcpy(&ilt_rva, d + 0, 4);
        if (!name_rva) break;   /* the terminator */
        int name_off = wubu_pe_rva_to_raw(sects, nsects, name_rva);
        if (name_off < 0) break;
        /* the DLL name is NUL-terminated in the file; copy bounded
         * (kernel-clean: no snprintf under -nostdlib) */
        const char *name = (const char *)p + name_off;
        size_t len = 0;
        while (len + 1 < sizeof(out[count].dll) && name[len]) len++;
        memcpy(out[count].dll, name, len);
        out[count].dll[len] = '\0';
        /* count the functions (the IAT) */
        int nfuncs = 0;
        if (ilt_rva) {
            int ilt_off = wubu_pe_rva_to_raw(sects, nsects, ilt_rva);
            if (ilt_off >= 0) {
                for (int k = 0; k < 4096; k++) {
                    uint32_t thunk;
                    memcpy(&thunk, p + ilt_off + k * 4, 4);
                    if (!thunk) break;
                    nfuncs++;
                }
            }
        }
        out[count].func_count = nfuncs;
        count++;
    }
    return count;
}
