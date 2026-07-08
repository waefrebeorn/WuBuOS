/*
 * wubu_anticheat.c  --  WuBuOS Anti-Cheat Userspace Hooks Implementation
 *
 * Cell 470: Stub implementations for anti-cheat compatibility.
 *
 * This is RESEARCH/STUB code. Real anti-cheat support requires:
 *   - Kernel modules (vendors must provide .ko for Linux)
 *   - Wine/Proton patches (upstream)
 *   - Vendor cooperation
 *
 * What we CAN do in userspace:
 *   - Hook Win32 APIs that anti-cheats use for detection
 *   - Spoof timing, hardware IDs, process lists
 *   - Provide Wine compatibility patches via Proton
 *   - Document compatibility matrix
 *
 * What we CANNOT do in hosted mode:
 *   - Load kernel drivers (.sys/.ko)
 *   - Intercept kernel syscalls
 *   - Prevent VM detection via CPUID/hypervisor bits
 */

#include "wubu_anticheat.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

/* ==================================================================
 * Anti-Cheat Database
 * ================================================================== */

static const AntiCheatInfo anticheat_db[] = {
    [AC_NONE] = {
        .type = AC_NONE,
        .name = "None",
        .wine_compatible = true,
        .notes = "No anti-cheat detected"
    },
    [AC_BATTLEYE] = {
        .type = AC_BATTLEYE,
        .name = "BattlEye",
        .dll_name = "BEClient.dll",
        .driver_name = "BEDaisy.sys",
        .has_kernel_component = true,
        .has_userspace_component = true,
        .wine_compatible = false,
        .notes = "Requires kernel module. Official Linux .ko available from BattlEye for supported games. Does not work on Wine/Proton without kernel driver."
    },
    [AC_EASYANTICHEAT] = {
        .type = AC_EASYANTICHEAT,
        .name = "EasyAntiCheat",
        .dll_name = "EasyAntiCheat_x64.dll",
        .driver_name = "easyanticheat.sys",
        .has_kernel_component = true,
        .has_userspace_component = true,
        .wine_compatible = true,
        .notes = "Works on Wine/Proton 8.0+ with EAC runtime. Many games work out of the box. Kernel module optional for some games."
    },
    [AC_VANGUARD] = {
        .type = AC_VANGUARD,
        .name = "Riot Vanguard",
        .dll_name = "vgk.sys",
        .driver_name = "vgk.sys",
        .has_kernel_component = true,
        .has_userspace_component = true,
        .wine_compatible = false,
        .notes = "Requires kernel driver at boot. Cannot work on Wine/Proton. Riot has not released Linux kernel module."
    },
    [AC_PUNKBUSTER] = {
        .type = AC_PUNKBUSTER,
        .name = "PunkBuster",
        .dll_name = "PnkbstrA.dll",
        .driver_name = "PunkBuster.sys",
        .has_kernel_component = true,
        .has_userspace_component = true,
        .wine_compatible = false,
        .notes = "Deprecated. Even Balance no longer updates. Some legacy servers still use it."
    },
    [AC_FAIRFIGHT] = {
        .type = AC_FAIRFIGHT,
        .name = "FairFight",
        .dll_name = "FairFight.dll",
        .driver_name = "",
        .has_kernel_component = false,
        .has_userspace_component = true,
        .wine_compatible = true,
        .notes = "Server-side statistical analysis. No client kernel component. Works on Wine/Proton."
    },
    [AC_NPROTECT] = {
        .type = AC_NPROTECT,
        .name = "nProtect GameGuard",
        .dll_name = "npggNT.des",
        .driver_name = "npggNT.sys",
        .has_kernel_component = true,
        .has_userspace_component = true,
        .wine_compatible = false,
        .notes = "Kernel driver required. Used in some Korean MMOs."
    },
    [AC_XIGNCODE] = {
        .type = AC_XIGNCODE,
        .name = "XIGNCODE3",
        .dll_name = "x3.xem",
        .driver_name = "xigncode.sys",
        .has_kernel_component = true,
        .has_userspace_component = true,
        .wine_compatible = false,
        .notes = "Kernel driver required. Used in several Asian MMOs (Black Desert, etc.)."
    },
    [AC_VMPROTECT] = {
        .type = AC_VMPROTECT,
        .name = "VMProtect",
        .dll_name = "VMProtectSDK32.dll",
        .driver_name = "",
        .has_kernel_component = false,
        .has_userspace_component = true,
        .wine_compatible = true,
        .notes = "Pure userspace software protection. Works on Wine/Proton."
    },
    [AC_DENUVO] = {
        .type = AC_DENUVO,
        .name = "Denuvo Anti-Tamper",
        .dll_name = "denuvo.dll",
        .driver_name = "",
        .has_kernel_component = false,
        .has_userspace_component = true,
        .wine_compatible = true,
        .notes = "DRM/anti-tamper. Works on Proton. No kernel component."
    }
};

#define AC_DB_COUNT (sizeof(anticheat_db) / sizeof(anticheat_db[0]))

/* ==================================================================
 * Anti-Cheat Detection
 * ================================================================== */

const AntiCheatInfo *wubu_anticheat_info(AntiCheatType type) {
    if (type < 1 || type >= AC_DB_COUNT) return NULL;
    return &anticheat_db[type];
}

int wubu_anticheat_scan_prefix(const char *prefix_path, AntiCheatType *out_types, int max) {
    if (!prefix_path || !out_types || max <= 0) return 0;

    int found = 0;
    char path[512];

    for (int i = 1; i < AC_DB_COUNT && found < max; i++) {
        if (!anticheat_db[i].dll_name[0]) continue;

        snprintf(path, sizeof(path), "%s/drive_c/windows/system32/%s", prefix_path, anticheat_db[i].dll_name);
        if (access(path, F_OK) == 0) {
            out_types[found++] = anticheat_db[i].type;
        }

        /* Also check in game directory */
        snprintf(path, sizeof(path), "%s/drive_c/Program Files/*/%s", prefix_path, anticheat_db[i].dll_name);
        /* Could glob here for real implementation */
    }

    return found;
}

