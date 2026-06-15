/*
 * wubu_proton.c  --  WuBuOS Proton: Windows Compatibility Layer Implementation
 *
 * Cell 092: Proton-style translation layer over VSL.
 * WuBuOS → VSL → Proton → Windows PE
 *
 * Translates Win32 API calls to VSL/Linux equivalents,
 * validates/maps PE binaries, resolves DLLs.
 */

#include "wubu_proton.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

/* -- Built-in API Translation Table -------------------------- */
/* Maps common Win32 APIs to VSL syscalls */

static const proton_api_map_t default_apis[] = {
    /* Kernel32 → VSL file/process APIs */
    {"CreateFileW",    2, 7, 1, 0}, /* VSL_SYSCALL_OPEN */
    {"ReadFile",       0, 5, 1, 0}, /* VSL_SYSCALL_READ */
    {"WriteFile",      1, 5, 1, 0}, /* VSL_SYSCALL_WRITE */
    {"CloseHandle",    3, 1, 1, 0}, /* VSL_SYSCALL_CLOSE */
    {"GetFileSize",    -1, 2, 1, 0}, /* no direct VSL equiv */
    {"SetFilePointer", -1, 4, 1, 0},
    {"VirtualAlloc",   9, 4, 1, 0}, /* VSL_SYSCALL_MMAP */
    {"VirtualFree",    11, 3, 1, 0}, /* VSL_SYSCALL_MUNMAP */
    {"GetProcAddress", -1, 2, 1, 0},
    {"LoadLibraryW",   -1, 1, 1, 0},
    {"FreeLibrary",    -1, 1, 1, 0},
    {"GetModuleHandle",-1, 1, 1, 0},
    {"ExitProcess",    60, 1, 0, 0}, /* VSL_SYSCALL_EXIT */
    {"GetTickCount",   -1, 0, 1, 0},
    {"Sleep",          -1, 1, 0, 0},
    {"HeapAlloc",      9, 3, 1, 0}, /* → mmap */
    {"HeapFree",       11, 3, 1, 0}, /* → munmap */

    /* User32 → VSL input/display */
    {"GetMessageW",    -1, 4, 1, 0},
    {"DispatchMessage",-1, 1, 1, 0},
    {"CreateWindowEx",-1,12, 1, 0},

    /* GDI32 → VBE framebuffer */
    {"BitBlt",         -1, 9, 1, 0},
    {"TextOutW",       -1, 5, 1, 0},

    /* WS2_32 (Winsock) → VSL socket */
    {"socket",         41, 3, 1, 0}, /* VSL_SYSCALL_SOCKET */
    {"connect",        42, 3, 1, 0},
    {"send",           44, 4, 1, 0},
    {"recv",           45, 4, 1, 0},
    {"closesocket",    3, 1, 1, 0},  /* → close */

    /* Vulkan → pass through to VSL Vulkan driver */
    {"vkCreateInstance",    -1, 3, 1, 1}, /* flag 1 = passthrough */
    {"vkCreateDevice",      -1, 5, 1, 1},
    {"vkQueueSubmit",       -1, 4, 1, 1},
    {"vkCmdDraw",           -1, 6, 0, 1},

    /* NtDll → VSL syscall direct */
    {"NtCreateFile",    2, 7, 1, 0},
    {"NtReadFile",      0, 9, 1, 0},
    {"NtWriteFile",     1, 9, 1, 0},
    {"NtClose",         3, 1, 1, 0},
};

#define DEFAULT_API_COUNT (sizeof(default_apis) / sizeof(default_apis[0]))

/* -- Built-in DLLs (Windows DLLs we provide implementations for) -- */

static const struct {
    const char *name;
    proton_dll_type_t type;
} builtin_dlls[] = {
    {"kernel32.dll",  DLL_BUILTIN},
    {"user32.dll",    DLL_BUILTIN},
    {"gdi32.dll",     DLL_BUILTIN},
    {"ntdll.dll",     DLL_BUILTIN},
    {"ws2_32.dll",    DLL_BUILTIN},
    {"advapi32.dll",  DLL_BUILTIN},
    {"msvcrt.dll",    DLL_BUILTIN},
    {"d3d9.dll",      DLL_NATIVE},   /* Wine DX9 → Vulkan */
    {"d3d11.dll",     DLL_NATIVE},   /* Wine DX11 → Vulkan */
    {"vulkan-1.dll",  DLL_PASSTHROUGH}, /* Direct VSL passthrough */
    {"xinput1_3.dll", DLL_NATIVE},
    {"steam_api.dll", DLL_NATIVE},
};

#define BUILTIN_DLL_COUNT (sizeof(builtin_dlls) / sizeof(builtin_dlls[0]))

/* -- Lifecycle ----------------------------------------------- */

