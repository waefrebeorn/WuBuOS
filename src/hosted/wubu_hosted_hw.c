/*
 * wubu_hosted_hw.c -- the HOSTED hardware backend (the OS's sense of the
 * machine it is running on as a process).
 *
 * The bare-metal kernel senses hardware through its own drivers (PCI MMIO,
 * ACPI, the DRM/KMS path). The hosted binary has no bus: it runs on a normal
 * Linux host, so the same questions -- is there a battery, is the wifi up,
 * how hot is the CPU, what is on the display -- are answered from the host's
 * own /sys and /proc trees. This module is the answer layer for wubu_world.c
 * (the AGI's world snapshot) and the GUI's world tray.
 *
 * Every getter is READ-ONLY and never fails loudly: a missing sensor is a
 * "not present" answer, not an error. That matches the kernel drivers'
 * contract (present/link/percent), so wubu_world.c links against either
 * backend unchanged.
 *
 * C11, self-contained, no libm.
 */
#include "../egojar.h"
#include "../egojar_net.h"
#include "../kernel/wubu_drv.h"
#include "../kernel/wubu_drv_nvme.h"
#include "../kernel/wubu_drv_net.h"
#include "../kernel/wubu_drv_gpu.h"
#include "../kernel/wubu_drv_battery.h"
#include "../kernel/wubu_drv_sd.h"
#include "../kernel/wubu_drv_thermal.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

/* ---- small /sys + /proc readers ------------------------------------ */

/* Read a whole sysfs/procfs attribute (trimmed) into buf. 0 on success. */
static int read_attr(const char *path, char *buf, size_t cap)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    if (!fgets(buf, (int)cap, f)) { fclose(f); return -1; }
    fclose(f);
    buf[strcspn(buf, "\r\n")] = '\0';
    return 0;
}

static int read_int_attr(const char *path, int *out)
{
    char b[64];
    if (read_attr(path, b, sizeof(b)) != 0) return -1;
    char *end = NULL;
    long v = strtol(b, &end, 10);
    if (end == b) return -1;
    *out = (int)v;
    return 0;
}

static int read_u64_attr(const char *path, uint64_t *out)
{
    char b[64];
    if (read_attr(path, b, sizeof(b)) != 0) return -1;
    char *end = NULL;
    unsigned long long v = strtoull(b, &end, 10);
    if (end == b) return -1;
    *out = (uint64_t)v;
    return 0;
}

/* First entry in `dir` whose name starts with `prefix`, copied to out. */
static int first_entry(const char *dir, const char *prefix, char *out, size_t cap)
{
    DIR *d = opendir(dir);
    if (!d) return -1;
    size_t plen = strlen(prefix);
    struct dirent *e;
    int found = -1;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        if (strncmp(e->d_name, prefix, plen) == 0) {
            snprintf(out, cap, "%s", e->d_name);
            found = 0;
            break;
        }
    }
    closedir(d);
    return found;
}

static int path_exists(const char *p)
{
    return access(p, F_OK) == 0;
}

/* ---- the power (battery) ------------------------------------------- */

int wubu_battery_present(void)
{
    char name[64];
    return first_entry("/sys/class/power_supply", "BAT", name, sizeof(name)) == 0;
}

int wubu_battery_percent(void)
{
    char name[64], path[256];
    int pct = 0;
    if (first_entry("/sys/class/power_supply", "BAT", name, sizeof(name)) != 0) return 0;
    snprintf(path, sizeof(path), "/sys/class/power_supply/%s/capacity", name);
    if (read_int_attr(path, &pct) != 0) {
        /* some batteries expose charge_full/charge_now instead */
        int full = 0, now = 0;
        snprintf(path, sizeof(path), "/sys/class/power_supply/%s/charge_full", name);
        if (read_int_attr(path, &full) != 0 || full <= 0) return 0;
        snprintf(path, sizeof(path), "/sys/class/power_supply/%s/charge_now", name);
        if (read_int_attr(path, &now) != 0) return 0;
        pct = (now * 100) / full;
    }
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return pct;
}

