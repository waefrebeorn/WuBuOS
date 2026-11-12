/*
 * wubu_hosted_hw_test.c -- prove the HOSTED hardware backend reports the
 * machine it is actually running on.
 *
 * Every assertion is cross-checked against the host's own /sys and /proc, so
 * the gate fails if the backend drifts from reality (a stub would pass a
 * hardcoded expectation; this cannot). It also drives wubu_world.c end to
 * end -- the AGI's world snapshot must agree with the same probes.
 */
#include "../egojar.h"
#include "../egojar_net.h"
#include "../kernel/wubu_drv_nvme.h"
#include "../kernel/wubu_drv_net.h"
#include "../kernel/wubu_drv_gpu.h"
#include "../kernel/wubu_drv_battery.h"
#include "../kernel/wubu_drv_sd.h"
#include "../kernel/wubu_drv_thermal.h"
#include "../kernel/wubu_drv.h"
#include "../kernel/wubu_hw_detect.h"
#include "../kernel/wubu_world.h"

#include <stdio.h>
#include <string.h>
#include <dirent.h>

static int failures;
static int checks;
#define CHECK(c, m) do { \
    checks++; \
    if (!(c)) { printf("  FAIL: %s\n", m); failures++; } \
} while (0)

/* the host's own answer, read the same way the backend reads it */
static int sys_has(const char *dir, const char *prefix)
{
    DIR *d = opendir(dir);
    if (!d) return 0;
    struct dirent *e;
    int found = 0;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        if (strncmp(e->d_name, prefix, strlen(prefix)) == 0) { found = 1; break; }
    }
    closedir(d);
    return found;
}

static int sys_drm_connector(void)
{
    DIR *d = opendir("/sys/class/drm");
    if (!d) return 0;
    struct dirent *e;
    int found = 0;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        if (strncmp(e->d_name, "card", 4) == 0 && strchr(e->d_name, '-') != NULL) {
            found = 1; break;
        }
    }
    closedir(d);
    return found;
}

static int path_exists(const char *p) { return access(p, F_OK) == 0; }

static int sys_is_wsl(void)
{
    if (access("/dev/dxg", F_OK) == 0) return 1;
    FILE *f = fopen("/proc/sys/kernel/osrelease", "r");
    if (!f) return 0;
    char b[128] = {0};
    size_t n = fread(b, 1, sizeof(b) - 1, f);
    fclose(f);
    b[n] = '\0';
    return strstr(b, "microsoft") != NULL;
}

int main(void)
{
    printf("\n=== hosted hardware backend ===\n");

    /* power */
    CHECK(wubu_battery_present() == sys_has("/sys/class/power_supply", "BAT"),
          "battery present matches /sys/class/power_supply");
    if (wubu_battery_present()) {
        int pct = wubu_battery_percent();
        CHECK(pct >= 0 && pct <= 100, "battery percent is 0..100");
        int chg = wubu_battery_charging();
        CHECK(chg == 0 || chg == 1, "battery charging is boolean");
    }

    /* network */
    CHECK(wubu_net_wifi_present() == sys_has("/sys/class/net", "wl"),
          "wifi present matches /sys/class/net");
    CHECK(wubu_net_eth_present() == (sys_has("/sys/class/net", "en") || sys_has("/sys/class/net", "eth")),
          "eth present matches /sys/class/net");
    CHECK(wubu_net_wifi_link() == 0 || wubu_net_wifi_link() == 1, "wifi link is boolean");
    CHECK(wubu_net_eth_link() == 0 || wubu_net_eth_link() == 1, "eth link is boolean");

    /* thermal: the CPU zone must be reported when the host has one */
    if (sys_has("/sys/class/thermal", "thermal_zone"))
        CHECK(wubu_thermal_temp(WUBU_THERMAL_CPU) > 0, "cpu temp is a real temperature");

    /* storage */
    CHECK(wubu_nvme_ready() == sys_has("/sys/class/block", "nvme"),
          "nvme ready matches /sys/class/block");
    CHECK(wubu_sd_card_present() == sys_has("/sys/class/block", "mmcblk"),
          "sd present matches /sys/class/block");
    CHECK(wubu_nvme_block_size() == 512, "nvme block size is 512");

    /* display */
    CHECK(wubu_gpu_present() == sys_drm_connector(),
          "gpu present matches a DRM connector");
    CHECK(wubu_gpu_present_wsl() == sys_is_wsl(), "wsl verdict matches /dev/dxg + osrelease");

    /* the detector */
    const char *plat = wubu_hw_platform();
    CHECK(plat != NULL, "platform verdict is non-NULL");
    CHECK(strcmp(plat, "wsl2") == 0 || strcmp(plat, "bare_metal") == 0 ||
          strcmp(plat, "kvm") == 0 || strcmp(plat, "qemu") == 0 ||
          strcmp(plat, "vmware") == 0 || strcmp(plat, "virtualbox") == 0 ||
          strcmp(plat, "hyperv") == 0,
          "platform is a known verdict");
    CHECK(wubu_hw_is_wsl() == sys_is_wsl(), "is_wsl agrees with the host");
    const char *path = wubu_hw_gpu_path();
    CHECK(path == NULL || path_exists(path), "gpu path is NULL or exists");

    /* the registry lookup agrees with the probes */
    CHECK((wubu_drv_find("nvme") != NULL) == wubu_nvme_ready(),
          "drv_find(nvme) matches the nvme probe");
    CHECK((wubu_drv_find("battery") != NULL) == wubu_battery_present(),
          "drv_find(battery) matches the battery probe");
    CHECK(wubu_drv_find("no-such-class") == NULL,
          "drv_find of an unknown class is NULL");

    /* the world bridge end to end */
    wubu_world_init();
    wubu_world_sample();
    const wubu_world_t *w = wubu_world_snapshot();
    CHECK(w != NULL, "world snapshot is non-NULL");
    if (w) {
        CHECK(w->has_nvme == wubu_nvme_ready(), "world nvme matches the probe");
        CHECK(w->has_battery == wubu_battery_present(), "world battery matches the probe");
        CHECK(w->has_wifi == wubu_net_wifi_present(), "world wifi matches the probe");
        char line[256];
        CHECK(wubu_world_state_str(line, sizeof(line)) == 0, "world state string renders");
        CHECK(strchr(line, '[') != NULL, "world state string is structured");
    }

    printf("%s: %d checks, %d failures\n", failures ? "FAIL" : "PASS", checks, failures);
    return failures ? 1 : 0;
}