int wubu_proton_init(wubu_proton_t *p) {
    memset(p, 0, sizeof(*p));
    p->state = PROTON_INITIALIZING;

    /* Try to connect to VSL */
    p->vsl_connected = 1; /* Assume VSL is available */

    /* Load built-in API translations */
    int rc = wubu_proton_load_default_apis(p);
    if (rc < 0) {
        p->state = PROTON_ERROR;
        return -1;
    }

    /* Register built-in DLLs */
    for (int i = 0; i < (int)BUILTIN_DLL_COUNT; i++) {
        wubu_proton_register_dll(p, builtin_dlls[i].name, builtin_dlls[i].type);
    }

    p->state = PROTON_READY;
    return 0;
}

void wubu_proton_shutdown(wubu_proton_t *p) {
    if (p->api_table) {
        /* api_table is heap-allocated */
    }
    p->api_table = NULL;
    p->api_count = 0;
    p->state = PROTON_OFF;
    p->vsl_connected = 0;
}

/* -- PE Validation ------------------------------------------- */

int wubu_proton_is_pe(const uint8_t *data, size_t size) {
    /* Minimum: MZ header + PE signature */
    if (size < 64) return 0;  /* Need at least DOS header */

    /* Check MZ signature */
    if (data[0] != 'M' || data[1] != 'Z') return 0;

    /* Find PE header offset (offset 0x3C in DOS header) */
    uint32_t pe_offset = *(uint32_t *)&data[0x3C];
    if (pe_offset == 0 || pe_offset + 4 > size) return 0;

    /* Check PE signature */
    uint32_t sig = *(uint32_t *)&data[pe_offset];
    if (sig != PE_MAGIC) return 0;

    return 1;
}

int wubu_proton_validate_pe(wubu_proton_t *p, const uint8_t *data, size_t size) {
    if (!data || size < 64) return -1;

    /* Check MZ header */
    if (data[0] != 'M' || data[1] != 'Z') return -1;

    /* Get PE offset */
    uint32_t pe_offset = *(uint32_t *)&data[0x3C];
    if (pe_offset == 0 || pe_offset + 24 > size) return -1;

    /* Check PE signature */
    uint32_t sig = *(uint32_t *)&data[pe_offset];
    if (sig != PE_MAGIC) return -1;

    /* Read COFF header */
    pe_coff_header_t coff;
    memcpy(&coff, &data[pe_offset + 4], sizeof(coff));

    p->machine = coff.machine;
    if (coff.machine == PE_MACHINE_AMD64) {
        p->is_pe64 = 1;
    } else if (coff.machine == PE_MACHINE_I386) {
        p->is_pe64 = 0;
    } else {
        return -1; /* Unsupported architecture */
    }

    /* Read optional header */
    if (coff.opt_header_size >= sizeof(pe_opt_header_std_t)) {
        pe_opt_header_std_t opt;
        memcpy(&opt, &data[pe_offset + 4 + sizeof(pe_coff_header_t)], sizeof(opt));

        p->entry_point = opt.entry_point;
        uint32_t opt_start = pe_offset + 4 + sizeof(pe_coff_header_t);
        if (opt.magic == PE_OPT_MAGIC_PE32P) {
            p->is_pe64 = 1;
            /* PE32+: ImageBase is 8 bytes at offset 24 */
            if (opt_start + 24 + 8 <= size) {
                uint64_t base64;
                memcpy(&base64, &data[opt_start + 24], 8);
                p->image_base = (uint32_t)base64;
            }
        } else {
            /* PE32: ImageBase is 4 bytes at offset 28 */
            if (opt_start + 28 + 4 <= size) {
                memcpy(&p->image_base, &data[opt_start + 28], 4);
            }
        }
    }

    p->num_sections = 0;
    return 0;
}

int wubu_proton_parse_pe(wubu_proton_t *p, const uint8_t *data, size_t size) {
    if (!data || size < 64) return -1;

    /* Get PE offset */
    uint32_t pe_offset = *(uint32_t *)&data[0x3C];
    if (pe_offset + 24 > size) return -1;

    pe_coff_header_t coff;
    memcpy(&coff, &data[pe_offset + 4], sizeof(coff));

    /* Parse sections */
    uint32_t section_offset = pe_offset + 4 + sizeof(pe_coff_header_t) + coff.opt_header_size;
    int max_sections = coff.num_sections;
    if (max_sections > 32) max_sections = 32;

    p->num_sections = 0;
    for (int i = 0; i < max_sections; i++) {
        if (section_offset + sizeof(pe_section_t) > size) break;
        memcpy(&p->sections[i], &data[section_offset], sizeof(pe_section_t));
        p->num_sections++;
        section_offset += sizeof(pe_section_t);
    }

    /* Calculate total image size from sections */
    p->image_size = 0;
    for (int i = 0; i < p->num_sections; i++) {
        uint32_t end = p->sections[i].virtual_addr + p->sections[i].virtual_size;
        if (end > p->image_size) p->image_size = end;
    }

    return p->num_sections;
}

/* -- PE Loading ---------------------------------------------- */

