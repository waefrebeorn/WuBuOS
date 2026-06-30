#ifndef LIBC_H
#define LIBC_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

/* CPU operations */
static inline void cli(void) { __asm__ __volatile__("cli" ::: "memory"); }
static inline void sti(void) { __asm__ __volatile__("sti" ::: "memory"); }
static inline void hlt(void) { __asm__ __volatile__("hlt"); }
static inline void pause(void) { __asm__ __volatile__("pause"); }

#define HLT() hlt()

static inline uint64_t rdtsc(void);

static inline void lgdt(void *ptr);

static inline uint64_t read_cr0(void);
static inline void write_cr0(uint64_t val);

static inline uint64_t read_cr3(void);
static inline void write_cr3(uint64_t val);

static inline uint64_t read_cr4(void);
static inline void write_cr4(uint64_t val);

static inline void wrmsr(uint32_t msr, uint64_t val);
static inline uint64_t rdmsr(uint32_t msr);

/* Panic */
void kernel_panic(const char *msg);

/* Standard I/O stubs */
typedef struct { int __val[1]; } FILE;
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

/* Time functions */
typedef long time_t;
time_t time(time_t *tloc);
struct tm { int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst; };
struct tm *gmtime(const time_t *timer);

/* Signal handling */
struct sigaction { void (*sa_handler)(int); int sa_flags; };
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
void (*__sysv_signal(int sig, void (*handler)(int)))(int);

/* Character classification */
extern int *__ctype_toupper_loc(void);

/* Fortified string functions */
extern void *__memset_chk(void *s, int c, size_t n, size_t sizelim);
extern void *__memcpy_chk(void *dest, const void *src, size_t n, size_t dstlen);
extern int __snprintf_chk(char *s, size_t n, size_t sizelim, int flags, const char *fmt, ...);
extern int __printf_chk(int flag, const char *fmt, ...);

/* Stack protector */
extern uintptr_t __stack_chk_guard;
void __stack_chk_fail(void);

/* Setjmp/longjmp - bare metal stubs only */
#ifdef WUBU_BAREMETAL
#if !defined(jmp_buf) && !defined(__jmp_buf_defined) && !defined(_SETJMP_H)
typedef int jmp_buf[1];
int _setjmp(jmp_buf env);
void __longjmp_chk(jmp_buf env, int val);
#endif

/* Character classification locale */
extern int *__ctype_toupper_loc(void);

/* Fortified string functions */
extern void *__memset_chk(void *s, int c, size_t n, size_t sizelim);
extern void *__memcpy_chk(void *dest, const void *src, size_t n, size_t dstlen);
extern int __snprintf_chk(char *s, size_t n, size_t sizelim, int flags, const char *fmt, ...);
extern int __printf_chk(int flag, const char *fmt, ...);

/* Stack protector */
extern uintptr_t __stack_chk_guard;
void __stack_chk_fail(void);

/* Setjmp/longjmp */
extern int _setjmp(jmp_buf env);
extern void __longjmp_chk(jmp_buf env, int val);

/* Character classification locale */
extern int *__ctype_toupper_loc(void);

/* Fortified string functions */
extern void *__memset_chk(void *s, int c, size_t n, size_t sizelim);
extern void *__memcpy_chk(void *dest, const void *src, size_t n, size_t dstlen);
extern int __snprintf_chk(char *s, size_t n, size_t sizelim, int flags, const char *fmt, ...);
extern int __printf_chk(int flag, const char *fmt, ...);
#endif /* WUBU_BAREMETAL */

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

#endif /* LIBC_H */