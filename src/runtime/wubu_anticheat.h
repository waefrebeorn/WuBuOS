/*
 * wubu_anticheat.h  --  WuBuOS Anti-Cheat Userspace Hooks
 *
 * Cell 470: Research and stubs for anti-cheat compatibility.
 *
 * Anti-cheat systems (BattlEye, EasyAntiCheat, Vanguard, PunkBuster, etc.)
 * operate at kernel level on Windows. On Linux/Proton, they require:
 *   1. Kernel modules (not feasible for WuBuOS hosted mode)
 *   2. Userspace hooks + wine emulation (what we can do)
 *   3. VM-based detection (what games do)
 *
 * This module provides:
 *   - Stubs for common anti-cheat syscalls/IPCs
 *   - Wine hooks for detection evasion
 *   - Documentation of known anti-cheat behaviors
 *   - Framework for future kernel-level support on bare metal
 *
 * References:
 *   - wine-anticheat (github.com/vitalif/wine-anticheat)
 *   - proton-anticheat discussions (Valve)
 *   - kernel-level anti-cheat on Linux (BattlEye/EAC kernel modules)
 *
 * IMPORTANT: This is RESEARCH/STUB code. Real anti-cheat support
 * requires either kernel modules (on bare metal) or cooperation
 * from anti-cheat vendors (who make Linux builds).
 */

#ifndef WUBU_ANTICHEAT_H
#define WUBU_ANTICHEAT_H

#include <stdint.h>
#include <stdbool.h>

/* -- Anti-Cheat Types --------------------------------------------- */

typedef enum {
    AC_NONE = 0,
    AC_BATTLEYE = 1,       /* BattlEye */
    AC_EASYANTICHEAT = 2,  /* EasyAntiCheat (EAC) */
    AC_VANGUARD = 3,       /* Riot Vanguard */
    AC_PUNKBUSTER = 4,     /* PunkBuster */
    AC_FAIRFIGHT = 5,      /* FairFight */
    AC_NPROTECT = 6,       /* nProtect GameGuard */
    AC_XIGNCODE = 7,       /* XIGNCODE3 */
    AC_VMPROTECT = 8,      /* VMProtect */
    AC_DENUVO = 9,         /* Denuvo Anti-Tamper */
    AC_CUSTOM = 0xFF
} AntiCheatType;

/* -- Anti-Cheat Detection ----------------------------------------- */

typedef struct {
    AntiCheatType type;
    char name[64];
    char dll_name[64];          /* Main anti-cheat DLL */
    char driver_name[64];       /* Kernel driver name (if any) */
    bool has_kernel_component;
    bool has_userspace_component;
    bool wine_compatible;       /* Known to work on Wine/Proton */
    char notes[512];
} AntiCheatInfo;

/* Get info about a known anti-cheat */
const AntiCheatInfo *wubu_anticheat_info(AntiCheatType type);

/* Scan a Wine prefix for anti-cheat DLLs */
int wubu_anticheat_scan_prefix(const char *prefix_path, AntiCheatType *out_types, int max);

/* -- Userspace Hooks ---------------------------------------------- */

/* Hook types we can intercept in userspace */
typedef enum {
    AC_HOOK_FILE_ACCESS = 0,      /* File open/read/write */
    AC_HOOK_PROCESS_ENUM = 1,     /* Process enumeration (CreateToolhelp32Snapshot) */
    AC_HOOK_MODULE_ENUM = 2,      /* Module enumeration (EnumProcessModules) */
    AC_HOOK_MEMORY_READ = 3,      /* Memory reading (ReadProcessMemory) */
    AC_HOOK_MEMORY_WRITE = 4,     /* Memory writing (WriteProcessMemory) */
    AC_HOOK_THREAD_CREATE = 5,    /* Thread creation */
    AC_HOOK_DEBUGGER_DETECT = 6,  /* Debugger detection (IsDebuggerPresent, NtQueryInformationProcess) */
    AC_HOOK_TIMING = 7,           /* Timing checks (RDTSC, QueryPerformanceCounter) */
    AC_HOOK_HARDWARE_ID = 8,      /* Hardware ID queries (WMI, SMBIOS) */
    AC_HOOK_NETWORK = 9,          /* Network traffic inspection */
    AC_HOOK_INPUT = 10,           /* Input injection detection */
    AC_HOOK_VM_DETECT = 11,       /* Virtual machine detection */
} AntiCheatHookType;

