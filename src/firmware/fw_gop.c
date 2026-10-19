/*
 * fw_gop.c  --  WuBuFW Graphics Output Protocol over a linear framebuffer.
 *
 * A gaming-console-class firmware must hand the OS a framebuffer, not a text
 * console. QEMU's std/virtio VGA exposes a linear framebuffer through BAR0
 * plus the Bochs VBE dispi registers, which is also what real GPUs provide
 * before their native driver loads. We program a mode, publish GOP, and let
 * the console mirror onto it.
 */

#include "fw.h"
#include "fw_pci.h"

#define VBE_DISPI_INDEX 0x01CE
#define VBE_DISPI_DATA  0x01CF

#define VBE_INDEX_ID        0
#define VBE_INDEX_XRES      1
#define VBE_INDEX_YRES      2
#define VBE_INDEX_BPP       3
#define VBE_INDEX_ENABLE    4
#define VBE_INDEX_VIRT_W    6
#define VBE_INDEX_VIRT_H    7

#define VBE_ENABLED     0x01
#define VBE_LFB_ENABLED 0x40

static EFI_GRAPHICS_OUTPUT_PROTOCOL g_gop;
static EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE g_gop_mode;
static EFI_GRAPHICS_OUTPUT_MODE_INFORMATION g_gop_info;
static uint32_t *g_fb;
static int g_have_gop;

/* Two dispi windows exist: the legacy I/O ports (0x1CE/0x1CF) and a MMIO
 * window inside the VGA's BAR2 at +0x500 (index) / +0x502 (data). QEMU accepts
 * writes on both and echoes them back on both, so "readback == written" does
 * not tell you which one the display actually follows. Try MMIO first: on
 * q35/std-VGA it is the window that drives the scanout. */
static volatile uint8_t *g_dispi_mmio;

static void vbe_write(uint16_t idx, uint16_t val) {
    if (g_dispi_mmio) {
        *(volatile uint16_t *)(g_dispi_mmio + 0x500) = idx;
        *(volatile uint16_t *)(g_dispi_mmio + 0x502) = val;
        return;
    }
    outw(VBE_DISPI_INDEX, idx);
    outw(VBE_DISPI_DATA, val);
}
static uint16_t vbe_read(uint16_t idx) {
    if (g_dispi_mmio) {
        *(volatile uint16_t *)(g_dispi_mmio + 0x500) = idx;
        return *(volatile uint16_t *)(g_dispi_mmio + 0x502);
    }
    outw(VBE_DISPI_INDEX, idx);
    return inw(VBE_DISPI_DATA);
}

