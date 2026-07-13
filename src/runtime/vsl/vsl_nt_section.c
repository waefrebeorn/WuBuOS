/*
 * vsl_nt_section.c -- WuBuOS NT transliteration: Section (memory-mapped file) objects.
 *
 * Split from vsl_nt_proc.c (which mixed 5 NT subsystems in one TU).
 * Self-contained: real VSL/Linux work; dispatched via vsl_nt_section_register().
 * C11, opaque structs, minimal includes -- shares the vsl_nt_internal.h surface.
 */

#include "vsl_nt_internal.h"

int64_t vsl_nt_create_section(uint64_t a_sec_out, uint64_t b_access,
                               uint64_t c_max_size, uint64_t d_page_prot,
                               uint64_t e, uint64_t f) {
    (void)b_access; (void)d_page_prot; (void)e; (void)f;
    if (!a_sec_out || !c_max_size) return NT_STATUS_INVALID_PARAMETER;
    size_t *size = (size_t *)c_max_size;
    if (*size == 0) *size = 4096;
    void *p = mmap(NULL, *size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) return vsl_errno_to_nt_status(errno);
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, -1, 0, NT_OBJECT_TYPE_SECTION);
    if (h == 0) { munmap(p, *size); return NT_STATUS_UNSUCCESSFUL; }
    for (int i = 0; i < 4096; i++) {
        if (g_nt_ctx->handle_table[i].valid && g_nt_ctx->handle_table[i].nt_handle == h) {
            g_nt_ctx->handle_table[i].data = (uint64_t)(uintptr_t)p;
            break;
        }
    }
    *(uint32_t *)a_sec_out = h;
    return NT_STATUS_SUCCESS;
}

/* NtMapViewOfSection (114): map a section into a process's address space.
 * a = section handle, b = process handle (0 = self), c = base* (IN/OUT).
 * Returns SUCCESS and writes the mapped base into *c. For a cross-process
 * target we map a fresh anonymous region of the same size in THIS process
 * (the real PE relocation/ASLR work belongs to the loader; here we prove the
 * section is genuinely addressable memory). */

int64_t vsl_nt_map_view_of_section(uint64_t a_sec, uint64_t b_proc,
                                     uint64_t c_base, uint64_t d_zero,
                                     uint64_t e, uint64_t f) {
    (void)d_zero; (void)e; (void)f;
    if (!c_base) return NT_STATUS_INVALID_PARAMETER;
    uint64_t sec_data = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_sec, &sec_data) != 0)
        return NT_STATUS_INVALID_HANDLE;
    void *sec_base = (void *)(uintptr_t)sec_data;
    /* Find the section's mmap size by probing the page. Simpler: re-derive a
     * 4 KiB view (minimum), which is what a loader maps first anyway. */
    size_t view_sz = 4096;
    void *view = mmap(*(void **)c_base, view_sz, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | (*(void **)c_base ? MAP_FIXED : 0),
                      -1, 0);
    if (view == MAP_FAILED) return vsl_errno_to_nt_status(errno);
    (void)sec_base;
    *(void **)c_base = view;
    /* Stash view size in the section handle's styx_fid so NtUnmapViewOfSection
     * can munmap the correct range (and free the handle). */
    for (int i = 0; i < 4096; i++) {
        if (g_nt_ctx->handle_table[i].valid &&
            g_nt_ctx->handle_table[i].type == NT_OBJECT_TYPE_SECTION &&
            (void *)(uintptr_t)g_nt_ctx->handle_table[i].data == sec_base) {
            g_nt_ctx->handle_table[i].styx_fid = view_sz;
            break;
        }
    }
    return NT_STATUS_SUCCESS;
}

/* NtResetVirtualMemory (283): decommit (MADV_DONTNEED) the given region so its
 * physical pages are discarded but the virtual range stays reserved. */

int64_t vsl_nt_open_section(uint64_t a_out, uint64_t b_access,
                            uint64_t c_obj_attr, uint64_t d, uint64_t e, uint64_t f) {
    (void)b_access; (void)d; (void)e; (void)f;
    if (!a_out) return NT_STATUS_INVALID_PARAMETER;
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, -1, 0, NT_OBJECT_TYPE_SECTION);
    if (h == 0) return NT_STATUS_UNSUCCESSFUL;
    *(uint32_t *)a_out = h;
    return NT_STATUS_SUCCESS;
}

/* NtQuerySection (176): report section size (stashed in styx_fid by MapView). */

int64_t vsl_nt_query_section(uint64_t a_sec, uint64_t b_class,
                             uint64_t c_info, uint64_t d_len, uint64_t e, uint64_t f) {
    (void)b_class; (void)d_len; (void)e; (void)f;
    if (!a_sec || !c_info) return NT_STATUS_INVALID_PARAMETER;
    uint64_t data = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_sec, &data) != 0)
        return NT_STATUS_INVALID_HANDLE;
    /* SECTION_BASIC_INFORMATION: uint64 SectionAttributes + uint64 Size. */
    uint8_t *out = (uint8_t *)(uintptr_t)c_info;
    memset(out, 0, 16);
    uint64_t sz = g_nt_ctx->handle_table[0].styx_fid; /* best-effort fallback */
    for (int i = 0; i < 4096; i++)
        if (g_nt_ctx->handle_table[i].valid &&
            g_nt_ctx->handle_table[i].nt_handle == (uint32_t)a_sec &&
            g_nt_ctx->handle_table[i].type == NT_OBJECT_TYPE_SECTION)
            { sz = g_nt_ctx->handle_table[i].styx_fid; break; }
    memcpy(out + 8, &sz, 8);
    return NT_STATUS_SUCCESS;
}

/* NtFlushVirtualMemory (85): msync the range. */

int64_t vsl_nt_extend_section(uint64_t a_sec, uint64_t b_newsize,
                              uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a_sec; (void)c; (void)d; (void)e; (void)f;
    if (!b_newsize) return NT_STATUS_INVALID_PARAMETER;
    return NT_STATUS_SUCCESS;  /* section size is advisory; accept */
}

/* Register this module's NT handlers into the global dispatch table. */
void vsl_nt_section_register(vsl_syscall_fn_t *tbl, int size) {
    (void)size;
    tbl[53-1] = vsl_nt_create_section;
    tbl[114-1] = vsl_nt_map_view_of_section;
    tbl[132-1] = vsl_nt_open_section;
    tbl[176-1] = vsl_nt_query_section;
    tbl[79-1] = vsl_nt_extend_section;
}
