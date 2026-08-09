/*
 * wubu_image_names.c -- arch/OS name registry (the Revolver Doctrine).
 *
 * Split out of wubu_image.c so the name tables are a self-contained,
 * linkable registry (no container/tar deps). The enum-name tables seed
 * the registry; a new arch or OS persona registers its name at runtime.
 * wubu_image.c includes the lookups; tests link just this file.
 */
#include "wubu_image.h"

#define IMAGE_NAME_MAX 8
static const char *g_arch_names[IMAGE_NAME_MAX] = {"x86_64", "aarch64", "riscv64", "wasm"};
static const char *g_os_names[IMAGE_NAME_MAX]   = {"linux", "windows", "macos", "native"};

int wubu_arch_name_register(WubuArch arch, const char *name)
{
    if (arch < 0 || arch >= IMAGE_NAME_MAX || !name) return -1;
    g_arch_names[arch] = name;
    return 0;
}

int wubu_os_name_register(WubuOS os, const char *name)
{
    if (os < 0 || os >= IMAGE_NAME_MAX || !name) return -1;
    g_os_names[os] = name;
    return 0;
}

const char *wubu_arch_name(WubuArch arch) {
    if (arch >= 0 && arch < IMAGE_NAME_MAX && g_arch_names[arch])
        return g_arch_names[arch];
    return "unknown";
}

const char *wubu_os_name(WubuOS os) {
    if (os >= 0 && os < IMAGE_NAME_MAX && g_os_names[os])
        return g_os_names[os];
    return "unknown";
}
