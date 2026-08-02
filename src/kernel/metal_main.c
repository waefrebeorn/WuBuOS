/*
 * metal_main.c  --  WuBuOS Bare-Metal Kernel Entry Point
 * 
 * Called from crt0.S after Limine/Stivale2 boot.
 * Initializes all kernel subsystems, then starts the shell.
 */

#include "memory.h"
#include "tasking.h"
#include "vbe.h"
#include "interrupt.h"
#include "input.h"
#include "wubu_gaad.h"
#include "wubu_agi_kernel.h"
#include "wubu_attest.h"
#include "ps2.h"
#include "klog.h"
#include "../hosted/wubu_metal.h"
#include <stdint.h>

/* Inline assembly helpers for freestanding mode */
#define ASM_VOLATILE(x) __asm__ __volatile__(x)
#define CLI() ASM_VOLATILE("cli")
#define STI() ASM_VOLATILE("sti")
#define HLT() ASM_VOLATILE("hlt")
#define PAUSE() ASM_VOLATILE("pause")

/* ==================================================================
 * External symbols from linker script / crt0
 * ================================================================= */
extern uint64_t _kernel_start;
extern uint64_t _kernel_end;
extern uint64_t _bss_start;
extern uint64_t _bss_end;
extern uint64_t _stack_top;

/* =================================================================
 * Limine boot-protocol detection flag
 *
 * Written by crt0.S (64-bit phase) BEFORE kernel_main is called:
 *   g_limine_ok = 1  -> booted via Limine; the limine_*_request.response
 *                        pointers were populated by the loader and are valid.
 *   g_limine_ok = 0  -> booted via multiboot / `qemu -kernel`; the .response
 *                        fields are uninitialized garbage and MUST NOT be
 *                        dereferenced (would #PF triple-fault).
 *
 * Lives in .data (initialized, not BSS) deliberately: kernel_main zeroes BSS
 * below, and if this flag were in BSS the zeroing would clobber the value
 * crt0 already set.  `used` + `volatile` keep the linker from stripping it
 * (its only writer is assembly, invisible to the C compiler).
 * ================================================================= */
__attribute__((used, section(".data"))) volatile uint8_t g_limine_ok = 0;

/* =================================================================
 * Limine/Stivale2 boot info (passed in registers)
 * ================================================================= */

struct limine_framebuffer {
    uint64_t address;
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bpp;
    uint8_t memory_model;
    uint8_t red_mask_size, red_mask_shift;
    uint8_t green_mask_size, green_mask_shift;
    uint8_t blue_mask_size, blue_mask_shift;
    uint8_t reserved;
};

struct limine_memmap_entry {
    uint64_t base;
    uint64_t length;
    uint64_t type;
};

struct limine_memmap_response {
    uint64_t revision;
    uint64_t entry_count;
    struct limine_memmap_entry *entries[];
};

/* Limine requests (placed in .rodata) */
__attribute__((section(".limine_requests")))
static volatile struct {
    uint64_t id[4];
    uint64_t revision;
    void *response;
} limine_framebuffer_request = {
    .id = {0xc7b1dd30df4c8b88, 0x6e1b0a4b5b8d7c0a, 0, 0},
    .revision = 0
};

__attribute__((section(".limine_requests")))
static volatile struct {
    uint64_t id[4];
    uint64_t revision;
    void *response;
} limine_memmap_request = {
    .id = {0x67cf3d9d378a806f, 0xc323c3c6f73f3e2a, 0, 0},
    .revision = 0
};

/* ==================================================================
 * Shell task entry (defined in wubu_shell.c)
 * ================================================================= */
extern void wubu_shell_run(void *arg);

/* =================================================================
 * Kernel Main  --  Bare Metal Entry
 * ================================================================= */

