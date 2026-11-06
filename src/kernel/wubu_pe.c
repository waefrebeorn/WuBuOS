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
#define IMAGE_NT_OPTIONAL_HDR32_MAGIC 0x10b
#define IMAGE_NT_OPTIONAL_HDR64_MAGIC 0x20b

/* PE1: validate + parse the headers. Returns 0 on success. Supports BOTH
 * PE32 (magic 0x10B) and PE32+ (magic 0x20B, x64). The two differ in WHERE
 * fields live in the optional header; entry_rva/subsystem are at the same
 * offsets, but image_base is u32@opt+28 (PE32) vs u64@opt+32 (PE32+), and
 * the 16 data directories begin at opt+96 (PE32) vs opt+112 (PE32+). So the
 * import(1)/reloc(5)/tls(9) RVAs are directory_base + {8,12}/{40,44}/{72,76}.
 * image_base is kept as a u32 in info_t (loader maps on 32-bit bases;
 * a PE32+ whose base exceeds 4GiB must use MAP_FIXED at the low part). */
int wubu_pe_parse(const void *data, size_t size, wubu_pe_info_t *out)
{
    if (!data || !out || size < 0x40) return -1;
    memset(out, 0, sizeof(*out));
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

    /* optional header magic distinguishes PE32 from PE32+ */
    uint16_t magic;
    memcpy(&magic, opt, 2);
    out->is_pe32plus = (magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC);

    memcpy(&out->entry_rva, opt + 16, 4);
    memcpy(&out->subsystem, opt + 68, 2);
    if (out->is_pe32plus) {
        /* PE32+: ImageBase is the u64 at opt+32; keep low 32 bits. */
        memcpy(&out->image_base, opt + 32, 4);
    } else {
        memcpy(&out->image_base, opt + 28, 4);
    }
    uint32_t dd = out->is_pe32plus ? 112u : 96u;   /* data-dir table offset */
    if (out->optional_size >= dd + 16) {
        /* data dir 1 = import */
        memcpy(&out->import_rva, opt + dd + 8, 4);
        memcpy(&out->import_size, opt + dd + 12, 4);
    } else {
        out->import_rva = 0;
        out->import_size = 0;
    }
    if (out->optional_size >= dd + 48) {
        /* data dir 5 = base relocation (.reloc) */
        memcpy(&out->reloc_rva, opt + dd + 40, 4);
        memcpy(&out->reloc_size, opt + dd + 44, 4);
    } else {
        out->reloc_rva = 0;
        out->reloc_size = 0;
    }
    if (out->optional_size >= dd + 80) {
        /* data dir 9 = TLS */
        memcpy(&out->tls_rva, opt + dd + 72, 4);
        memcpy(&out->tls_size, opt + dd + 76, 4);
    } else {
        out->tls_rva = 0;
        out->tls_size = 0;
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

/* PE5: apply base relocations. The .reloc directory is a stream of
 * IMAGE_BASE_RELOCATION blocks (page_rva + block size), each followed by
 * 16-bit fixup entries: high nibble = type, low 12 bits = offset within the
 * page. HIGHLOW(3) patches a u32, DIR64(10) a u64, both raised by the base
 * delta. Type 0 (absolute) is a no-op. */
int wubu_pe_relocate(const wubu_pe_info_t *info,
                     const wubu_pe_section_t *sects, int nsects,
                     const void *file, size_t file_size,
                     void *image, uintptr_t load_base)
{
    if (!info || !sects || !file || !image) return -1;
    if (!info->reloc_rva) return 0;                 /* nothing to fix up */

    int off = wubu_pe_rva_to_raw(sects, nsects, info->reloc_rva);
    if (off < 0) return -1;
    const uint8_t *p = (const uint8_t *)file;
    int64_t delta = (int64_t)load_base - (int64_t)info->image_base;
    if (!delta) return 0;                            /* at preferred base */

    for (;;) {
        if ((size_t)off >= file_size) return 0;   /* clean end of reloc stream */
        if ((size_t)off + 8 > file_size) return -1;
        uint32_t page_rva, blk_size;
        memcpy(&page_rva, p + off, 4);
        memcpy(&blk_size, p + off + 4, 4);
        if (!page_rva || !blk_size) return 0;        /* terminator */
        if (blk_size < 8) return -1;
        if ((size_t)off + blk_size > file_size) return -1;

        int n = (int)((blk_size - 8) / 2);
        for (int i = 0; i < n; i++) {
            uint16_t entry;
            memcpy(&entry, p + off + 8 + i * 2, 2);
            unsigned type = entry >> 12;
            uint32_t rva = page_rva + (entry & 0xFFF);
            uint8_t *dst = (uint8_t *)image + rva;
            if (type == 3) {                        /* IMAGE_REL_BASED_HIGHLOW */
                uint32_t v;
                memcpy(&v, dst, 4);
                v += (uint32_t)delta;
                memcpy(dst, &v, 4);
            } else if (type == 10) {                /* IMAGE_REL_BASED_DIR64 */
                uint64_t v;
                memcpy(&v, dst, 8);
                v += (uint64_t)delta;
                memcpy(dst, &v, 8);
            }
            /* type 0 (abs) / others: no fixup */
        }
        off = (int)((off + blk_size + 3) & ~3u);    /* 4-byte align */
    }
}

/* PE9: run the TLS callbacks. The .tls data directory (32 bytes, x64 flat VAs)
 * is copied into the mapped image at info->tls_rva. IMAGE_TLS_DIRECTORY's
 * AddressOfCallBacks sits at offset 24 — a VA for the NULL-terminated callback
 * array. Convert it back to an in-buffer offset by subtracting the image base,
 * then invoke each non-NULL entry in order. Returns the number called. */
int wubu_pe_tls_callbacks(const wubu_pe_info_t *info,
                          const wubu_pe_section_t *sects, int nsects,
                          void *image)
{
    (void)sects; (void)nsects;   /* the directory/array are in the mapped image */
    if (!info || !image || !info->tls_rva) return 0;
    uint8_t *dir = (uint8_t *)image + info->tls_rva;
    uintptr_t aocb = 0;
    memcpy(&aocb, dir + 24, sizeof(uintptr_t));       /* AddressOfCallBacks */
    if (!aocb) return 0;
    uintptr_t off = aocb - (uintptr_t)info->image_base;
    uint8_t *arr = (uint8_t *)image + off;
    int n = 0;
    for (int i = 0; i < 64; i++) {
        uintptr_t fp = 0;
        memcpy(&fp, arr + i * sizeof(uintptr_t), sizeof(uintptr_t));
        if (!fp) break;
        ((void (*)(void))fp)();
        n++;
    }
    return n;
}

/* PE10: run TLS callbacks then transfer to the entry point. The entry code sits
 * at the mapped buffer offset info->entry_rva; call it and return its result,
 * or -1 when the image declares no entry. */
int wubu_pe_call_entry(const wubu_pe_info_t *info,
                       const wubu_pe_section_t *sects, int nsects,
                       void *image)
{
    if (!info || !image) return -1;
    wubu_pe_tls_callbacks(info, sects, nsects, image);
    if (!info->entry_rva) return -1;
    int (*entry)(void) = (int (*)(void))((uint8_t *)image + info->entry_rva);
    return entry();
}

/* PE11: snap the IAT of a mapped image. Walks each import descriptor's thunk
 * chain (OriginalFirstThunk -> IMAGE_IMPORT_BY_NAME, or by-ordinal when bit 63
 * is set), asks the resolver for the address, and writes it into the matching
 * IAT slot (FirstThunk) inside the mapped image. */
int wubu_pe_snap_iat(const void *data, size_t size,
                     const wubu_pe_info_t *info,
                     const wubu_pe_section_t *sects, int nsects,
                     void *image, wubu_pe_resolve_fn resolve)
{
    if (!data || !info || !image || !resolve) return -1;
    if (!info->import_rva) return 0;
    int off = wubu_pe_rva_to_raw(sects, nsects, info->import_rva);
    if (off < 0) return -1;
    const uint8_t *p = (const uint8_t *)data;
    int patched = 0;

    for (int i = 0; i < 4096; i++) {
        const uint8_t *d = p + off + i * 20;
        uint32_t name_rva = 0, ilt_rva = 0, iat_rva = 0;
        memcpy(&name_rva, d + 12, 4);
        memcpy(&ilt_rva, d + 0, 4);
        memcpy(&iat_rva, d + 16, 4);
        if (!name_rva) break;                       /* terminator descriptor */

        int name_off = wubu_pe_rva_to_raw(sects, nsects, name_rva);
        int iat_off  = wubu_pe_rva_to_raw(sects, nsects, iat_rva);
        if (name_off < 0 || iat_off < 0) return -1;
        const char *dll = (const char *)p + name_off;

        /* the thunk chain: OriginalFirstThunk (ILT) or FirstThunk if ILT absent */
        int thunk_raw = -1;
        if (ilt_rva) thunk_raw = wubu_pe_rva_to_raw(sects, nsects, ilt_rva);
        else         thunk_raw = iat_off;
        if (thunk_raw < 0) return -1;

        for (int k = 0; k < 4096; k++) {
            uint64_t thunk = 0;
            memcpy(&thunk, p + thunk_raw + k * 8, 8);
            if (!thunk) break;
            char func[128] = { 0 };
            uint32_t ordinal = 0; int is_ordinal = 0;
            if (thunk & (1ULL << 63)) {             /* import by ordinal */
                is_ordinal = 1;
                ordinal = (uint32_t)(thunk & 0xFFFF);
            } else {                                 /* import by name */
                int noff = wubu_pe_rva_to_raw(sects, nsects,
                                              (uint32_t)(thunk & 0x7FFFFFFF));
                if (noff < 0) continue;
                const char *nm = (const char *)p + noff + 2;   /* skip hint */
                size_t len = 0;
                while (len + 1 < sizeof(func) && nm[len]) len++;
                memcpy(func, nm, len);
                func[len] = '\0';
            }
            uintptr_t addr = 0;
            if (resolve(dll, func, ordinal, is_ordinal, &addr) != 0)
                return -1;                          /* dependency not found */
            uint64_t slot = (uint64_t)addr;
            /* the IAT slot being patched is the FirstThunk (iat_rva) location */
            memcpy((uint8_t *)image + iat_rva + k * 8, &slot, 8);
            patched++;
        }
    }
    return patched;
}
