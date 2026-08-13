/*
 * wubu_gpu_icd.c -- Vulkan ICD selection (extracted from wubu_hw_detect.c).
 *
 * WSL2 reaches the real GPU via the Dozen (dzn) driver:
 *   Vulkan -> dzn ICD -> D3D12 -> /dev/dxg -> GPU
 * Without dzn, only llvmpipe (software Vulkan) is available.
 *
 * On bare metal NVIDIA, nvidia_icd.json gives direct Vulkan.
 * The ICD chain always appends lvp_icd.json (llvmpipe) as the fallback
 * so the loader always enumerates at least one device.
 */
#include "wubu_hw_detect.h"
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

/* ---- W4/W5/W6/W7: Vulkan ICD selection ----
 *
 * On WSL2, the real GPU is only reachable through the Dozen (dzn) driver:
 *   Vulkan -> dzn ICD -> D3D12 -> /dev/dxg -> GPU
 * Without dzn, only llvmpipe (software Vulkan) is available.
 *
 * On bare metal NVIDIA, the nvidia_icd.json provides direct Vulkan access.
 *
 * The ICD chain always appends lvp_icd.json (llvmpipe) as the last fallback
 * so the Vulkan loader always enumerates at least one device. */

#ifdef _GNU_SOURCE

/* Check if a file exists at the given path */
/* Check if a file exists at the given path. Non-static: also used by
 * wubu_hw_detect.c's summary (wubu_hw_summary). */
int wubu_file_exists(const char *path)
{
    return access(path, R_OK) == 0;
}

/* Check if dzn (Dozen) driver JSON exists in the standard Vulkan ICD dir.
 * The dzn ICD is: /usr/share/vulkan/icd.d/microsoft_dzn_icd.x86_64.json */
int wubu_hw_has_dzn(void)
{
    /* Common dzn ICD filenames across distributions */
    const char *candidates[] = {
        "/usr/share/vulkan/icd.d/microsoft_dzn_icd.x86_64.json",
        "/usr/share/vulkan/icd.d/microsoft_dzn_icd.json",
        "/usr/share/vulkan/icd.d/dzn_icd.x86_64.json",
        "/usr/share/vulkan/icd.d/dzn_icd.json",
        "/usr/share/vulkan/icd.d/99-bifrost-Dzn.json",
        NULL
    };
    for (int i = 0; candidates[i]; i++) {
        if (wubu_file_exists(candidates[i])) return 1;
    }
    /* Check if the Vulkan loader has a "microsoft" entry in its ICD search */
    /* Also check the lib path directly */
    if (wubu_file_exists("/usr/lib/x86_64-linux-gnu/libvulkan_dzn.so") ||
        wubu_file_exists("/usr/lib/x86_64-linux-gnu/libvulkan_dzn.so.1")) {
        return 1;
    }
    return 0;
}

/* Check if NVIDIA Vulkan ICD JSON exists (bare metal). */
int wubu_hw_has_nvidia_icd(void)
{
    const char *candidates[] = {
        "/usr/share/vulkan/icd.d/nvidia_icd.json",
        "/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json",
        "/etc/vulkan/icd.d/nvidia_icd.json",
        NULL
    };
    for (int i = 0; candidates[i]; i++) {
        if (wubu_file_exists(candidates[i])) return 1;
    }
    if (wubu_file_exists("/usr/lib/x86_64-linux-gnu/libvulkan_nvidia.so") ||
        wubu_file_exists("/usr/lib/x86_64-linux-gnu/libvulkan_nvidia.so.1")) {
        return 1;
    }
    return 0;
}

#else

/* Bare-metal kernel libc stubs -- no filesystem access.
 * The kernel build uses its own PCI-based detection instead. */
int wubu_hw_has_dzn(void)        { return 0; }
int wubu_hw_has_nvidia_icd(void) { return wubu_hw_gpu_path()[0] && strstr(wubu_hw_gpu_path(), "/dev/nvidia0"); }

#endif

/* W4: return the preferred Vulkan ICD JSON for the detected platform.
 * WSL2 + dzn: returns the dzn ICD path.
 * Bare metal NVIDIA: returns nvidia ICD path.
 * Fallback: returns NULL (caller should use llvmpipe/lvp_icd.json). */
