/* vbe_probe.c -- minimal multiboot payload: set a Bochs VBE mode and halt.
 *
 * Purpose: answer one question in isolation. Every other component (EFI
 * firmware, chainloader, kernel) is removed. If QEMU's display switches to
 * 1024x768 here, mode switching works and the defect is in our sequence. If it
 * stays 640x480, QEMU in this configuration cannot switch and no OS code can.
 *
 * Build: gcc -m32 -ffreestanding -nostdlib -Wl,-Ttext=0x100000 \
 *            -o build/vbe_probe.elf vbe_probe.c
 * Run:   qemu-system-i386 -kernel build/vbe_probe.elf -display vnc=:3
 */
typedef unsigned char u8;
typedef unsigned short u16;

#define MB_MAGIC 0x1BADB002u
#define MB_FLAGS 0x00000003u          /* page-align modules + provide meminfo */
#define MB_CHECKSUM (-(MB_MAGIC + MB_FLAGS))

__attribute__((section(".multiboot"), used))
static const unsigned int mb_header[3] = { MB_MAGIC, MB_FLAGS, MB_CHECKSUM };

static inline void outw_p(u16 port, u16 val)
{
    __asm__ __volatile__("outw %0, %1" :: "a"(val), "Nd"(port));
}

static inline u16 inw_p(u16 port)
{
    u16 v;
    __asm__ __volatile__("inw %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline void outb_p(u16 port, u8 val)
{
    __asm__ __volatile__("outb %0, %1" :: "a"(val), "Nd"(port));
}

static inline u8 inb_p(u16 port)
{
    u8 v;
    __asm__ __volatile__("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static void dispi(u16 idx, u16 val)
{
    outw_p(0x1CE, idx);
    outw_p(0x1CF, val);
}

void _start(void)
{
    /* Sequencer unlock + CRTC protection clear, as the kernel does. */
    outb_p(0x3C4, 0x01);
    outb_p(0x3C5, (u8)(inb_p(0x3C5) & ~0x20));
    outb_p(0x3D4, 0x11);
    outb_p(0x3D5, (u8)(inb_p(0x3D5) & ~0x80));

    dispi(4, 0x00);                 /* disable */
    dispi(1, 1024);
    dispi(2, 768);
    dispi(3, 32);
    dispi(6, 1024);
    dispi(7, 768);
    dispi(4, 0x41);                 /* enable + LFB */

    /* Paint the standard QEMU LFB so a successful switch is unmistakable. */
    {
        volatile unsigned int *fb = (volatile unsigned int *)0xFD000000u;
        unsigned int i;
        for (i = 0; i < 1024u * 768u; i++)
            fb[i] = 0x0020C060u;
    }

    /* Report the mode on the debug port so the serial log carries it. */
    outw_p(0x1CE, 1);
    outw_p(0xE9, inw_p(0x1CF));
    outw_p(0x1CE, 4);
    outw_p(0xE9, inw_p(0x1CF));
    outb_p(0xE9, '\n');

    for (;;)
        __asm__ __volatile__("hlt");
}