void kernel_main(void *boot_info) {
    (void)boot_info;  /* Parsed from registers in crt0.S */
    /* Raw serial heartbeat (no klog/string dependency) so we can tell from
     * the QEMU -serial trace whether we actually reached kernel_main and
     * where the boot dies.  'Z' = entered, 'A' = BSS zeroed, 'B' = heap ok. */
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'Z', %%al\n outb %%al, %%dx" ::: "dx","al");

    /* 1. Zero BSS FIRST -- before any subsystem init.  klog_init() below
     * sets g_klog_ready (a BSS variable); zeroing BSS *after* init would
     * wipe that flag and every later klog_printf would silently no-op. */
    uint64_t *bss = &_bss_start;
    uint64_t *bss_end = &_bss_end;
    while (bss < bss_end) *bss++ = 0;
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'A', %%al\n outb %%al, %%dx" ::: "dx","al");

    /* Diagnostic: emit fixed bytes (no deref) to bracket klog_init. */
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'h', %%al\n outb %%al, %%dx" ::: "dx","al");
    klog_init();
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'i', %%al\n outb %%al, %%dx" ::: "dx","al");
    klog_printf("WuBuOS: kernel_main entered (long mode OK)\n");
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'j', %%al\n outb %%al, %%dx" ::: "dx","al");
    klog_printf("WuBuOS: BSS zeroed\n");

    /* 1b. Consume the WuBuFW loader handoff (firmware attestation +
     * kernel digest). The AGI supervisor gates self-improvement promotion
     * on this: no attestation -> no self-modification. */
    {
        static const char hexd[] = "0123456789ABCDEF";
        char hexbuf[65];
        char *o = hexbuf;
        uint8_t p4[32];
        if (wubu_attest_load_scratch() == 0 && wubu_attest_pcr4_digest(p4) == 0) {
            for (int i = 0; i < 32; i++) {
                *o++ = hexd[p4[i] >> 4]; *o++ = hexd[p4[i] & 15];
            }
            *o = 0;
            klog_printf("WuBuOS: firmware attestation consumed (boot=%d sb=%d setup=%d pcr4=%s)\n",
                        (int)wubu_attest_boot_counter(),
                        wubu_attest_sb_enabled() ? 1 : 0,
                        wubu_attest_setup_mode() ? 1 : 0, hexbuf);
        } else {
            klog_printf("WuBuOS: no firmware attestation (loader handoff absent) "
                        "-- self-improve promotion disabled\n");
        }
    }
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'k', %%al\n outb %%al, %%dx" ::: "dx","al");

    /* 2. Initialize memory allocator FIRST (everything needs it) */
    /* Calculate available memory from Limine memmap (only if we actually
     * booted via Limine; g_limine_ok is set by crt0 and gates the otherwise
     * uninitialized .response pointers so a multiboot boot can't deref garbage). */
    uint64_t mem_size = 64 * 1024 * 1024;  /* Default 64MB fallback */
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'q', %%al\n outb %%al, %%dx" ::: "dx","al");
    if (0 && limine_memmap_request.response) {
        struct limine_memmap_response *resp = limine_memmap_request.response;
        for (uint64_t i = 0; i < resp->entry_count; i++) {
            if (resp->entries[i]->type == 0) {  /* Usable RAM */
                mem_size += resp->entries[i]->length;
            }
        }
    }
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'r', %%al\n outb %%al, %%dx" ::: "dx","al");
    if (mem_init(mem_size) != 0) {
        klog_printf("WuBuOS PANIC: mem_init failed\n");
        for (;;) { CLI(); HLT(); }
    }
    klog_printf("WuBuOS: heap initialized (%u MB)\n", (unsigned)(mem_size >> 20));
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'B', %%al\n outb %%al, %%dx" ::: "dx","al");

    /* 3. Initialize interrupt subsystem (IDT, PIC, PIT) */
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'1', %%al\n outb %%al, %%dx" ::: "dx","al");
    if (interrupt_init() != 0) {
        klog_printf("WuBuOS PANIC: interrupt_init failed\n");
        for (;;) { CLI(); HLT(); }
    }
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'o', %%al\n outb %%al, %%dx" ::: "dx","al");
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'g', %%al\n outb %%al, %%dx" ::: "dx","al");
    klog_printf("WuBuOS: interrupts initialized\n");

    /* 4. Initialize VBE/DRM-KMS framebuffer */
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'2', %%al\n outb %%al, %%dx" ::: "dx","al");
    int fb_width = 1920, fb_height = 1080;
    struct limine_framebuffer *fb = NULL;
    if (g_limine_ok && limine_framebuffer_request.response) {
        fb = limine_framebuffer_request.response;
        fb_width = fb->width;
        fb_height = fb->height;
        /* Map framebuffer - already identity-mapped by Limine */
        /* VBE init will use this directly */
    }
    if (vbe_init(fb_width, fb_height) != 0) {
        klog_printf("WuBuOS: VBE init failed (non-fatal under emulator)\n");
    } else {
        klog_printf("WuBuOS: VBE initialized (%ux%u)\n", fb_width, fb_height);
    }
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'3', %%al\n outb %%al, %%dx" ::: "dx","al");

    /* 5. Initialize GAAD (φ-structured allocation for window snap) */
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'3', %%al\n outb %%al, %%dx" ::: "dx","al");
    extern void wubu_gaad_init(void);
    wubu_gaad_init();

    /* 6. Initialize input subsystem (PS/2 + evdev fallback) */
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'4', %%al\n outb %%al, %%dx" ::: "dx","al");
    input_init();

    /* 6b. Initialize PS/2 keyboard/mouse for bare metal */
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'5', %%al\n outb %%al, %%dx" ::: "dx","al");
    int fb_w = 1920, fb_h = 1080;
    if (g_limine_ok && limine_framebuffer_request.response) {
        struct limine_framebuffer *fb2 = limine_framebuffer_request.response;
        fb_w = fb2->width;
        fb_h = fb2->height;
    }
    ps2_init(fb_w, fb_h);
    klog_printf("WuBuOS: input/PS2 initialized\n");

    /* 7. Initialize tasking (cooperative scheduler, PIT timer) */
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'6', %%al\n outb %%al, %%dx" ::: "dx","al");
    if (tasking_init() != 0) {
        klog_printf("WuBuOS PANIC: tasking_init failed\n");
        for (;;) { CLI(); HLT(); }
    }
    klog_printf("WuBuOS: tasking initialized\n");

    /* 8. Enable preemptive scheduling (timer-driven) */
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'7', %%al\n outb %%al, %%dx" ::: "dx","al");
    extern void task_preempt_enable(void);
    task_preempt_enable();

    /* 9. Boot the AGI kernel supervisor (ring-0 operator + agent realm).
     *    This replaces the old `for(;;) HLT();` shell: the OS is now an AGI
     *    kernel -- it decomposes the viewport via GAAD, spawns a co-resident
     *    REALM_AGENT task, and runs the independent-verifier self-improve loop
     *    ticked by the PIT timer. Safe by default (no verifier => no promote). */
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'8', %%al\n outb %%al, %%dx" ::: "dx","al");
    int agi_w = 1920, agi_h = 1080;
    if (g_limine_ok && limine_framebuffer_request.response) {
        struct limine_framebuffer *fb3 = limine_framebuffer_request.response;
        agi_w = (int)fb3->width;
        agi_h = (int)fb3->height;
    }
    wubu_agi_kernel_t *agi = wubu_agi_kernel_init(agi_w, agi_h);
    if (!agi) {
        klog_printf("WuBuOS PANIC: agi_kernel_init failed\n");
        for (;;) { CLI(); HLT(); }
    }
    klog_printf("WuBuOS: AGI kernel booted (regions=%d)\n",
                wubu_agi_kernel_region_count(agi));

    /* 10. Enter the cooperative supervisor run loop (spawns agent task,
     *     yields to the PIT-ticked scheduler). Never returns. */
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'9', %%al\n outb %%al, %%dx" ::: "dx","al");
    wubu_agi_kernel_run(agi);  /* Never returns */

    /* Unreachable: clean isa-debug-exit so the VM halts instead of looping. */
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'H', %%al\n outb %%al, %%dx\n"
                         "movb $0, %%al\n movw $0xf4, %%dx\n outb %%al, %%dx" ::: "dx","al");

    /* Unreachable */
    for (;;) { CLI(); HLT(); }
}