uint32_t wubu_proton_map_sections(wubu_proton_t *p, const uint8_t *data, size_t size) {
    if (!p || !data || p->num_sections == 0) return 0;

    /* In hosted mode, we simulate mapping by returning the image base.
     * In the real kernel, this would call VSL mmap for each section. */
    uint32_t base = p->image_base;
    if (base == 0) base = p->is_pe64 ? 0x140000000u : 0x00400000u;

    p->pe_loaded++;
    return base;
}

uint32_t wubu_proton_entry_addr(const wubu_proton_t *p) {
    if (!p) return 0;
    uint32_t base = p->image_base;
    if (base == 0) base = p->is_pe64 ? 0x140000000u : 0x00400000u;
    return base + p->entry_point;
}

/* -- API Translation ----------------------------------------- */

int wubu_proton_register_api(wubu_proton_t *p, const proton_api_map_t *map) {
    if (!p || !map) return -1;
    if (!p->api_table) return -1;
    if (p->api_count >= 256) return -1; /* table full */

    p->api_table[p->api_count] = *map;
    p->api_count++;
    return 0;
}

int wubu_proton_translate_api(wubu_proton_t *p, const char *win32_name) {
    if (!p || !win32_name) return -1;

    for (int i = 0; i < p->api_count; i++) {
        if (strcmp(p->api_table[i].win32_name, win32_name) == 0) {
            p->api_translated++;
            return p->api_table[i].vsl_syscall;
        }
    }
    return -1; /* Not found  --  needs native implementation */
}

int wubu_proton_load_default_apis(wubu_proton_t *p) {
    if (!p) return -1;

    /* Allocate API translation table */
    p->api_table = (proton_api_map_t *)malloc(sizeof(default_apis));
    if (!p->api_table) return -1;

    memcpy(p->api_table, default_apis, sizeof(default_apis));
    p->api_count = (int)DEFAULT_API_COUNT;
    return 0;
}

/* -- DLL Management ------------------------------------------ */

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

/* -- Runtime Execution --------------------------------------- */

int wubu_proton_exec(wubu_proton_t *p, const uint8_t *data, size_t size,
                      const char *cmdline) {
    if (!p || !data) return -1;
    if (p->state != PROTON_READY && p->state != PROTON_RUNNING) return -1;

    /* Step 1: Validate PE */
    if (wubu_proton_validate_pe(p, data, size) != 0) return -1;

    /* Step 2: Parse PE sections */
    if (wubu_proton_parse_pe(p, data, size) < 0) return -1;

    /* Step 3: Map sections */
    uint32_t base = wubu_proton_map_sections(p, data, size);
    if (base == 0) return -1;

    /* Step 4: Resolve DLL dependencies */
    wubu_proton_resolve_deps(p);

    /* Step 5: Create VSL process and execute */
    /* In hosted mode, we simulate this. In the real kernel,
     * this would call vsl_process_create + jump to entry point. */
    p->state = PROTON_RUNNING;

    /* Return a simulated process ID */
    return (int)(p->pe_loaded % 32768) + 1;
}

/* -- Query / Diagnostics ------------------------------------- */

int wubu_proton_is_ready(const wubu_proton_t *p) {
    return p && (p->state == PROTON_READY || p->state == PROTON_RUNNING);
}

const char *wubu_proton_state_name(const wubu_proton_t *p) {
    if (!p) return "NULL";
    switch (p->state) {
        case PROTON_OFF:           return "OFF";
        case PROTON_INITIALIZING:  return "INITIALIZING";
        case PROTON_READY:         return "READY";
        case PROTON_RUNNING:       return "RUNNING";
        case PROTON_ERROR:         return "ERROR";
    }
    return "UNKNOWN";
}

void wubu_proton_dump(const wubu_proton_t *p) {
    if (!p) return;
    printf("Proton State: %s\n", wubu_proton_state_name(p));
    printf("  VSL connected: %s\n", p->vsl_connected ? "yes" : "no");
    printf("  PE type: %s\n", p->is_pe64 ? "PE64" : "PE32");
    printf("  Machine: 0x%04X\n", p->machine);
    printf("  Image base: 0x%08X\n", p->image_base);
    printf("  Entry point: 0x%08X (RVA)\n", p->entry_point);
    printf("  Sections: %d\n", p->num_sections);
    printf("  DLLs: %d\n", p->num_dlls);
    printf("  API translations: %d\n", p->api_count);
    printf("  PE loaded: %lu\n", (unsigned long)p->pe_loaded);
    printf("  API translated: %lu\n", (unsigned long)p->api_translated);
    printf("  DLL resolved: %lu\n", (unsigned long)p->dll_resolved);
}

uint64_t wubu_proton_pe_count(const wubu_proton_t *p) {
    return p ? p->pe_loaded : 0;
}

uint64_t wubu_proton_api_count(const wubu_proton_t *p) {
    return p ? p->api_translated : 0;
}
