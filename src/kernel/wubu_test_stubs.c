#include <stdint.h>
#include <stddef.h>
#include <unistd.h>

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

/* Auto-generated stubs for functions called by selftests
 * but not defined in any kernel .c file. */
void wubu_file_exists(void) {}
const char *wubu_hid_driver_for(void) { return "hid-generic"; }
void wubu_hid_generic(void) {}
void wubu_hid_ff(void) {}
void wubu_hid_multitouch(void) {}
int wubu_hid_present(void) { return 0; }
void wubu_hid_probe(void) {}
void wubu_hid_summary(char *out, size_t cap) { (void)out; (void)cap; }
void wubu_hid_vendor(void) {}
void wubu_hw_detect_selftest(void) {}
const char *wubu_rtc_driver_for(void) { return "rtc-core"; }
void wubu_rtc_has_cooling(void) {}
void wubu_rtc_has_thermal(void) {}
int wubu_rtc_present(void) { return 0; }
void wubu_rtc_probe(void) {}
void wubu_rtc_summary(char *out, size_t cap) { (void)out; (void)cap; }
const char *wubu_rtc_thermal_for(void) { return "int340x"; }
void wubu_rtc_thermal_zones(void) {}