int wubu_battery_charging(void)
{
    char name[64], path[256], status[32];
    if (first_entry("/sys/class/power_supply", "BAT", name, sizeof(name)) != 0) return 0;
    snprintf(path, sizeof(path), "/sys/class/power_supply/%s/status", name);
    if (read_attr(path, status, sizeof(status)) != 0) return 0;
    return strcmp(status, "Charging") == 0;
}

/* ---- the network --------------------------------------------------- */

/* A wifi iface: wl* (wlan/wlp/wlo). An eth iface: en or eth prefix
 * (enp/eno/eth), excluding the virtual bridges/veths a host accumulates. */
static int is_wifi_name(const char *n) { return strncmp(n, "wl", 2) == 0; }

static int is_eth_name(const char *n)
{
    if (strncmp(n, "eth", 3) == 0 || strncmp(n, "en", 2) == 0) {
        if (strncmp(n, "veth", 4) == 0) return 0;
        if (strncmp(n, "docker", 6) == 0) return 0;
        if (strncmp(n, "br-", 3) == 0) return 0;
        if (strncmp(n, "virbr", 5) == 0) return 0;
        if (strncmp(n, "tap", 3) == 0 || strncmp(n, "tun", 3) == 0) return 0;
        return 1;
    }
    return 0;
}

/* carrier == 1 means link up; operstate is the fallback for wifi. */
static int iface_link(const char *n)
{
    char path[256], b[32];
    snprintf(path, sizeof(path), "/sys/class/net/%s/carrier", n);
    int carrier = -1;
    if (read_int_attr(path, &carrier) == 0 && carrier >= 0) return carrier;
    snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", n);
    if (read_attr(path, b, sizeof(b)) == 0)
        return (strcmp(b, "up") == 0 || strcmp(b, "unknown") == 0) ? 1 : 0;
    return 0;
}

int wubu_net_wifi_present(void)
{
    DIR *d = opendir("/sys/class/net");
    if (!d) return 0;
    struct dirent *e;
    int found = 0;
    while ((e = readdir(d)) != NULL)
        if (e->d_name[0] != '.' && is_wifi_name(e->d_name)) { found = 1; break; }
    closedir(d);
    return found;
}

int wubu_net_eth_present(void)
{
    DIR *d = opendir("/sys/class/net");
    if (!d) return 0;
    struct dirent *e;
    int found = 0;
    while ((e = readdir(d)) != NULL)
        if (e->d_name[0] != '.' && is_eth_name(e->d_name)) { found = 1; break; }
    closedir(d);
    return found;
}

int wubu_net_wifi_link(void)
{
    DIR *d = opendir("/sys/class/net");
    if (!d) return 0;
    struct dirent *e;
    int link = 0;
    while ((e = readdir(d)) != NULL)
        if (e->d_name[0] != '.' && is_wifi_name(e->d_name)) { link = iface_link(e->d_name); break; }
    closedir(d);
    return link;
}

int wubu_net_eth_link(void)
{
    DIR *d = opendir("/sys/class/net");
    if (!d) return 0;
    struct dirent *e;
    int link = 0;
    while ((e = readdir(d)) != NULL)
        if (e->d_name[0] != '.' && is_eth_name(e->d_name)) { link = iface_link(e->d_name); break; }
    closedir(d);
    return link;
}

/* ---- the thermal --------------------------------------------------- */

/* Find a thermal zone whose type matches `needle` ("cpu"/"gpu"), else zone0. */
static int zone_temp_c(const char *needle)
{
    char path[256], type[128];
    int best = 0, fallback = 0;
    for (int z = 0; z < 32; z++) {
        snprintf(path, sizeof(path), "/sys/class/thermal/thermal_zone%d/type", z);
        if (read_attr(path, type, sizeof(type)) != 0) continue;
        snprintf(path, sizeof(path), "/sys/class/thermal/thermal_zone%d/temp", z);
        int millideg = 0;
        if (read_int_attr(path, &millideg) != 0) continue;
        if (fallback == 0) fallback = millideg / 1000;
        if (needle && strstr(type, needle) != NULL) { best = millideg / 1000; break; }
    }
    return best ? best : fallback;
}

