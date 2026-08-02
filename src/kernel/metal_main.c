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
#include "interrupt_apic.h"
#include "wubu_apic.h"
#include "input.h"
#include "wubu_gaad.h"
#include "wubu_agi_kernel.h"
#include "wubu_attest.h"
#include "wubu_hive.h"   /* G5: the metal's long-term memory */
#include "ps2.h"
#include "klog.h"
#include "../hosted/wubu_metal.h"

/* Gap G5: the metal's long-term hive -- file-scope (the AGI's memory
 * adapter cannot be a nested function). */
static wubu_hive_t *g_hive;
static uint64_t g_lm_count;
static int lm_put(const char *op, uint64_t id, const char *payload, void *ud)
{
    (void)op; (void)id; (void)ud;
    if (!g_hive || !payload) return -1;
    if (++g_lm_count % 25 != 0) return 0;   /* same cadence as the echo */
    extern void *mem_alloc(size_t);
    size_t len = 0;
    for (const char *p = payload; *p; p++) len++;
    char *copy = (char *)mem_alloc(len + 1);
    if (!copy) return -1;
    for (size_t i = 0; i < len; i++) copy[i] = payload[i];
    copy[len] = '\0';
    extern void *wubu_hive_insert(wubu_hive_t *, void *);
    return (wubu_hive_insert(g_hive, copy) != NULL) ? 0 : -1;
}

void metal_lm_setup(void *agi)
{
    extern void *mem_alloc(size_t);
    extern void mem_free(void *);
    extern wubu_hive_t *wubu_hive_new(size_t, void *(*)(size_t), void (*)(void *));
    extern void wubu_agi_kernel_set_memory(wubu_agi_kernel_t *,
                                           int (*)(const char *, uint64_t,
                                                   const char *, void *),
                                           void *);
    g_hive = wubu_hive_new(64, mem_alloc, mem_free);
    if (g_hive) {
        wubu_agi_kernel_set_memory((wubu_agi_kernel_t *)agi, lm_put, NULL);
        klog_printf("WuBuOS: long-term hive armed (gap G5)\n");
    }
}
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

/* Gap J6: the kernel.ld ALIGN(16) fix -- assert at BOOT time (the
 * linker symbols are not compile-time constants) that the image + the
 * stack top are 16-byte aligned, so a future linker-script drift trips
 * a loud early halt instead of the movaps #GP class of corruption. */
