/* Minimal libc for bare-metal kernel */

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

/* Forward declare needed types */
typedef struct { int __val[1]; } FILE;
typedef long time_t;
struct tm { int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst; };
struct sigaction { void (*sa_handler)(int); int sa_flags; };
typedef int (*sighandler_t)(int);
typedef int jmp_buf[1];

/* Forward declarations */
void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);

/* Memory allocation - simple bump allocator */

/* Memory allocation - simple bump allocator */
static uint8_t *heap_ptr = NULL;
static uint8_t *heap_end = NULL;

void *malloc(size_t size) {
    if (!heap_ptr) return NULL;
    size = (size + 7) & ~7;  /* 8-byte align */
    if (heap_ptr + size > heap_end) return NULL;
    void *ptr = heap_ptr;
    heap_ptr += size;
    return ptr;
}

void free(void *ptr) {
    (void)ptr;
}

void *calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *ptr = malloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

/* String functions */
void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) *d++ = *s++;
    return dest;
}

void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;
    while (n--) *p++ = (uint8_t)c;
    return s;
}

size_t strlen(const char *s) {
    size_t len = 0;
    while (*s++) len++;
    return len;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s1 == *s2) { s1++; s2++; }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && *s1 == *s2) { s1++; s2++; n--; }
    return n ? *(unsigned char *)s1 - *(unsigned char *)s2 : 0;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    char *d = dest;
    while (n && (*d++ = *src++)) n--;
    while (n--) *d++ = '\0';
    return dest;
}

char *strcat(char *dest, const char *src) {
    char *d = dest;
    while (*d) d++;
    while ((*d++ = *src++));
    return dest;
}

/* Integer to string */
char *itoa(int value, char *str, int base) {
    if (base < 2 || base > 36) return NULL;
    char *ptr = str;
    char *ptr1 = str;
    char tmp_char;
    int tmp_value;
    int sign = 0;
    
    if (value < 0 && base == 10) {
        sign = 1;
        value = -value;
    }
    
    do {
        tmp_value = value % base;
        *ptr++ = (tmp_value < 10) ? '0' + tmp_value : 'a' + tmp_value - 10;
        value /= base;
    } while (value);
    
    if (sign) *ptr++ = '-';
    *ptr = '\0';
    
    /* Reverse string */
    while (ptr1 < --ptr) {
        tmp_char = *ptr;
        *ptr = *ptr1;
        *ptr1 = tmp_char;
        ptr1++;
    }
    
    return str;
}

/* Formatted output - minimal printf */
static int putchar(void (*putc)(char), char c) {
    putc(c);
    return 1;
}

static int puts(void (*putc)(char), const char *s) {
    int count = 0;
    while (*s) count += putchar(putc, *s++);
    return count;
}