int wubu_thermal_temp(int zone)
{
    if (zone == WUBU_THERMAL_GPU) return zone_temp_c("gpu");
    return zone_temp_c("cpu");
}

int wubu_thermal_fan_duty(void)
{
    /* No universal host exposure (no hwmon pwm on most laptops without
     * root). Reported as 0 = "not controlled", which is the honest answer
     * for a hosted process; the metal kernel reads the EC. */
    return 0;
}

int wubu_thermal_throttled(void)
{
    /* Same boundary: the host does not expose thermal throttling to an
     * unprivileged process. 0 = "not throttled", never a guess. */
    return 0;
}

/* ---- the storage --------------------------------------------------- */

int wubu_nvme_ready(void)
{
    char name[64];
    return first_entry("/sys/class/block", "nvme", name, sizeof(name)) == 0;
}

uint64_t wubu_nvme_nsze(void)
{
    char name[64], path[256];
    uint64_t sectors = 0;
    if (first_entry("/sys/class/block", "nvme", name, sizeof(name)) != 0) return 0;
    /* name may be the controller (nvme0) or the ns (nvme0n1): prefer a ns */
    if (strchr(name, 'n') == NULL || strstr(name, "n1") == NULL) {
        snprintf(path, sizeof(path), "/sys/class/block/%s/nvme0n1/size", name);
        /* fallthrough to the plain path below if that fails */
    }
    snprintf(path, sizeof(path), "/sys/class/block/%s/size", name);
    if (read_u64_attr(path, &sectors) != 0) return 0;
    return sectors;
}

uint32_t wubu_nvme_block_size(void)
{
    return 512; /* the NVMe logical block size on every current device */
}

int wubu_sd_card_present(void)
{
    char name[64];
    return first_entry("/sys/class/block", "mmcblk", name, sizeof(name)) == 0;
}

uint64_t wubu_sd_capacity_mb(void)
{
    char name[64], path[256];
    uint64_t sectors = 0;
    if (first_entry("/sys/class/block", "mmcblk", name, sizeof(name)) != 0) return 0;
    snprintf(path, sizeof(path), "/sys/class/block/%s/size", name);
    if (read_u64_attr(path, &sectors) != 0) return 0;
    return (sectors * 512ULL) / (1024ULL * 1024ULL);
}

/* ---- the display / GPU --------------------------------------------- */

int wubu_gpu_present_wsl(void)
{
    return path_exists("/dev/dxg") ? 1 : 0;
}

/* Pick the connector that is actually DRIVING a display: status "connected"
 * and a non-empty modes file. A box with several cards lists disconnected
 * connectors too (empty modes), so "first card*-* entry" reads 0x0. Falls
 * back to any connector with a mode, then to the first connector. */
static int drm_connector(char *name, size_t cap)
{
    char cands[16][64];
    int n = 0;
    DIR *d = opendir("/sys/class/drm");
    if (!d) return -1;
    struct dirent *e;
    while ((e = readdir(d)) != NULL && n < 16) {
        if (e->d_name[0] == '.') continue;
        if (strncmp(e->d_name, "card", 4) != 0) continue;
        if (strchr(e->d_name, '-') == NULL) continue;
        snprintf(cands[n], sizeof(cands[n]), "%s", e->d_name);
        n++;
    }
    closedir(d);
    if (n == 0) return -1;

    char fallback[64] = {0};
    for (int i = 0; i < n; i++) {
        char path[256], status[32], modes[128];
        snprintf(path, sizeof(path), "/sys/class/drm/%s/status", cands[i]);
        if (read_attr(path, status, sizeof(status)) != 0) continue;
        snprintf(path, sizeof(path), "/sys/class/drm/%s/modes", cands[i]);
        if (read_attr(path, modes, sizeof(modes)) != 0 || modes[0] == '\0') continue;
        if (fallback[0] == '\0') snprintf(fallback, sizeof(fallback), "%s", cands[i]);
        if (strcmp(status, "connected") == 0) {
            snprintf(name, cap, "%s", cands[i]);
            return 0;
        }
    }
    if (fallback[0] != '\0') {
        snprintf(name, cap, "%s", fallback);
        return 0;
    }
    snprintf(name, cap, "%s", cands[0]);
    return 0;
}

