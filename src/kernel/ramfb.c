/* ramfb.c -- universal QEMU display via fw_cfg "etc/ramfb".
 *
 * Why this exists: programming VGA/Bochs-dispi requires the card to be in a
 * sane POST state, which depends on whether the firmware ran the VGA option
 * ROM. That makes the VBE path machine-specific. ramfb sidesteps the whole
 * question: QEMU is told "here is a buffer, scan it out", and the buffer is
 * ordinary RAM. It works on x86, ARM and riscv guests alike.
 *
 * Interface (QEMU fw_cfg spec + OSDev Ramfb):
 *   struct RAMFBCfg { u64 addr; u32 fourcc; u32 flags;
 *                     u32 width; u32 height; u32 stride; }   (big endian)
 *
 * Access here uses the legacy I/O port interface (0x510 selector, 0x511 data),
 * which every x86 QEMU provides; no DMA needed for a single 28-byte write.
 */
#include <stdint.h>
#include "interrupt_io.h"
#include "klog.h"

#define FWCFG_SEL   0x510
#define FWCFG_DATA  0x511

#define FOURCC_XR24 0x34325258u   /* 'X','R','2','4' little-endian */

/* A static 3 MB array pushed the kernel image past 0x400000 into the heap
 * (heap starts at 0x400000), which broke boot. The buffer therefore lives at a
 * fixed free physical address above the heap window:
 *   kernel image   ~0x100000..0x300000
 *   heap (mem_alloc) 0x400000..0x4400000  (4 MB..68 MB)
 *   ramfb          0x5000000            (80 MB), inside the 128 MB warm window
 * Identity-mapped, so the virtual address equals the physical one. */
#define RAMFB_PHYS 0x05000000u
static uint8_t *const g_ramfb_buf = (uint8_t *)(uintptr_t)RAMFB_PHYS;

typedef struct {
    uint64_t addr;
    uint32_t fourcc;
    uint32_t flags;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
} ramfb_cfg_t;

static void fwcfg_select(uint16_t sel)
{
    outw(FWCFG_SEL, sel);
}

static uint8_t fwcfg_read8(void)
{
    return inb(FWCFG_DATA);
}

static void fwcfg_write8(uint8_t v)
{
    outb(FWCFG_DATA, v);
}

static uint32_t rd_be32(void)
{
    uint32_t v = 0;
    for (int i = 0; i < 4; i++)
        v = (v << 8) | fwcfg_read8();
    return v;
}

static uint16_t rd_be16(void)
{
    uint16_t v = 0;
    for (int i = 0; i < 2; i++)
        v = (uint16_t)((v << 8) | fwcfg_read8());
    return v;
}

static void wr_be32(uint32_t v)
{
    for (int i = 3; i >= 0; i--)
        fwcfg_write8((uint8_t)(v >> (8 * i)));
}

static void wr_be64(uint64_t v)
{
    for (int i = 7; i >= 0; i--)
        fwcfg_write8((uint8_t)(v >> (8 * i)));
}

/* Returns 1 if fw_cfg answers with the QEMU signature. */
static int fwcfg_probe(void)
{
    fwcfg_select(0);
    char sig[4];
    for (int i = 0; i < 4; i++)
        sig[i] = (char)fwcfg_read8();
    return sig[0] == 'Q' && sig[1] == 'E' && sig[2] == 'M' && sig[3] == 'U';
}

/* Find the selector of "etc/ramfb" in the fw_cfg file directory. */
static uint16_t fwcfg_find(const char *name)
{
    fwcfg_select(0x19);
    uint32_t count = rd_be32();
    if (count == 0 || count > 4096)
        return 0;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t size = rd_be32();
        uint16_t sel  = rd_be16();
        (void)rd_be16();                     /* reserved */
        char fname[57];
        for (int j = 0; j < 56; j++)
            fname[j] = (char)fwcfg_read8();
        fname[56] = '\0';
        (void)size;
        if (sel && fname[0]) {
            int match = 1;
            for (int j = 0; name[j]; j++)
                if (fname[j] != name[j]) { match = 0; break; }
            if (match && fname[0] != '\0')
                return sel;
        }
    }
    return 0;
}

/* Set up ramfb scanning out g_ramfb_buf. Returns 1 on success. */
int wubu_ramfb_init(uint32_t width, uint32_t height)
{
    if (!fwcfg_probe())
        return 0;

    uint16_t sel = fwcfg_find("etc/ramfb");
    if (!sel) {
        klog_printf("WuBuOS: fw_cfg present, no etc/ramfb (add -device ramfb)\n");
        return 0;
    }

    ramfb_cfg_t cfg;
    cfg.addr   = (uint64_t)(uintptr_t)g_ramfb_buf;
    cfg.fourcc = FOURCC_XR24;
    cfg.flags  = 0;
    cfg.width  = width;
    cfg.height = height;
    cfg.stride = width * 4;

    fwcfg_select(sel);
    wr_be64(cfg.addr);
    wr_be32(cfg.fourcc);
    wr_be32(cfg.flags);
    wr_be32(cfg.width);
    wr_be32(cfg.height);
    wr_be32(cfg.stride);

    klog_printf("WuBuOS: ramfb configured %ux%u at 0x%x (stride %u)\n",
                (unsigned)width, (unsigned)height,
                (unsigned)(uintptr_t)g_ramfb_buf, (unsigned)cfg.stride);
    return 1;
}

uint32_t *wubu_ramfb_buffer(void)
{
    return (uint32_t *)g_ramfb_buf;
}
