/* wubu_proton_api.c -- WuBuOS Proton Win32->VSL API translation subsystem
 *
 * Extracted from wubu_proton.c (the default Win32 API -> VSL syscall mapping
 * table, built-in DLL catalog, and the register/translate/load-default APIs).
 * This is the symbol-resolution concern; PE loading, runtime execution,
 * diagnostics, and DXVK config stay in wubu_proton.c.
 */

#include "wubu_proton.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

/* -- Built-in API Translation Table -------------------------- */
/* Maps common Win32 APIs to VSL syscalls */

static const proton_api_map_t default_apis[] = {
    /* Kernel32 -> VSL file/process APIs */
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
    {"HeapAlloc",      9, 3, 1, 0}, /* -> mmap */
    {"HeapFree",       11, 3, 1, 0}, /* -> munmap */

    /* User32 -> VSL input/display */
    {"GetMessageW",    -1, 4, 1, 0},
    {"DispatchMessage",-1, 1, 1, 0},
    {"CreateWindowEx",-1,12, 1, 0},

    /* GDI32 -> VBE framebuffer */
    {"BitBlt",         -1, 9, 1, 0},
    {"TextOutW",       -1, 5, 1, 0},

    /* WS2_32 (Winsock) -> VSL socket */
    {"socket",         41, 3, 1, 0}, /* VSL_SYSCALL_SOCKET */
    {"connect",        42, 3, 1, 0},
    {"send",           44, 4, 1, 0},
    {"recv",           45, 4, 1, 0},
    {"closesocket",    3, 1, 1, 0},  /* -> close */

    /* Vulkan -> pass through to VSL Vulkan driver */
    {"vkCreateInstance",    -1, 3, 1, 1}, /* flag 1 = passthrough */
    {"vkCreateDevice",      -1, 5, 1, 1},
    {"vkQueueSubmit",       -1, 4, 1, 1},
    {"vkCmdDraw",           -1, 6, 0, 1},

    /* NtDll -> VSL syscall direct */
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
    {"d3d9.dll",      DLL_NATIVE},   /* Wine DX9 -> Vulkan */
    {"d3d11.dll",     DLL_NATIVE},   /* Wine DX11 -> Vulkan */
    {"vulkan-1.dll",  DLL_PASSTHROUGH}, /* Direct VSL passthrough */
    {"xinput1_3.dll", DLL_NATIVE},
    {"steam_api.dll", DLL_NATIVE},
};

#define BUILTIN_DLL_COUNT (sizeof(builtin_dlls) / sizeof(builtin_dlls[0]))

/* Register the built-in Windows DLL catalog. Called from wubu_proton_init
 * so the DLL table (defined above, in this module) stays self-contained. */
int wubu_proton_load_default_dlls(wubu_proton_t *p) {
    if (!p) return -1;
    for (int i = 0; i < (int)BUILTIN_DLL_COUNT; i++) {
        wubu_proton_register_dll(p, builtin_dlls[i].name, builtin_dlls[i].type);
    }
    return 0;
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

