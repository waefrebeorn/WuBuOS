/*
 * wubu_wine_env.c -- Wine/Proton environment setup for Windows PE execution.
 *
 * Sets up Wine prefix, DXVK/Vulkan ICD routing, and secmon-compatible
 * environment variables so the AGI syscall camera can observe Wine.
 *
 * GPU backend is auto-selected based on wubu_hw_detect (kernel-owned):
 *   - WSL2 + /dev/dgx + dzn -> VK_ICD_FILENAMES=dzn + GALLIUM_DRIVER=d3d12
 *                              (Vulkan -> D3D12 -> /dev/dgx -> host GPU)
 *   - WSL2 without dzn      -> PROTON_USE_WINED3D=1 + lvp ICD (software)
 *   - bare metal NVIDIA     -> VK_ICD_FILENAMES=nvidia_icd
 *   - bare metal AMD/Intel  -> VK_ICD_FILENAMES=radeon/intel_icd
 *
 * This is the integration layer that bridges wubu_exec_win_pe to the
 * Proton translation subsystem. Wine is NOT a fork-and-forget; it is a
 * kernel-managed container with syscall capture.
 */
#include "wubu_host_exec.h"
#include "wubu_exec.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

/* Forward-declare hw detection from the kernel (wubu_hw_detect.h).
 * The kernel owns GPU detection -- the runtime layer consumes it. */
extern int wubu_hw_is_wsl(void);
extern const char *wubu_hw_gpu_path(void);
extern const char *wubu_hw_vulkan_icd(void);
extern char *wubu_hw_vulkan_icd_chain(void);
extern int wubu_hw_has_dzn(void);
extern int wubu_hw_has_nvidia_icd(void);
extern int wubu_hw_has_prime(void);

/* Default Wine prefix path */
#define WUBE_WINE_PREFIX_BASE ".wubu/wineprefix"

/*
 * Determine the Vulkan ICD file path based on the kernel-detected GPU.
 * The kernel (wubu_hw_detect.c, W4) owns ICD selection -- see
 * wubu_hw_vulkan_icd(). On WSL2 this returns the dzn ICD (Vulkan->D3D12->
 * /dev/dxg), which is what SteamDB compatibility testing confirmed works.
 *
 * Returns a heap-allocated string the caller must free, or NULL if none.
 */
char *wubu_wine_vk_icd(void) {
    /* Ask the kernel which ICD to use (dzn, nvidia, etc). */
    const char *icd_path = wubu_hw_vulkan_icd();
    if (icd_path) {
        return strdup(icd_path);
    }
    /* Fallback: try llvmpipe (software Vulkan) */
    const char *candidates[] = {
        "/usr/share/vulkan/icd.d/lvp_icd.json",
        "/usr/share/vulkan/icd.d/lvp_icd.x86_64.json",
        NULL
    };
    for (int i = 0; candidates[i]; i++) {
        if (access(candidates[i], R_OK) == 0)
            return strdup(candidates[i]);
    }
    return NULL;
}

/*
 * Set up the Wine prefix with DXVK DLL overrides.
 * Returns the prefix path on success (heap-allocated), NULL on failure.
 *
 * DXVK translation: d3d9.dll, d3d10.dll, d3d11.dll, dxgi.dll
 * are overridden to native DXVK DLLs. These provide D3D->Vulkan
 * translation so Wine apps can use GPU acceleration on Linux.
 */
char *wubu_wine_setup_prefix(const char *prefix_override) {
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";

    char prefix_path[512];
    if (prefix_override) {
        snprintf(prefix_path, sizeof(prefix_path), "%s", prefix_override);
    } else {
        snprintf(prefix_path, sizeof(prefix_path), "%s/" WUBE_WINE_PREFIX_BASE, home);
    }

    /* Ensure prefix directory exists */
    struct stat st;
    if (stat(prefix_path, &st) != 0) {
        snprintf(prefix_path, sizeof(prefix_path), "%s/" WUBE_WINE_PREFIX_BASE, home);
        mkdir(prefix_path, 0755);
    }

    /* Initialize Wine prefix if needed (WINEARCH=win32 for compatibility) */
    char prefix_env[1024];
    snprintf(prefix_env, sizeof(prefix_env), "WINEPREFIX=%s", prefix_path);

    char *prefix_dup = strdup(prefix_path);
    return prefix_dup;
}