/* ==================================================================
 * Userspace Hooks (Stubs)
 * ================================================================== */

static AntiCheatHookFn g_hooks[12] = {0};
static void *g_hook_ctx[12] = {0};

int wubu_anticheat_hook_register(AntiCheatHookType type, AntiCheatHookFn fn, void *ctx) {
    if (type >= 12) return -1;
    if (!fn) return -1;

    g_hooks[type] = fn;
    g_hook_ctx[type] = ctx;
    return 0;
}

int wubu_anticheat_hook_unregister(AntiCheatHookType type) {
    if (type >= 12) return -1;

    g_hooks[type] = NULL;
    g_hook_ctx[type] = NULL;
    return 0;
}

/* ==================================================================
 * Wine Integration
 * ================================================================== */

static uint32_t g_ac_wine_flags = 0;

int wubu_anticheat_wine_config(uint32_t flags) {
    g_ac_wine_flags = flags;

    /* Apply Wine configuration via environment variables */
    if (flags & WINE_AC_DISABLE_DEBUGGER_DETECTION) {
        setenv("WINEDEBUG", "+anti-debug", 1);  /* Not real, just example */
    }
    if (flags & WINE_AC_DISABLE_TIMING_CHECKS) {
        setenv("WINE_TIMING_SPOOF", "1", 1);
    }
    if (flags & WINE_AC_SPOOF_HARDWARE_IDS) {
        setenv("WINE_HWID_SPOOF", "1", 1);
    }

    return 0;
}

bool wubu_anticheat_check_compatibility(AntiCheatType type) {
    const AntiCheatInfo *info = wubu_anticheat_info(type);
    return info ? info->wine_compatible : false;
}

/* ==================================================================
 * Kernel-Level Stubs (Bare Metal Only)
 * ================================================================== */

int wubu_anticheat_kernel_load(const char *driver_path, const char *device_name) {
    (void)driver_path; (void)device_name;
    /* On bare metal WuBuOS, this would use init_module() or insmod */
    fprintf(stderr, "wubu_anticheat: kernel_load stub - driver=%s device=%s\n", driver_path, device_name);
    return -1;  /* Not implemented in hosted mode */
}

int wubu_anticheat_kernel_unload(const char *device_name) {
    (void)device_name;
    fprintf(stderr, "wubu_anticheat: kernel_unload stub - device=%s\n", device_name);
    return -1;
}

bool wubu_anticheat_kernel_loaded(const char *device_name) {
    (void)device_name;
    return false;
}

/* ==================================================================
 * Proton Integration
 * ================================================================== */

static AntiCheatProtonConfig recommended_configs[] = {
    [AC_BATTLEYE] = {
        .type = AC_BATTLEYE,
        .enable_proton_hook = false,
        .disable_driver_check = false,
        .spoof_timing = false,
        .sandbox_fs = false,
        .allow_debugger = false,
    },
    [AC_EASYANTICHEAT] = {
        .type = AC_EASYANTICHEAT,
        .enable_proton_hook = true,
        .disable_driver_check = true,
        .spoof_timing = false,
        .sandbox_fs = false,
        .allow_debugger = false,
    },
    [AC_VANGUARD] = {
        .type = AC_VANGUARD,
        .enable_proton_hook = false,
        .disable_driver_check = false,
        .spoof_timing = false,
        .sandbox_fs = false,
        .allow_debugger = false,
    },
    [AC_FAIRFIGHT] = {
        .type = AC_FAIRFIGHT,
        .enable_proton_hook = true,
        .disable_driver_check = true,
        .spoof_timing = false,
        .sandbox_fs = false,
        .allow_debugger = true,
    },
    [AC_VMPROTECT] = {
        .type = AC_VMPROTECT,
        .enable_proton_hook = true,
        .disable_driver_check = true,
        .spoof_timing = true,
        .sandbox_fs = true,
        .allow_debugger = true,
    },
    [AC_DENUVO] = {
        .type = AC_DENUVO,
        .enable_proton_hook = true,
        .disable_driver_check = true,
        .spoof_timing = false,
        .sandbox_fs = false,
        .allow_debugger = true,
    },
};

int wubu_anticheat_proton_config(AntiCheatType type, const AntiCheatProtonConfig *config) {
    if (type <= 0 || type >= (int)(sizeof(recommended_configs) / sizeof(recommended_configs[0])))
        return -1;
    if (!config) return -2;

    /* Store custom config, overriding the recommended defaults.
       Other subsystems (Proton launcher, wubu_bottles) can query this
       via wubu_anticheat_recommended_config() — if a custom config has
       been set, the recommended function returns the override. */

    AntiCheatProtonConfig *dst = (AntiCheatProtonConfig*)&recommended_configs[type];
    dst->enable_proton_hook   = config->enable_proton_hook;
    dst->disable_driver_check = config->disable_driver_check;
    dst->spoof_timing         = config->spoof_timing;
    dst->sandbox_fs           = config->sandbox_fs;
    dst->allow_debugger       = config->allow_debugger;

    return 0;
}

const AntiCheatProtonConfig *wubu_anticheat_recommended_config(AntiCheatType type) {
    if (type <= 0 || type >= (int)(sizeof(recommended_configs) / sizeof(recommended_configs[0]))) {
        return NULL;
    }
    return &recommended_configs[type];
}
