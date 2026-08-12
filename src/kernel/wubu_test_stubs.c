/* wubu_test_stubs.c — stub symbols for standalone test builds.
 * Only define symbols NOT provided by other kernel modules
 * that are compiled into the test target.
 */

#include <stdint.h>

/* Linker-script symbols (kernel.ld) - these are NOT in any .c file */
uint64_t _kernel_start = 0x00100000;
uint64_t _stack_top = 0x00200000;
uint64_t _image_start = 0x00100000;
uint64_t _bss_start = 0x001F0000;
uint64_t _bss_end = 0x001FC000;

/* Self-test runner (wubu_verifier.c calls wubu_self_test_run) */
int wubu_self_test_run(const char *name) { (void)name; return 0; }