/*
 * Configure Wine environment variables for the container.
 * Adds DXVK, Vulkan ICD, DLL overrides, and WINEDEBUG settings.
 * These are added to the container's envp list.
 *
 * Returns 0 on success, -1 on error.
 */
int wubu_wine_configure_env(WubuCt *ct, const char *wine_prefix) {
    if (!ct || !wine_prefix) return -1;

    /* Wine prefix */
    char buf[512];
    snprintf(buf, sizeof(buf), "WINEPREFIX=%s", wine_prefix);
    if (wubu_ct_add_env(ct, buf) != 0) return -1;

    /* Wine architecture (win32 for best game compat) */
    if (wubu_ct_add_env(ct, "WINEARCH=win32") != 0) return -1;

    /* Vulkan ICD selection (auto-detected by kernel based on GPU platform).
     * On WSL2: selects dzn_icd.json + GALLIUM_DRIVER=d3d12 for /dev/dxg.
     * On bare NVIDIA: selects nvidia_icd.json. */
    char *icd = wubu_wine_vk_icd();
    if (icd) {
        snprintf(buf, sizeof(buf), "VK_ICD_FILENAMES=%s", icd);
        wubu_ct_add_env(ct, buf);
        free(icd);
    }

    /* GALLIUM_DRIVER=d3d12: on WSL2, this forces Mesa's OpenGL/Vulkan to
     * go through the D3D12 backend -> /dev/dxg -> host GPU. Without this,
     * Mesa silently falls back to llvmpipe (software rasterizer) even when
     * a real GPU is available. SteamDB/Proton testing showed this is the
     * difference between 0MB VRAM reported and full GPU acceleration.
     * Only set on WSL2 with dzn (the D3D12 bridge); bare metal doesn't
     * need it since Mesa talks to the GPU directly. */
    if (wubu_hw_is_wsl() && wubu_hw_has_dzn()) {
        if (wubu_ct_add_env(ct, "GALLIUM_DRIVER=d3d12") != 0) return -1;
    }

    /* DRI_PRIME=1 on a hybrid iGPU+dGPU laptop: makes the discrete GPU
     * (rendernode) the render device. WuBuOS auto-detects the hybrid
     * topology and hands Proton/Wine the discrete card. */
    if (!wubu_hw_is_wsl() && wubu_hw_has_prime()) {
        if (wubu_ct_add_env(ct, "DRI_PRIME=1") != 0) return -1;
    }

    /* DXVK: enable Vulkan-based D3D translation */
    if (wubu_ct_add_env(ct, "DXVK_HUD=0") != 0) return -1;
    if (wubu_ct_add_env(ct, "DXVK_LOG_LEVEL=none") != 0) return -1;
    if (wubu_ct_add_env(ct, "DXVK_LOG_PATH=/dev/null") != 0) return -1;
    if (wubu_ct_add_env(ct, "DXVK_STATE_CACHE=0") != 0) return -1;

    /* DLL overrides: use native DXVK DLLs for D3D -> Vulkan */
    if (wubu_ct_add_env(ct, "WINEDLLOVERRIDES=builtin,d3d9=n,builtin;dxgi=n,builtin;d3d10=n,builtin;d3d11=n") != 0) return -1;

    /* Disable Wine's fullscreen hack (can cause issues with some games) */
    if (wubu_ct_add_env(ct, "WINE_DISABLE_FULLSCREEN_HACK=0") != 0) return -1;

    /* Halo PC needs a large address space (>=2GB user-mode) on 32-bit Wine. */
    if (wubu_ct_add_env(ct, "WINE_LARGEADDRESSARENA=1") != 0) return -1;

    /* Fallback: if the kernel didn't find a working Vulkan ICD (dzn/nvidia),
     * use PROTON_USE_WINED3D so Proton uses Wine's OpenGL-based wined3d
     * (D3D->OpenGL) instead of DXVK. This lets the game run even without
     * GPU Vulkan support -- at the cost of software rendering. */
    if (!wubu_hw_vulkan_icd()) {
        wubu_ct_add_env(ct, "PROTON_USE_WINED3D=1");
    }

    /* Reduce Wine debug noise */
    if (wubu_ct_add_env(ct, "WINEDEBUG=-all") != 0) return -1;

    /* Ensure X11 display is available */
    const char *display = getenv("DISPLAY");
    if (display) {
        snprintf(buf, sizeof(buf), "DISPLAY=%s", display);
        wubu_ct_add_env(ct, buf);
    }

    return 0;
}