int wubu_gpu_present(void)
{
    char c[128];
    return drm_connector(c, sizeof(c)) == 0;
}

int wubu_gpu_connector(void)
{
    char c[128];
    if (drm_connector(c, sizeof(c)) != 0 || strchr(c, '-') == NULL)
        return WUBU_GPU_CONNECTOR_EDP;   /* a laptop's internal panel is the common case */
    if (strstr(c, "eDP")) return WUBU_GPU_CONNECTOR_EDP;
    if (strstr(c, "DSI")) return WUBU_GPU_CONNECTOR_DSI;
    if (strstr(c, "HDMI")) return WUBU_GPU_CONNECTOR_HDMI;
    if (strstr(c, "DP")) return WUBU_GPU_CONNECTOR_DP;
    return WUBU_GPU_CONNECTOR_EDP;
}

int wubu_gpu_width(void)
{
    char c[128], path[256], modes[512];
    if (drm_connector(c, sizeof(c)) != 0 || strchr(c, '-') == NULL) return 0;
    snprintf(path, sizeof(path), "/sys/class/drm/%s/modes", c);
    if (read_attr(path, modes, sizeof(modes)) != 0) return 0;
    int w = 0, h = 0;
    if (sscanf(modes, "%dx%d", &w, &h) != 2) return 0;
    return w;
}

int wubu_gpu_height(void)
{
    char c[128], path[256], modes[512];
    if (drm_connector(c, sizeof(c)) != 0 || strchr(c, '-') == NULL) return 0;
    snprintf(path, sizeof(path), "/sys/class/drm/%s/modes", c);
    if (read_attr(path, modes, sizeof(modes)) != 0) return 0;
    int w = 0, h = 0;
    if (sscanf(modes, "%dx%d", &w, &h) != 2) return 0;
    return h;
}

uint64_t wubu_gpu_vram_mb(void)
{
    /* amdgpu/iris expose the VRAM size in bytes; nvidia does not without
     * the proprietary proc node. Return what the host actually says. */
    char path[256], c[128];
    uint64_t bytes = 0;
    if (drm_connector(c, sizeof(c)) == 0 && strchr(c, '-') != NULL) {
        snprintf(path, sizeof(path), "/sys/class/drm/%s/device/mem_info_vram_total", c);
        if (read_u64_attr(path, &bytes) == 0 && bytes > 0)
            return bytes / (1024ULL * 1024ULL);
    }
    return 0;
}

/* ---- the device registry lookup ------------------------------------ */

/*
 * wubu_world.c asks the registry for a device by class name. The hosted
 * binary has no PCI bus, so the answer is derived from the same /sys probes
 * above: a device "exists" exactly when its sensor says it does. The returned
 * struct is a static placeholder -- callers read only the pointer's
 * null-ness (the world bridge checks `nvme != NULL`).
 */
const wubu_drv_dev_t *wubu_drv_find(const char *name)
{
    static wubu_drv_dev_t dev;
    if (!name) return NULL;

    int present = 0;
    if (strcmp(name, "nvme") == 0)         present = wubu_nvme_ready();
    else if (strcmp(name, "sd") == 0)      present = wubu_sd_card_present();
    else if (strcmp(name, "gpu") == 0)     present = wubu_gpu_present();
    else if (strcmp(name, "battery") == 0) present = wubu_battery_present();
    else if (strcmp(name, "wifi") == 0)    present = wubu_net_wifi_present();
    else if (strcmp(name, "eth") == 0)     present = wubu_net_eth_present();
    else if (strcmp(name, "thermal") == 0) present = 1;   /* the CPU zone always exists */

    if (!present) return NULL;
    memset(&dev, 0, sizeof(dev));
    dev.bound = 1;
    return &dev;
}