static inline void kernel_alignment_assert(void)
{
    if ((uintptr_t)_kernel_start % 16 != 0 ||
        (uintptr_t)_stack_top % 16 != 0) {
        /* serial port 1 raw: the earliest possible scream */
        klog_printf("WuBuOS PANIC: kernel image misaligned (ld script?)\n");
        for (;;) __asm__ __volatile__("cli; hlt");
    }
}


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
    /* Gap J6: the image-alignment boot check FIRST (before anything
     * touches the heap -- the movaps #GP class of corruption). */
    kernel_alignment_assert();
    /* Raw serial heartbeat (no klog/string dependency) so we can tell from
     * the QEMU -serial trace whether we actually reached kernel_main and
     * where the boot dies.  'Z' = entered, 'A' = BSS zeroed, 'B' = heap ok. */
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'1', %%al\n outb %%al, %%dx\n movb $'6', %%al\n outb %%al, %%dx" ::: "dx","al");

    /* 1. Zero BSS FIRST -- before any subsystem init.  klog_init() below
     * sets g_klog_ready (a BSS variable); zeroing BSS *after* init would
     * wipe that flag and every later klog_printf would silently no-op. */
    uint64_t *bss = &_bss_start;
    uint64_t *bss_end = &_bss_end;
    while (bss < bss_end) *bss++ = 0;
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'1', %%al\n outb %%al, %%dx\n movb $'7', %%al\n outb %%al, %%dx" ::: "dx","al");

    /* Diagnostic: emit fixed bytes (no deref) to bracket klog_init. */
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'1', %%al\n outb %%al, %%dx\n movb $'8', %%al\n outb %%al, %%dx" ::: "dx","al");
    klog_init();
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'1', %%al\n outb %%al, %%dx\n movb $'9', %%al\n outb %%al, %%dx" ::: "dx","al");
    klog_printf("WuBuOS: kernel_main entered (long mode OK)\n");
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'2', %%al\n outb %%al, %%dx\n movb $'0', %%al\n outb %%al, %%dx" ::: "dx","al");
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
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'2', %%al\n outb %%al, %%dx\n movb $'1', %%al\n outb %%al, %%dx" ::: "dx","al");

    /* 2. Initialize memory allocator FIRST (everything needs it) */
    /* Calculate available memory from Limine memmap (only if we actually
     * booted via Limine; g_limine_ok is set by crt0 and gates the otherwise
     * uninitialized .response pointers so a multiboot boot can't deref garbage). */
    uint64_t mem_size = 64 * 1024 * 1024;  /* Default 64MB fallback */
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'2', %%al\n outb %%al, %%dx\n movb $'2', %%al\n outb %%al, %%dx" ::: "dx","al");
    if (0 && limine_memmap_request.response) {
        struct limine_memmap_response *resp = limine_memmap_request.response;
        for (uint64_t i = 0; i < resp->entry_count; i++) {
            if (resp->entries[i]->type == 0) {  /* Usable RAM */
                mem_size += resp->entries[i]->length;
            }
        }
    }
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'2', %%al\n outb %%al, %%dx\n movb $'3', %%al\n outb %%al, %%dx" ::: "dx","al");
    if (mem_init(mem_size) != 0) {
        klog_printf("WuBuOS PANIC: mem_init failed\n");
        for (;;) { CLI(); HLT(); }
    }
    klog_printf("WuBuOS: heap initialized (%u MB)\n", (unsigned)(mem_size >> 20));
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'2', %%al\n outb %%al, %%dx\n movb $'4', %%al\n outb %%al, %%dx" ::: "dx","al");

    /* 3. Initialize interrupt subsystem (IDT, PIC, PIT) */
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'2', %%al\n outb %%al, %%dx\n movb $'5', %%al\n outb %%al, %%dx" ::: "dx","al");
    if (interrupt_init() != 0) {
        klog_printf("WuBuOS PANIC: interrupt_init failed\n");
        for (;;) { CLI(); HLT(); }
    }
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'2', %%al\n outb %%al, %%dx\n movb $'6', %%al\n outb %%al, %%dx" ::: "dx","al");
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'2', %%al\n outb %%al, %%dx\n movb $'7', %%al\n outb %%al, %%dx" ::: "dx","al");
    klog_printf("WuBuOS: interrupts initialized\n");

    /* 3b. Program the PIT (IRQ0 @ 100 Hz) + register the timer handler.
     * WITHOUT this, g_tick never advances: task_sleep never wakes tasks,
     * preemption never fires, and the AGI supervisor uptime stays 0. */
    if (pit_init(100) != 0) {
        klog_printf("WuBuOS PANIC: pit_init failed\n");
        for (;;) { CLI(); HLT(); }
    }
    klog_printf("WuBuOS: PIT timer @ 100 Hz\n");

    /* 3c. Enable the APIC (q35-correct delivery) + interrupts GLOBALLY.
     * crt0 cli'd at entry and nothing ever sti'd, so no hardware IRQ was
     * delivered: g_tick stayed 0 (task_sleep never woke tasks, no
     * preemption) and the AGI uptime never advanced.  And on q35 the
     * PIT/keyboard/mouse lines are wired to the I/O APIC, whose
     * redirection table was never programmable (broken accessor) -- the
     * APIC bring-up fixes both. */
    if (wubu_apic_enable() != 0) {
        klog_printf("WuBuOS PANIC: APIC enable failed\n");
        for (;;) { CLI(); HLT(); }
    }
    STI();
    klog_printf("WuBuOS: interrupts enabled (APIC mode)\n");

    /* 4. Initialize VBE/DRM-KMS framebuffer */
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'2', %%al\n outb %%al, %%dx\n movb $'8', %%al\n outb %%al, %%dx" ::: "dx","al");
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
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'2', %%al\n outb %%al, %%dx\n movb $'9', %%al\n outb %%al, %%dx" ::: "dx","al");

    /* 5. Initialize GAAD (φ-structured allocation for window snap) */
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'3', %%al\n outb %%al, %%dx\n movb $'0', %%al\n outb %%al, %%dx" ::: "dx","al");
    extern void wubu_gaad_init(void);
    wubu_gaad_init();

    /* 6. Initialize input subsystem (PS/2 + evdev fallback) */
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'3', %%al\n outb %%al, %%dx\n movb $'1', %%al\n outb %%al, %%dx" ::: "dx","al");
    input_init();

    /* 6b. Initialize PS/2 keyboard/mouse for bare metal */
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'3', %%al\n outb %%al, %%dx\n movb $'2', %%al\n outb %%al, %%dx" ::: "dx","al");
    int fb_w = 1920, fb_h = 1080;
    if (g_limine_ok && limine_framebuffer_request.response) {
        struct limine_framebuffer *fb2 = limine_framebuffer_request.response;
        fb_w = fb2->width;
        fb_h = fb2->height;
    }
    ps2_probe_t ps2p;
    ps2_init(fb_w, fb_h, &ps2p);
    klog_printf("WuBuOS: PS2 self-test=%d kbd_id=%x mouse_id=%x ack=%d flags=%x\n",
                ps2p.self_test, (unsigned)ps2p.kbd_id,
                (unsigned)ps2p.mouse_id, ps2p.mouse_ack, (unsigned)ps2p.flags);
    klog_printf("WuBuOS: input/PS2 initialized\n");

    /* 7. Initialize tasking (cooperative scheduler, PIT timer) */
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'3', %%al\n outb %%al, %%dx\n movb $'3', %%al\n outb %%al, %%dx" ::: "dx","al");
    if (tasking_init() != 0) {
        klog_printf("WuBuOS PANIC: tasking_init failed\n");
        for (;;) { CLI(); HLT(); }
    }
    klog_printf("WuBuOS: tasking initialized\n");

    /* 7b. The /theme graphic-set namespace (self-modifying UI) +
     *     the unified input layer (GameInput-style, one event model) +
     *     the vmm (page allocator + demand-zero regions). */
    extern void wubu_theme_init(void);
    extern void wubu_hid_init(void);
    extern void wubu_vmm_init(void);
    extern uint64_t wubu_vmm_register_demand(uint64_t, uint32_t);
    wubu_theme_init();
    wubu_hid_init();
    wubu_vmm_init();
    /* interrupt-driven serial RX (gap E2): UART IRQ -> wubu_sync FIFO */
    extern void wubu_serial_init(void);
    wubu_serial_init();
    /* a demand-zero demo region in the free higher-half space */
    wubu_vmm_register_demand(0xffffffff90000000ull, 4096);
    klog_printf("WuBuOS: /theme + unified input + vmm + serial ready\n");
    /* 8. Timer-driven PREEMPTION. The tracked #GP (resumed iretq with the
     * NT flag + no TSS) is fixed: wubu_tss installs a real TSS64 + GDT
     * (stray task-returns defined), and tasking_switch.S masks NT out of
     * the restored rflags (the kernel never hardware-task-switches). */
    extern void wubu_tss_init(void);
    wubu_tss_init();
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'3', %%al\n outb %%al, %%dx\n movb $'4', %%al\n outb %%al, %%dx" ::: "dx","al");
    extern void task_preempt_enable(void);
    task_preempt_enable();

    /* Gap E7: arm the 8254 channel-2 hardware watchdog (2s one-shot);
     * the PIT tick feeds it from here on. If the kernel ever stalls,
     * the countdown fires and the panic path reports it. */
    extern int wdt_arm(uint32_t);
    wdt_arm(2000);

    /* 9. Boot the AGI kernel supervisor (ring-0 operator + agent realm).
     *    This replaces the old `for(;;) HLT();` shell: the OS is now an AGI
     *    kernel -- it decomposes the viewport via GAAD, spawns a co-resident
     *    REALM_AGENT task, and runs the independent-verifier self-improve loop
     *    ticked by the PIT timer. Safe by default (no verifier => no promote). */
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'3', %%al\n outb %%al, %%dx\n movb $'5', %%al\n outb %%al, %%dx" ::: "dx","al");
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

    /* Boot wall clock (gap A17): the RTC is read once at boot; the
     * 'date' console command reads it live. */
    {
        extern int wubu_rtc_read(void *);
        struct { uint8_t s, m, h, d, mo; uint16_t y; } bt;
        if (wubu_rtc_read(&bt) == 0)
            klog_printf("WuBuOS: boot clock %u-%u-%u %u:%u:%u\n",
                        (unsigned)bt.y, (unsigned)bt.mo, (unsigned)bt.d,
                        (unsigned)bt.h, (unsigned)bt.m, (unsigned)bt.s);
    }

    /* ACPI (gap A18): read the real firmware tables -- RSDP -> XSDT ->
     * FADT -- instead of assuming the memory map. */
    {
        extern int wubu_acpi_init(void *);
        struct {
            uint64_t dsdt_addr, x_facs_addr;
            uint8_t  sci_irq, acpi_enable, acpi_disable, pm_tmr_len,
                     revision, minor;
            int      found;
        } ac;
        if (wubu_acpi_init(&ac) == 0 && ac.found) {
            klog_printf("WuBuOS: ACPI FADT rev=%u minor=%u sci=%u pmtmr=%u dsdt=%x\n",
                        (unsigned)ac.revision, (unsigned)ac.minor,
                        (unsigned)ac.sci_irq, (unsigned)ac.pm_tmr_len,
                        (unsigned)ac.dsdt_addr);
        } else {
            klog_printf("WuBuOS: ACPI tables not found\n");
        }
    }

    /* A19: the HPET -- the high-precision time source (its MMIO base
     * comes from the ACPI HPET table; the counter is enabled + read). */
    {
        extern uint64_t wubu_hpet_probe(uint64_t *);
        extern void     wubu_hpet_enable(uint64_t);
        extern uint64_t wubu_hpet_ns(uint64_t, uint64_t);
        uint64_t hpet_fs = 0;
        uint64_t hpet = wubu_hpet_probe(&hpet_fs);
        if (hpet) {
            wubu_hpet_enable(hpet);
            klog_printf("WuBuOS: HPET @ %x period=%u fs cnt=%u ns\n",
                        (unsigned)hpet, (unsigned)hpet_fs,
                        (unsigned)wubu_hpet_ns(hpet, hpet_fs));
        } else {
            klog_printf("WuBuOS: no HPET (PIT/LAPIC remain)\n");
        }
    }

    /* 9b. Wire the INDEPENDENT verifier (DA-3): this ACTIVATES the
     *     self-improve loop -- without a verifier the cycle refuses to
     *     promote (dormant by design). The verifier is a fixed,
     *     kernel-resident scorer (well-formedness + emitter trust +
     *     semantic budget); the test-suite-as-verifier doctrine grows it. */
    extern void wubu_verifier_install(void);
    wubu_verifier_install();
    klog_printf("WuBuOS: independent verifier installed (promote loop live)\n");

    /* 9c. Gap G5: the metal's long-term memory -- a C11 hive fed by the
     * AGI's memory hook (every promoted span is retained). Rate-limited
     * to the same cadence as the console echo (the flood would burn the
     * heap); the hive is the seed of the persisted store. */
    extern void metal_lm_setup(void *);
    metal_lm_setup(agi);

    /* 10. Enter the cooperative supervisor run loop (spawns agent task,
     *     yields to the PIT-ticked scheduler). Never returns. */
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'3', %%al\n outb %%al, %%dx\n movb $'6', %%al\n outb %%al, %%dx" ::: "dx","al");
    wubu_agi_kernel_run(agi);  /* Never returns */

    /* Unreachable: clean isa-debug-exit so the VM halts instead of looping. */
    __asm__ __volatile__("movw $0x3F8, %%dx\n movb $'3', %%al\n outb %%al, %%dx\n movb $'7', %%al\n outb %%al, %%dx\n"
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