int vsprintf(char *buf, const char *fmt, va_list args) {
    char *out = buf;
    const char *p = fmt;

    while (*p) {
        if (*p != '%') {
            *out++ = *p++;
            continue;
        }
        p++;  /* skip '%' */

        /* Parse flags (simplified: only '-' for left-pad with 0s) */
        int zero_pad = 0;
        int width = 0;
        int is_long = 0;

        /* Zero-fill flag */
        if (*p == '0') {
            zero_pad = 1;
            p++;
        }

        /* Width digits */
        while (*p >= '0' && *p <= '9') {
            width = width * 10 + (*p - '0');
            p++;
        }

        /* Long modifier */
        if (*p == 'l') {
            is_long = 1;
            p++;
            if (*p == 'l') p++;  /* ll */
        }

        char spec = *p++;
        char numbuf[32];

        switch (spec) {
            case 'd': case 'i': {
                long val = is_long ? va_arg(args, long) : (long)va_arg(args, int);
                if (val < 0) {
                    *out++ = '-';
                    val = -val;
                }
                itoa((int)val, numbuf, 10);
                int len = (int)strlen(numbuf);
                if (zero_pad && width > len) {
                    for (int i = 0; i < width - len; i++) *out++ = '0';
                }
                strcpy(out, numbuf);
                out += strlen(numbuf);
                break;
            }
            case 'u': {
                unsigned long val = is_long ? va_arg(args, unsigned long)
                                            : (unsigned long)va_arg(args, unsigned int);
                /* Manual conversion to avoid ita's negative handling */
                int idx = 0;
                if (val == 0) { numbuf[idx++] = '0'; }
                while (val > 0) {
                    numbuf[idx++] = '0' + (val % 10);
                    val /= 10;
                }
                numbuf[idx] = '\0';
                /* Reverse */
                for (int i = 0; i < idx / 2; i++) {
                    char tmp = numbuf[i];
                    numbuf[i] = numbuf[idx - 1 - i];
                    numbuf[idx - 1 - i] = tmp;
                }
                if (zero_pad && width > idx) {
                    for (int i = 0; i < width - idx; i++) *out++ = '0';
                }
                strcpy(out, numbuf);
                out += strlen(numbuf);
                break;
            }
            case 'x': case 'X': {
                unsigned long val = is_long ? va_arg(args, unsigned long)
                                            : (unsigned long)va_arg(args, unsigned int);
                int idx = 0;
                if (val == 0) { numbuf[idx++] = '0'; }
                while (val > 0) {
                    int digit = val & 0xF;
                    numbuf[idx++] = (digit < 10) ? '0' + digit
                                 : (spec == 'X' ? 'A' + digit - 10
                                                : 'a' + digit - 10);
                    val >>= 4;
                }
                numbuf[idx] = '\0';
                /* Reverse */
                for (int i = 0; i < idx / 2; i++) {
                    char tmp = numbuf[i];
                    numbuf[i] = numbuf[idx - 1 - i];
                    numbuf[idx - 1 - i] = tmp;
                }
                if (zero_pad && width > idx) {
                    for (int i = 0; i < width - idx; i++) *out++ = '0';
                }
                strcpy(out, numbuf);
                out += strlen(numbuf);
                break;
            }
            case 's': {
                const char *s = va_arg(args, const char*);
                if (!s) s = "(null)";
                int len = (int)strlen(s);
                if (width > len) {
                    for (int i = 0; i < width - len; i++) *out++ = ' ';
                }
                strcpy(out, s);
                out += len;
                break;
            }
            case 'c': {
                char c = (char)va_arg(args, int);
                *out++ = c;
                break;
            }
            case 'p': {
                *out++ = '0';
                *out++ = 'x';
                unsigned long val = (unsigned long)va_arg(args, void*);
                int idx = 0;
                if (val == 0) { numbuf[idx++] = '0'; }
                while (val > 0) {
                    int digit = val & 0xF;
                    numbuf[idx++] = (digit < 10) ? '0' + digit : 'a' + digit - 10;
                    val >>= 4;
                }
                numbuf[idx] = '\0';
                for (int i = 0; i < idx / 2; i++) {
                    char tmp = numbuf[i];
                    numbuf[i] = numbuf[idx - 1 - i];
                    numbuf[idx - 1 - i] = tmp;
                }
                strcpy(out, numbuf);
                out += strlen(numbuf);
                break;
            }
            case '%': {
                *out++ = '%';
                break;
            }
            default: {
                /* Unknown specifier: pass through literally */
                *out++ = '%';
                if (spec) *out++ = spec;
                break;
            }
        }
    }

    *out = '\0';
    return (int)(out - buf);
}

int sprintf(char *buf, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = vsprintf(buf, fmt, args);
    va_end(args);
    return ret;
}

/* va_list support - don't redefine if stdarg.h provides them */
#if !defined(va_list) && !defined(__va_list) && !defined(_VA_LIST)
typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_end(ap) __builtin_va_end(ap)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#endif

/* assert */
void __assert_fail(const char *assertion, const char *file, unsigned int line, const char *function) {
    (void)assertion; (void)file; (void)line; (void)function;
    for (;;) __asm__ __volatile__("hlt");
}

/* Stack protector */
uintptr_t __stack_chk_guard = 0xdeadbeefdeadbeef;
void __stack_chk_fail(void) {
    for (;;) __asm__ __volatile__("cli; hlt");
}

/* Initialize heap - call from kernel_main */
void libc_init_heap(void *start, void *end) {
    heap_ptr = (uint8_t *)start;
    heap_end = (uint8_t *)end;
}

/* Low-level I/O */
void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void outw(uint16_t port, uint16_t val) {
    __asm__ __volatile__("outw %0, %1" : : "a"(val), "Nd"(port));
}

uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ __volatile__("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void outl(uint16_t port, uint32_t val) {
    __asm__ __volatile__("outl %0, %1" : : "a"(val), "Nd"(port));
}

uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ __volatile__("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* CPU operations */
static inline void cli(void) { __asm__ __volatile__("cli" ::: "memory"); }
static inline void sti(void) { __asm__ __volatile__("sti" ::: "memory"); }
static inline void hlt(void) { __asm__ __volatile__("hlt"); }
static inline void pause(void) { __asm__ __volatile__("pause"); }

#define HLT() hlt()

static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static inline void lgdt(void *ptr) {
    __asm__ __volatile__("lgdt %0" : : "m"(*(uint16_t(*)[2])ptr) : "memory");
}

/* Externs for kernel_main */
extern void kernel_main(void *boot_info);
extern void wubu_shell_run(void *arg);
extern void wubu_gaad_init(void);
extern void task_preempt_enable(void);
extern void task_yield(void);
extern int mem_init(uint64_t size);
extern int vbe_init(int w, int h);
extern void input_init(void);
extern void ps2_init(int w, int h);
extern int tasking_init(void);
extern int interrupt_init(void);

/* Global symbols */
extern uint64_t _kernel_start;
extern uint64_t _kernel_end;
extern uint64_t _bss_start;
extern uint64_t _bss_end;
extern uint64_t _stack_top;