/* Shell task entry point -- retained as the AGI operator's host surface.
 * On bare metal the supervisor (wubu_agi_kernel_run) is the live loop; this
 * remains available as a co-resident task if the operator spawns it. */
void wubu_shell_run(void *arg) {
    (void)arg;
    for (;;) {
        task_yield();
    }
}

/* GAAD initialization -- real work: decompose the active framebuffer into
 * golden-ratio regions so the WM/agent viewport is φ-structured from boot. */
void wubu_gaad_init(void) {
    int w = 1920, h = 1080;
    if (g_limine_ok && limine_framebuffer_request.response) {
        struct limine_framebuffer *fb = limine_framebuffer_request.response;
        w = (int)fb->width;
        h = (int)fb->height;
    }
    static WubuGaadDecomp g_gaad_boot;   /* persists for WM snapping */
    wubu_gaad_decompose(w, h, WUBU_GAAD_MAX_DEPTH, &g_gaad_boot);
    if (klog_printf)
        klog_printf("WuBuOS: GAAD viewport decomposed (%dx%d, %d regions)\n",
                    w, h, g_gaad_boot.n_regions);
}

/* ==================================================================
 * Panic Handler
 */
void kernel_panic(const char *msg) {
    (void)msg;
    CLI();
    /* Would draw msg to framebuffer */
    for (;;) { HLT(); }
}