/* Hook callback signature */
typedef int (*AntiCheatHookFn)(void *ctx, const char *dll, const char *func, void *args);

/* Register a hook for anti-cheat evasion */
int wubu_anticheat_hook_register(AntiCheatHookType type, AntiCheatHookFn fn, void *ctx);

/* Unregister a hook */
int wubu_anticheat_hook_unregister(AntiCheatHookType type);

/* -- Wine Integration --------------------------------------------- */

/* Wine anti-cheat compatibility flags */
#define WINE_AC_DISABLE_DEBUGGER_DETECTION   (1 << 0)
#define WINE_AC_DISABLE_TIMING_CHECKS        (1 << 1)
#define WINE_AC_SPOOF_HARDWARE_IDS           (1 << 2)
#define WINE_AC_HOOK_NT_APIS                 (1 << 3)
#define WINE_AC_EMULATE_KERNEL_CALLS         (1 << 4)
#define WINE_AC_BLOCK_DRIVER_LOAD            (1 << 5)
#define WINE_AC_SANDBOX_SYSCALLS             (1 << 6)

/* Configure Wine for anti-cheat compatibility */
int wubu_anticheat_wine_config(uint32_t flags);

/* Check if a specific anti-cheat is likely to work */
bool wubu_anticheat_check_compatibility(AntiCheatType type);

/* -- Kernel-Level Stubs (for bare metal) ------------------------- */

/* On bare metal WuBuOS, we could load kernel modules for anti-cheat.
 * These are stubs for future implementation. */

typedef struct {
    const char *driver_path;      /* Path to .ko or .sys driver */
    const char *device_name;      /* /dev/ name */
    int major, minor;             /* Device numbers */
    bool loaded;
} AntiCheatKernelModule;

/* Load anti-cheat kernel module (bare metal only) */
int wubu_anticheat_kernel_load(const char *driver_path, const char *device_name);

/* Unload anti-cheat kernel module */
int wubu_anticheat_kernel_unload(const char *device_name);

/* Check if kernel module is loaded */
bool wubu_anticheat_kernel_loaded(const char *device_name);

/* -- Proton Integration ------------------------------------------- */

typedef struct {
    AntiCheatType type;
    bool enable_proton_hook;      /* Enable Proton's built-in hooks */
    bool disable_driver_check;    /* Skip kernel driver check */
    bool spoof_timing;            /* Spoof RDTSC/QPC */
    bool sandbox_fs;              /* Sandbox filesystem access */
    bool allow_debugger;          /* Allow debugger attachment */
} AntiCheatProtonConfig;

/* Configure Proton/Wine for specific anti-cheat */
int wubu_anticheat_proton_config(AntiCheatType type, const AntiCheatProtonConfig *config);

/* Get recommended config for an anti-cheat */
const AntiCheatProtonConfig *wubu_anticheat_recommended_config(AntiCheatType type);

/* -- Known Anti-Cheat Compatibility ------------------------------- */

/* Compatibility matrix (as of 2024):
 *
 * | Anti-Cheat          | Wine/Proton | Kernel Needed | Notes                                    |
 * |---------------------|-------------|---------------|------------------------------------------|
 * | BattlEye            | ❌          | Yes           | Official Linux module available          |
 * | EasyAntiCheat       | ✅*         | Optional      | *Works with Wine 8.0+ + EAC runtime      |
 * | Vanguard            | ❌          | Yes           | Requires Vanguard kernel driver          |
 * | PunkBuster          | ❌          | Yes           | Deprecated                               |
 * | FairFight           | ✅          | No            | Server-side                              |
 * | nProtect GameGuard  | ❌          | Yes           | Rarely used                              |
 * | XIGNCODE3           | ❌          | Yes           | Kernel driver required                   |
 * | VMProtect           | ✅          | No            | Pure userspace                           |
 * | Denuvo              | ✅          | No            | Works on Proton                          |
 *
 * For kernel-required anti-cheats on WuBuOS:
 *   - Hosted mode: NOT SUPPORTED (no kernel module loading)
 *   - Bare metal: Could load vendor .ko modules if provided
 *   - Container: NOT SUPPORTED (containers share host kernel)
 */

#endif /* WUBU_ANTICHEAT_H */
