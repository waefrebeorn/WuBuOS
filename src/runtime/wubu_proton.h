/*
 * wubu_proton.h — WuBuOS Proton: Windows Compatibility Layer
 *
 * Cell 092: Proton-style Windows PE binary compatibility over VSL.
 *
 * Architecture (the "Proton within Proton"):
 *   WuBuOS (ring-0) → VSL (Linux) → Proton (Wine-like) → Windows PE
 *
 * Proton translates Windows API calls (Win32, DirectX, Vulkan) to
 * Linux/VSL equivalents, allowing Windows .exe/.dll binaries to run
 * inside the VSL with near-native performance.
 *
 * This is NOT a full Wine reimplementation. It's a thin translation
 * layer that:
 *   1. Validates PE32/PE64 binaries
 *   2. Maps PE sections into VSL process memory
 *   3. Translates Win32 API calls to VSL syscalls
 *   4. Provides DirectX/Vulkan translation via VSL drivers
 *   5. Handles DLL dependency resolution
 *
 * All C11, no external deps.
 */

#ifndef WUBU_PROTON_H
#define WUBU_PROTON_H

#include <stdint.h>
#include <stddef.h>

/* ── PE Format Constants ───────────────────────────────────── */

#define PE_MAGIC       0x00004550  /* "PE\0\0" */
#define PE_MACHINE_AMD64 0x8664
#define PE_MACHINE_I386  0x014C
#define PE_OPT_MAGIC_PE32   0x010B
#define PE_OPT_MAGIC_PE32P  0x020B  /* PE32+ */

/* PE Section flags */
#define PE_MEM_EXECUTE  0x20000000
#define PE_MEM_READ     0x40000000
#define PE_MEM_WRITE    0x80000000

/* ── PE Header Structures (packed) ─────────────────────────── */

#pragma pack(push, 1)

typedef struct {
    uint16_t machine;
    uint16_t num_sections;
    uint32_t timestamp;
    uint32_t symbol_table;
    uint32_t num_symbols;
    uint16_t opt_header_size;
    uint16_t characteristics;
} pe_coff_header_t;

typedef struct {
    uint16_t magic;         /* PE_OPT_MAGIC_PE32 or PE32P */
    uint8_t  linker_major;
    uint8_t  linker_minor;
    uint32_t code_size;
    uint32_t initialized_data;
    uint32_t uninitialized_data;
    uint32_t entry_point;   /* RVA */
    uint32_t base_of_code;
    uint32_t base_of_data;  /* PE32 only, 0 in PE32+ */
} pe_opt_header_std_t;

typedef struct {
    char     name[8];
    uint32_t virtual_size;
    uint32_t virtual_addr;  /* RVA */
    uint32_t raw_size;
    uint32_t raw_offset;
    uint32_t relocations;
    uint16_t num_relocations;
    uint16_t num_linenumbers;
    uint32_t characteristics;
} pe_section_t;

#pragma pack(pop)

/* ── Win32 API Translation ─────────────────────────────────── */

/* Categories of Win32 APIs that Proton translates */
typedef enum {
    PROTON_API_KERNEL32 = 0,  /* CreateFile, ReadFile, WriteFile, etc. */
    PROTON_API_USER32    = 1,  /* CreateWindow, GetMessage, etc. */
    PROTON_API_GDI32     = 2,  /* BitBlt, TextOut, etc. */
    PROTON_API_NTDLL     = 3,  /* Low-level NT API */
    PROTON_API_D3D9      = 4,  /* DirectX 9 */
    PROTON_API_D3D11     = 5,  /* DirectX 11 */
    PROTON_API_VULKAN    = 6,  /* Vulkan (passes through to VSL) */
    PROTON_API_OPENGL    = 7,  /* OpenGL (translates to Vulkan via DXVK) */
    PROTON_API_XINPUT    = 8,  /* XInput (controller) */
    PROTON_API_WS2_32    = 9,  /* Winsock → VSL socket */
    PROTON_API_ADVAPI32  = 10, /* Registry → VSL config */
    PROTON_API_MSVCRT    = 11, /* C runtime */
    PROTON_API_STEAM     = 12, /* Steam API shim */
    PROTON_API_COUNT
} proton_api_category_t;

/* Win32 → VSL syscall mapping entry */
typedef struct {
    const char *win32_name;     /* e.g. "CreateFileW" */
    int         vsl_syscall;    /* VSL syscall number */
    uint8_t     param_count;    /* Number of parameters */
    uint8_t     has_return;     /* 1 = returns value */
    uint16_t    flags;          /* Translation flags */
} proton_api_map_t;

/* ── DLL Management ────────────────────────────────────────── */

#define PROTON_MAX_DLLS    64
#define PROTON_MAX_DLL_NAME 64

typedef enum {
    DLL_NATIVE = 0,    /* Wine/proton implementation */
    DLL_BUILTIN = 1,   /* WuBuOS built-in replacement */
    DLL_PASSTHROUGH = 2 /* VSL Linux .so */
} proton_dll_type_t;

