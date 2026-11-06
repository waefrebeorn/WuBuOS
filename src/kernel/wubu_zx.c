/*
 * wubu_zx.c  --  the module-runner primitive (ZXE-style loader core).
 *
 * One reusable call that runs a raw executable image (PE) through the whole
 * loader pipeline: parse -> sections -> map exec memory -> copy sections ->
 * relocate (base fixups) -> snap IAT via the symbol registry -> call entry.
 * Higher layers (the /n namespace loader, a .ZXE reader, the boot shell) call
 * this instead of re-implementing the loader. Kernel-clean C11; memory is
 * supplied by the caller's ops ({map[,unmap]}) so the kernel fixed allocator
 * or a host mmap both work.
 */

#include "wubu_zx.h"
#include "wubu_modsym.h"

int wubu_zx_run(const wubu_zx_ops *ops, const uint8_t *bytes, size_t size,
                uintptr_t base, wubu_pe_resolve_fn resolve, int *out_err)
{
    if (!ops || !ops->map || !bytes || size == 0) {
        if (out_err) *out_err = WUBU_ZX_ERR_PARAM; return WUBU_ZX_ERR_PARAM;
    }
    if (!resolve) resolve = wubu_modsym_resolve;

    wubu_pe_info_t info;
    if (wubu_pe_parse(bytes, size, &info) != 0) {
        if (out_err) *out_err = WUBU_ZX_ERR_PARSE; return WUBU_ZX_ERR_PARSE;
    }

    wubu_pe_section_t sects[16];
    int ns = wubu_pe_sections(bytes, size, &info, sects, 16);
    if (ns <= 0) { if (out_err) *out_err = WUBU_ZX_ERR_SECTIONS; return WUBU_ZX_ERR_SECTIONS; }

    /* span: highest section virtual end (+ classic slack) */
    size_t span = 0x4000;
    for (int i = 0; i < ns; i++) {
        uint32_t end = sects[i].virtual_addr + sects[i].virtual_size;
        if ((size_t)end > span) span = (size_t)end;
    }
    span = (span + 0xFFFu) & ~(size_t)0xFFFu;

    uint8_t *image = ops->map(base, span);
    if (!image) { if (out_err) *out_err = WUBU_ZX_ERR_MAP; return WUBU_ZX_ERR_MAP; }

    /* copy each section raw bytes to its virtual offset (mapper must zero the
     * span so any bss-like tail is zeroed); guard raw runs past the file */
    for (int i = 0; i < ns; i++) {
        size_t raw_sz = sects[i].raw_size;
        if ((size_t)sects[i].raw_ptr + raw_sz > size) raw_sz = size - (size_t)sects[i].raw_ptr;
        uint8_t *dst = image + sects[i].virtual_addr;
        for (size_t k = 0; k < raw_sz; k++) dst[k] = bytes[sects[i].raw_ptr + k];
    }

    if (wubu_pe_relocate(&info, sects, ns, bytes, size, image, base) != 0) {
        if (out_err) *out_err = WUBU_ZX_ERR_RELOC; return WUBU_ZX_ERR_RELOC;
    }
    if (wubu_pe_snap_iat(bytes, size, &info, sects, ns, image, resolve) < 0) {
        if (out_err) *out_err = WUBU_ZX_ERR_IAT; return WUBU_ZX_ERR_IAT;
    }

    int rc = wubu_pe_call_entry(&info, sects, ns, image);

    if (ops->unmap) ops->unmap(image, base, span);
    if (out_err) *out_err = 0;
    return rc;
}

int wubu_zx_run_simple(wubu_zx_map_fn map,
                       const uint8_t *bytes, size_t size, uintptr_t base,
                       wubu_pe_resolve_fn resolve, int *out_err)
{
    wubu_zx_ops ops = { map, NULL };
    return wubu_zx_run(&ops, bytes, size, base, resolve, out_err);
}
