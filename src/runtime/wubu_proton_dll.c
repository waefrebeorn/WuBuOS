/* wubu_proton_dll.c -- Proton DLL management subsystem (self-contained).
 *
 * wubu_proton_register_dll/find_dll/resolve_deps. Uses wubu_proton_t /
 * proton_dll_t / proton_dll_type_t (wubu_proton.h). Minimal includes.
 */

#include "wubu_proton.h"

int wubu_proton_register_dll(wubu_proton_t *p, const char *name,
                              proton_dll_type_t type) {
    if (!p || !name) return -1;
    if (p->num_dlls >= PROTON_MAX_DLLS) return -1;

    proton_dll_t *dll = &p->dlls[p->num_dlls];
    strncpy(dll->name, name, PROTON_MAX_DLL_NAME - 1);
    dll->name[PROTON_MAX_DLL_NAME - 1] = '\0';
    dll->type = type;
    dll->base_addr = 0;
    dll->loaded = 0;
    p->num_dlls++;
    return 0;
}

int wubu_proton_find_dll(wubu_proton_t *p, const char *name) {
    if (!p || !name) return -1;

    for (int i = 0; i < p->num_dlls; i++) {
        if (strcmp(p->dlls[i].name, name) == 0 ||
            strcasecmp(p->dlls[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

int wubu_proton_resolve_deps(wubu_proton_t *p) {
    if (!p) return -1;

    /* Mark all built-in DLLs as "loaded" (resolved) */
    for (int i = 0; i < p->num_dlls; i++) {
        if (p->dlls[i].type == DLL_BUILTIN || p->dlls[i].type == DLL_PASSTHROUGH) {
            p->dlls[i].loaded = 1;
            /* Assign base addresses for loaded DLLs */
            p->dlls[i].base_addr = 0x7FFE0000u + (uint32_t)i * 0x100000u;
        }
    }

    p->dll_resolved += (uint64_t)p->num_dlls;
    return 0;
}