static EFI_STATUS EFIAPI gop_query(EFI_GRAPHICS_OUTPUT_PROTOCOL *This, UINT32 mode,
                                   UINTN *size, EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **info) {
    (void)This;
    if (mode != 0 || !size || !info) return EFI_INVALID_PARAMETER;
    *size = sizeof(g_gop_info);
    *info = &g_gop_info;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI gop_set_mode(EFI_GRAPHICS_OUTPUT_PROTOCOL *This, UINT32 mode) {
    (void)This;
    return mode == 0 ? EFI_SUCCESS : EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI gop_blt(EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
                                 EFI_GRAPHICS_OUTPUT_BLT_PIXEL *buf,
                                 EFI_GRAPHICS_OUTPUT_BLT_OPERATION op,
                                 UINTN sx, UINTN sy, UINTN dx, UINTN dy,
                                 UINTN w, UINTN h, UINTN delta) {
    (void)This;
    if (!g_fb) return EFI_DEVICE_ERROR;
    UINTN stride = g_gop_info.PixelsPerScanLine;
    UINTN bstride = delta ? delta / 4 : w;

    switch (op) {
    case EfiBltVideoFill: {
        if (!buf) return EFI_INVALID_PARAMETER;
        uint32_t v = *(uint32_t *)buf;
        for (UINTN y = 0; y < h; y++)
            for (UINTN x = 0; x < w; x++)
                g_fb[(dy + y) * stride + dx + x] = v;
        return EFI_SUCCESS;
    }
    case EfiBltVideoToBltBuffer:
        if (!buf) return EFI_INVALID_PARAMETER;
        for (UINTN y = 0; y < h; y++)
            for (UINTN x = 0; x < w; x++)
                ((uint32_t *)buf)[(dy + y) * bstride + dx + x] =
                    g_fb[(sy + y) * stride + sx + x];
        return EFI_SUCCESS;
    case EfiBltBufferToVideo:
        if (!buf) return EFI_INVALID_PARAMETER;
        for (UINTN y = 0; y < h; y++)
            for (UINTN x = 0; x < w; x++)
                g_fb[(dy + y) * stride + dx + x] =
                    ((uint32_t *)buf)[(sy + y) * bstride + sx + x];
        return EFI_SUCCESS;
    case EfiBltVideoToVideo:
        /* Overlap-safe: copy bottom-up when moving down. */
        if (dy > sy) {
            for (UINTN y = h; y-- > 0;)
                for (UINTN x = 0; x < w; x++)
                    g_fb[(dy + y) * stride + dx + x] = g_fb[(sy + y) * stride + sx + x];
        } else {
            for (UINTN y = 0; y < h; y++)
                for (UINTN x = 0; x < w; x++)
                    g_fb[(dy + y) * stride + dx + x] = g_fb[(sy + y) * stride + sx + x];
        }
        return EFI_SUCCESS;
    default:
        return EFI_INVALID_PARAMETER;
    }
}

EFI_GRAPHICS_OUTPUT_PROTOCOL *fw_gop_get(void) { return g_have_gop ? &g_gop : NULL; }

int fw_gop_init(fw_pci_dev *d) {
    if (!d) return -1;

    /* BAR2 (non-prefetchable MMIO, >= 0x600) carries the dispi window. */
    for (int i = 0; i < 6; i++)
        if (!d->bar[i].is_io && d->bar[i].addr && d->bar[i].size >= 0x600 &&
            d->bar[i].size <= 0x10000) {
            g_dispi_mmio = (volatile uint8_t *)(uintptr_t)d->bar[i].addr;
            break;
        }

    uint64_t fb = 0;
    for (int i = 0; i < 6; i++)
        if (!d->bar[i].is_io && d->bar[i].size >= 0x100000) { fb = d->bar[i].addr; break; }
    if (!fb) return -1;

    /* Bochs VBE: present only when the ID register reads back a 0xB0Cx id. */
    uint16_t id = vbe_read(VBE_INDEX_ID);
    /* If the MMIO window does not answer with a Bochs id, this card only has
     * the legacy port window -- fall back rather than driving inert registers. */
    if (g_dispi_mmio && (id & 0xFFF0) != 0xB0C0) {
        fw_printf("[gop] MMIO dispi id=%x invalid, using ports\n", id);
        g_dispi_mmio = 0;
        id = vbe_read(VBE_INDEX_ID);
    }
    fw_printf("[gop] dispi via %s, id=%x\n", g_dispi_mmio ? "MMIO" : "ports", id);
    if ((id & 0xFFF0) != 0xB0C0) return -1;

    const uint16_t W = 1024, H = 768, BPP = 32;
    vbe_write(VBE_INDEX_ENABLE, 0);
    vbe_write(VBE_INDEX_XRES, W);
    vbe_write(VBE_INDEX_YRES, H);
    vbe_write(VBE_INDEX_BPP, BPP);
    vbe_write(VBE_INDEX_VIRT_W, W);
    vbe_write(VBE_INDEX_VIRT_H, H);
    vbe_write(VBE_INDEX_ENABLE, VBE_ENABLED | VBE_LFB_ENABLED);

    /* Read back the mode we just set: if this fails, the display never
     * switched and every later write goes to a surface nobody scans out. */
    fw_printf("[gop] readback xres=%u yres=%u bpp=%u enable=%u\n",
              vbe_read(VBE_INDEX_XRES), vbe_read(VBE_INDEX_YRES),
              vbe_read(VBE_INDEX_BPP), vbe_read(VBE_INDEX_ENABLE));
    fw_printf("[gop] bars: 0=%llx/0x%llx 1=%llx/0x%llx 2=%llx/0x%llx\n",
              (unsigned long long)d->bar[0].addr, (unsigned long long)d->bar[0].size,
              (unsigned long long)d->bar[1].addr, (unsigned long long)d->bar[1].size,
              (unsigned long long)d->bar[2].addr, (unsigned long long)d->bar[2].size);
    if (vbe_read(VBE_INDEX_XRES) != W) return -1;

    g_fb = (uint32_t *)(uintptr_t)fb;

    g_gop_info.Version = 0;
    g_gop_info.HorizontalResolution = W;
    g_gop_info.VerticalResolution = H;
    g_gop_info.PixelFormat = PixelBlueGreenRedReserved8BitPerColor;
    g_gop_info.PixelsPerScanLine = W;

    g_gop_mode.MaxMode = 1;
    g_gop_mode.Mode = 0;
    g_gop_mode.Info = &g_gop_info;
    g_gop_mode.SizeOfInfo = sizeof(g_gop_info);
    g_gop_mode.FrameBufferBase = fb;
    g_gop_mode.FrameBufferSize = (UINTN)W * H * 4;

    g_gop.QueryMode = gop_query;
    g_gop.SetMode   = gop_set_mode;
    g_gop.Blt       = gop_blt;
    g_gop.Mode      = &g_gop_mode;

    /* Clear to the WuBu deep blue so a live framebuffer is visibly proven. */
    for (UINTN i = 0; i < (UINTN)W * H; i++) g_fb[i] = 0x00201838;

    g_have_gop = 1;
    fw_printf("[gop] %ux%u x32 linear framebuffer at 0x%lx\n", W, H, fb);

    EFI_HANDLE h = fw_efi_new_handle();
    if (h) fw_efi_install(h, &gEfiGraphicsOutputProtocolGuid, &g_gop);
    return 0;
}

/* Accessor: the linear framebuffer the firmware programmed, if any. */
int fw_gop_fb(uint32_t **fb, uint32_t *w, uint32_t *h, uint32_t *pitch)
{
    if (!g_have_gop || !g_fb) return -1;
    if (fb)    *fb = g_fb;
    if (w)     *w = g_gop_info.HorizontalResolution;
    if (h)     *h = g_gop_info.VerticalResolution;
    if (pitch) *pitch = g_gop_info.PixelsPerScanLine * 4u;
    return 0;
}
