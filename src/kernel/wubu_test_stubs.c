#include <stdint.h>
#include <unistd.h>
#include <stddef.h>

/* Linker-script symbols (kernel.ld) */
uint64_t _kernel_start = 0x00100000;
uint64_t _stack_top = 0x00200000;
uint64_t _image_start = 0x00100000;
uint64_t _bss_start = 0x001F0000;
uint64_t _bss_end = 0x001FC000;
uint64_t _text_end = 0x001F0000;

/* Self-test runner */
int wubu_self_test_run(const char *name) { (void)name; return 0; }