/* ---- the hardware-detector API (wubu_gpu_backend + wubu_wine_env) ----- */

static int   g_hw_probe_done;
static int   g_hw_wsl;
static char  g_hw_platform[16];

void wubu_hw_detect(void)
{
    if (g_hw_probe_done) return;
    g_hw_probe_done = 1;

    g_hw_wsl = path_exists("/dev/dxg") ? 1 : 0;
    if (!g_hw_wsl) {
        char b[128];
        if (read_attr("/proc/sys/kernel/osrelease", b, sizeof(b)) == 0 &&
            strstr(b, "microsoft") != NULL) g_hw_wsl = 1;
    }

    const char *plat = "bare_metal";
    if (g_hw_wsl) plat = "wsl2";
    else {
        char p[128];
        if (read_attr("/sys/class/dmi/id/product_name", p, sizeof(p)) == 0) {
            if (strstr(p, "VirtualBox")) plat = "virtualbox";
            else if (strstr(p, "VMware"))   plat = "vmware";
            else if (strstr(p, "Hyper-V"))  plat = "hyperv";
            else if (strstr(p, "KVM") || strstr(p, "QEMU")) plat = "kvm";
        }
    }
    snprintf(g_hw_platform, sizeof(g_hw_platform), "%s", plat);
}

int wubu_hw_is_wsl(void)       { wubu_hw_detect(); return g_hw_wsl; }
int wubu_hw_gpu_present(void)  { return wubu_gpu_present(); }
const char *wubu_hw_platform(void) { wubu_hw_detect(); return g_hw_platform; }

const char *wubu_hw_gpu_path(void)
{
    if (wubu_hw_is_wsl() && path_exists("/dev/dxg")) return "/dev/dxg";
    if (path_exists("/dev/dri/card0")) return "/dev/dri/card0";
    if (path_exists("/dev/nvidia0"))   return "/dev/nvidia0";
    return 0;
}

/* No NVIDIA PRIME render-offload on a stock Linux host, and no "dzn"
 * (that is a Game Deck EC class, not a host device). Both are honest 0s. */
int wubu_hw_has_prime(void) { return 0; }
int wubu_hw_has_dzn(void)   { return 0; }

int wubu_hw_has_nvidia_icd(void)
{
    static const char *const paths[] = {
        "/usr/share/vulkan/icd.d/nvidia_icd.json",
        "/etc/vulkan/icd.d/nvidia_icd.json",
        "/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json",
    };
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++)
        if (path_exists(paths[i])) return 1;
    return 0;
}

const char *wubu_hw_vulkan_icd(void)
{
    return wubu_hw_has_nvidia_icd() ? "/usr/share/vulkan/icd.d/nvidia_icd.json" : 0;
}

char *wubu_hw_vulkan_icd_chain(void)
{
    static char chain[512];
    static const char *const dirs[] = { "/usr/share/vulkan/icd.d", "/etc/vulkan/icd.d" };
    chain[0] = '\0';
    size_t used = 0;
    for (size_t d = 0; d < sizeof(dirs) / sizeof(dirs[0]); d++) {
        DIR *dir = opendir(dirs[d]);
        if (!dir) continue;
        struct dirent *e;
        while ((e = readdir(dir)) != NULL) {
            if (e->d_name[0] == '.') continue;
            size_t len = strlen(e->d_name);
            if (len < 5 || strcmp(e->d_name + len - 5, ".json") != 0) continue;
            size_t need = strlen(dirs[d]) + 1 + len + 1;
            if (used + need >= sizeof(chain)) { closedir(dir); return chain; }
            if (used) chain[used++] = ':';
            used += (size_t)snprintf(chain + used, sizeof(chain) - used, "%s/%s", dirs[d], e->d_name);
        }
        closedir(dir);
    }
    return chain;
}