typedef struct {
    char                name[PROTON_MAX_DLL_NAME];
    proton_dll_type_t   type;
    uint32_t            base_addr;  /* Loaded base address in VSL */
    int                 loaded;
} proton_dll_t;

/* ── Proton State ──────────────────────────────────────────── */

typedef enum {
    PROTON_OFF = 0,
    PROTON_INITIALIZING = 1,
    PROTON_READY = 2,
    PROTON_RUNNING = 3,
    PROTON_ERROR = 4
} proton_state_t;

typedef struct {
    /* Status */
    proton_state_t state;
    int            vsl_connected;  /* 1 if VSL is available for Proton */

    /* PE info */
    int            is_pe64;        /* 1 = PE64, 0 = PE32 */
    uint16_t       machine;        /* PE machine type */
    uint32_t       image_base;     /* Preferred load address */
    uint32_t       entry_point;    /* RVA of entry point */
    uint32_t       image_size;     /* Total image size */

    /* Sections */
    pe_section_t   sections[32];
    int            num_sections;

    /* DLLs */
    proton_dll_t   dlls[PROTON_MAX_DLLS];
    int            num_dlls;

    /* API translation table */
    proton_api_map_t *api_table;
    int               api_count;

    /* Runtime stats */
    uint64_t pe_loaded;      /* PE binaries loaded */
    uint64_t api_translated; /* API calls translated */
    uint64_t dll_resolved;   /* DLLs resolved */
} wubu_proton_t;

/* ── Lifecycle ─────────────────────────────────────────────── */

/* Initialize Proton layer. Requires VSL to be initialized. */
int  wubu_proton_init(wubu_proton_t *p);

/* Shutdown Proton layer */
void wubu_proton_shutdown(wubu_proton_t *p);

/* ── PE Validation ─────────────────────────────────────────── */

/* Validate a PE binary. Returns 0 if valid PE, -1 if not.
 * Sets p->is_pe64, machine, entry_point, etc. */
int  wubu_proton_validate_pe(wubu_proton_t *p, const uint8_t *data, size_t size);

/* Extract PE headers from validated binary.
 * Returns number of sections, or -1 on error. */
int  wubu_proton_parse_pe(wubu_proton_t *p, const uint8_t *data, size_t size);

/* Check if data looks like a PE (quick check, no full validation) */
int  wubu_proton_is_pe(const uint8_t *data, size_t size);

/* ── PE Loading ────────────────────────────────────────────── */

/* Map PE sections into VSL process memory.
 * Returns base address, or 0 on error. */
uint32_t wubu_proton_map_sections(wubu_proton_t *p, const uint8_t *data, size_t size);

/* Get the entry point address (base + RVA) */
uint32_t wubu_proton_entry_addr(const wubu_proton_t *p);

/* ── API Translation ───────────────────────────────────────── */

/* Register a Win32 → VSL API mapping */
int  wubu_proton_register_api(wubu_proton_t *p, const proton_api_map_t *map);

/* Look up a Win32 API name and return the VSL syscall number.
 * Returns -1 if not found. */
int  wubu_proton_translate_api(wubu_proton_t *p, const char *win32_name);

/* Load the built-in API translation table (Kernel32, User32, etc.) */
int  wubu_proton_load_default_apis(wubu_proton_t *p);

/* ── DLL Management ────────────────────────────────────────── */

/* Register a DLL */
int  wubu_proton_register_dll(wubu_proton_t *p, const char *name,
                               proton_dll_type_t type);

/* Look up a DLL by name. Returns index or -1. */
int  wubu_proton_find_dll(wubu_proton_t *p, const char *name);

/* Resolve DLL dependencies (stub) */
int  wubu_proton_resolve_deps(wubu_proton_t *p);

/* ── Runtime Execution ─────────────────────────────────────── */

/* Execute a PE binary through Proton+VSL pipeline.
 * 1. Validate PE
 * 2. Map sections
 * 3. Resolve DLLs
 * 4. Create VSL process
 * 5. Jump to entry point (via VSL)
 * Returns VSL process ID, or -1 on error. */
int  wubu_proton_exec(wubu_proton_t *p, const uint8_t *data, size_t size,
                       const char *cmdline);

/* ── Query / Diagnostics ───────────────────────────────────── */

/* Is Proton ready to execute Windows binaries? */
int  wubu_proton_is_ready(const wubu_proton_t *p);

/* Get Proton state name */
const char *wubu_proton_state_name(const wubu_proton_t *p);

/* Print Proton diagnostics */
void wubu_proton_dump(const wubu_proton_t *p);

/* Stats */
uint64_t wubu_proton_pe_count(const wubu_proton_t *p);
uint64_t wubu_proton_api_count(const wubu_proton_t *p);

#endif /* WUBU_PROTON_H */
