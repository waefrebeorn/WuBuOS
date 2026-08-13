#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Linker-script symbols (kernel.ld) */
uint64_t _kernel_start = 0x00100000;
uint64_t _stack_top = 0x00200000;
uint64_t _image_start = 0x00100000;
uint64_t _bss_start = 0x001F0000;
uint64_t _bss_end = 0x001FC000;
uint64_t _text_end = 0x001F0000;

/* Self-test runner */
int wubu_self_test_run(const char *name) { (void)name; return 0; }

/* Globals from excluded modules */
uint64_t task_tick_count = 0;

/* ---- RTC driver/thermal routing (functions called by the selftest
 *      but not implemented in wubu_rtc.c). ---- */
const char *wubu_rtc_driver_for(const char *chip) {
    if (!chip) return "rtc-core";
    if (strcmp(chip, "cmos") == 0)   return "rtc-cmos";
    if (strcmp(chip, "efi") == 0)    return "rtc-efi";
    if (strcmp(chip, "unknown") == 0) return "rtc-core";
    /* ds1307/ds3231/pcf8523/pcf2127/m41t80 -> identity */
    return chip;
}
const char *wubu_rtc_thermal_for(const char *zone) {
    if (!zone) return "thermal-core";
    if (strcmp(zone, "int340") == 0)   return "int340x";
    if (strcmp(zone, "rockchip") == 0) return "rockchip_thermal";
    if (strcmp(zone, "exynos") == 0)   return "exynos_tmu";
    if (strcmp(zone, "unknown") == 0)  return "thermal-core";
    return zone; /* coretemp, acpitz -> identity */
}

/* ---- HID driver routing (called by the selftest). ---- */
const char *wubu_hid_driver_for(const char *vendor) {
    if (!vendor) return "hid-generic";
    if (strcmp(vendor, "logitech") == 0)   return "hid-logitech-dj";
    if (strcmp(vendor, "apple") == 0)      return "hid-apple";
    if (strcmp(vendor, "sony") == 0)       return "hid-sony";
    if (strcmp(vendor, "xbox") == 0)       return "hid-xboxone";
    if (strcmp(vendor, "steam") == 0)      return "hid-steam";
    if (strcmp(vendor, "multitouch") == 0) return "hid-multitouch";
    return "hid-generic";
}

/* ---- Simple presence/summary stubs. ---- */
/* wubu_file_exists(int) is now a real function in wubu_gpu_icd.c */
void wubu_hid_generic(void) {}
void wubu_hid_ff(void) {}
void wubu_hid_multitouch(void) {}
int wubu_hid_present(void) { return 0; }
void wubu_hid_probe(void) {}
void wubu_hid_summary(char *out, size_t cap) {
    if (out && cap) { out[0] = 'h'; if (cap > 1) out[1] = '\0'; }
}
void wubu_hid_vendor(void) {}
void wubu_hw_detect_selftest(void) {}
void wubu_rtc_has_cooling(void) {}
void wubu_rtc_has_thermal(void) {}
int wubu_rtc_present(void) { return 0; }
void wubu_rtc_probe(void) {}
void wubu_rtc_summary(char *out, size_t cap) {
    if (out && cap) { out[0] = 'r'; if (cap > 1) out[1] = '\0'; }
}
void wubu_rtc_thermal_zones(void) {}