const char *wubu_hw_vulkan_icd(void)
{
#ifdef _GNU_SOURCE
    static char icd_path[256] = "";
    if (icd_path[0]) return icd_path;  /* cached */

    if (wubu_hw_is_wsl()) {
        /* WSL2: prefer dzn (Vulkan -> D3D12 -> /dev/dxg -> GPU) */
        const char *candidates[] = {
            "/usr/share/vulkan/icd.d/microsoft_dzn_icd.x86_64.json",
            "/usr/share/vulkan/icd.d/microsoft_dzn_icd.json",
            "/usr/share/vulkan/icd.d/dzn_icd.x86_64.json",
            "/usr/share/vulkan/icd.d/dzn_icd.json",
            NULL
        };
        for (int i = 0; candidates[i]; i++) {
            if (wubu_file_exists(candidates[i])) {
                strcpy(icd_path, candidates[i]);
                return icd_path;
            }
        }
        /* No dzn — fall through to llvmpipe (returned as NULL) */
    } else {
        /* Bare metal: check for GPU vendor ICD. The PCI scan in
         * wubu_hw_detect() recorded wubu_hw_gpu_vendor(). RADV handles AMD,
         * ANV handles Intel, and the NVIDIA loader handles NVIDIA.
         * RDNA4 (Navi44/48) prefers AMDVLK over RADV for full Vulkan 1.4. */
        const char *candidates[][4] = {
            { "/usr/share/vulkan/icd.d/nvidia_icd.json",
              "/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json", NULL },  /* 0x10DE */
            { NULL, NULL, NULL, NULL },  /* 0x1002 AMD: filled below */
            { "/usr/share/vulkan/icd.d/intel_icd.json",
              "/usr/share/vulkan/icd.d/intel_icd.x86_64.json", NULL },  /* 0x8086 */
        };
        int vendor_idx = -1;
        if (wubu_hw_gpu_vendor() == 0x10DE) vendor_idx = 0;
        else if (wubu_hw_gpu_vendor() == 0x1002) vendor_idx = 1;
        else if (wubu_hw_gpu_vendor() == 0x8086) vendor_idx = 2;

        /* AMD: prefer AMDVLK on RDNA4, else RADV. */
        if (vendor_idx == 1) {
            if (wubu_hw_needs_amdvlk()) {
                candidates[1][0] = "/usr/share/vulkan/icd.d/amdvlk_icd.x86_64.json";
                candidates[1][1] = "/usr/share/vulkan/icd.d/amdvlk_icd.json";
            } else {
                candidates[1][0] = "/usr/share/vulkan/icd.d/radeon_icd.json";
                candidates[1][1] = "/usr/share/vulkan/icd.d/radeon_icd.x86_64.json";
            }
        }

        if (vendor_idx >= 0) {
            for (int i = 0; candidates[vendor_idx][i]; i++) {
                if (wubu_file_exists(candidates[vendor_idx][i])) {
                    strcpy(icd_path, candidates[vendor_idx][i]);
                    return icd_path;
                }
            }
        }
        /* GCN1/2 (Vega/SI/CIK) needs amdgpu KMD, not radeon. The kernel
         * module param amdgpu.si_support=1 amdgpu.cik_support=1 must be
         * set at boot (wubu_hw_amdgpu_params()); see W3c. That's a kernel
         * cmdline fix, not a userspace ICD fix. */
    }
    return NULL;  /* no hardware ICD found → caller should use llvmpipe */
#else
    /* Bare-metal kernel: no filesystem */
    if (wubu_hw_gpu_path()[0] && strstr(wubu_hw_gpu_path(), "/dev/nvidia0")) {
        return "/usr/share/vulkan/icd.d/nvidia_icd.json";
    }
    return NULL;
#endif
}

/* W5: build the full ICD chain as a ':'-separated string.
 * Preferred ICD first, llvmpipe (lvp_icd.json) always last as fallback. */
char *wubu_hw_vulkan_icd_chain(void)
{
#ifdef _GNU_SOURCE
    char chain[512] = "";
    const char *primary = wubu_hw_vulkan_icd();
    if (primary) {
        snprintf(chain, sizeof(chain), "%s:", primary);
    }
    /* Always append llvmpipe (lvp_icd.json) as the fallback */
    if (wubu_file_exists("/usr/share/vulkan/icd.d/lvp_icd.json")) {
        strcat(chain, "/usr/share/vulkan/icd.d/lvp_icd.json");
    } else if (wubu_file_exists("/usr/share/vulkan/icd.d/lvp_icd.x86_64.json")) {
        strcat(chain, "/usr/share/vulkan/icd.d/lvp_icd.x86_64.json");
    }
    /* Note: gfxstream_vk_icd.json is NOT included -- it fails on WSL2 with
     * "Failed to detect any valid GPUs". The correct WSL2 path is dzn
     * (Vulkan->D3D12->/dev/dxg), which is already the primary above. */
    if (chain[0] == '\0') {
        /* No ICDs found at all — only llvmpipe if available */
        return strdup("");
    }
    return strdup(chain);
#else
    /* Kernel libc stub */
    const char *primary = wubu_hw_vulkan_icd();
    if (primary) {
        char *combined = malloc(512);
        if (combined) {
            snprintf(combined, 512, "%s:/usr/share/vulkan/icd.d/lvp_icd.json", primary);
        }
        return combined;
    }
    return strdup("");
#endif
}