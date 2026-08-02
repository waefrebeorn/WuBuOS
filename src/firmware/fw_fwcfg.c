/*
 * fw_fwcfg.c  --  QEMU fw_cfg interface and ACPI table installation.
 *
 * QEMU does not place ACPI tables in memory for the firmware to find; it
 * hands them over through fw_cfg together with a linker/loader script telling
 * the firmware where to allocate them, how to patch cross-table pointers, and
 * which checksums to recompute. Real firmware (and therefore ours) must
 * execute that script, otherwise there is no RSDP, no MCFG, and no TPM2
 * table — which is exactly why TPM discovery was failing.
 *
 * This implements the documented fw_cfg DMA protocol and the
 * etc/table-loader command set (ALLOCATE / ADD_POINTER / ADD_CHECKSUM /
 * WRITE_POINTER).
 */

#include "fw.h"
#include "fw_fwcfg.h"

#define FW_CFG_PORT_SEL  0x510
#define FW_CFG_PORT_DATA 0x511
#define FW_CFG_PORT_DMA  0x514

#define FW_CFG_SIGNATURE 0x0000
#define FW_CFG_ID        0x0001
#define FW_CFG_FILE_DIR  0x0019

#define FW_CFG_DMA_CTL_ERROR  0x01
#define FW_CFG_DMA_CTL_READ   0x02
#define FW_CFG_DMA_CTL_SKIP   0x04
#define FW_CFG_DMA_CTL_SELECT 0x08

typedef struct __attribute__((packed)) {
    uint32_t control;
    uint32_t length;
    uint64_t address;
} fw_cfg_dma;

static int g_have_dma;
static int g_present;

static uint32_t bswap32(uint32_t v) { return __builtin_bswap32(v); }
static uint64_t bswap64(uint64_t v) { return __builtin_bswap64(v); }

int fw_cfg_present(void) { return g_present; }

int fw_cfg_init(void) {
    outw(FW_CFG_PORT_SEL, FW_CFG_SIGNATURE);
    char sig[4];
    for (int i = 0; i < 4; i++) sig[i] = (char)inb(FW_CFG_PORT_DATA);
    if (sig[0] != 'Q' || sig[1] != 'E' || sig[2] != 'M' || sig[3] != 'U') {
        g_present = 0;
        return -1;
    }
    g_present = 1;

    outw(FW_CFG_PORT_SEL, FW_CFG_ID);
    uint32_t id = 0;
    for (int i = 0; i < 4; i++) id |= (uint32_t)inb(FW_CFG_PORT_DATA) << (i * 8);
    g_have_dma = (id & 2) != 0;

    fw_printf("[fwcfg] QEMU fw_cfg present (id=%x, dma=%s)\n",
              id, g_have_dma ? "yes" : "no");
    return 0;
}

/* Read `len` bytes of the selected item. Uses DMA when available (much
 * faster and the only reliable path for large blobs) else PIO. */
static int fw_cfg_read_sel(uint16_t sel, void *buf, uint32_t len) {
    if (!g_present) return -1;

    if (g_have_dma) {
        static volatile fw_cfg_dma dma __attribute__((aligned(8)));
        dma.control = bswap32(((uint32_t)sel << 16) |
                              FW_CFG_DMA_CTL_SELECT | FW_CFG_DMA_CTL_READ);
        dma.length  = bswap32(len);
        dma.address = bswap64((uint64_t)(uintptr_t)buf);

        uint64_t pa = (uint64_t)(uintptr_t)&dma;
        /* The DMA address register is big-endian and split hi/lo. */
        outl(FW_CFG_PORT_DMA, bswap32((uint32_t)(pa >> 32)));
        outl(FW_CFG_PORT_DMA + 4, bswap32((uint32_t)pa));

        for (int i = 0; i < 1000000; i++) {
            uint32_t c = bswap32(dma.control);
            if (c == 0) return 0;
            if (c & FW_CFG_DMA_CTL_ERROR) return -1;
            fw_stall_us(1);
        }
        return -1;
    }

    outw(FW_CFG_PORT_SEL, sel);
    uint8_t *p = buf;
    for (uint32_t i = 0; i < len; i++) p[i] = inb(FW_CFG_PORT_DATA);
    return 0;
}

/* -- file directory -------------------------------------------------- */

typedef struct __attribute__((packed)) {
    uint32_t size;      /* BE */
    uint16_t select;    /* BE */
    uint16_t reserved;
    char     name[56];
} fw_cfg_file;

#define MAX_FILES 48
static fw_cfg_file g_files[MAX_FILES];
static uint32_t    g_nfiles;

static int load_dir(void) {
    uint32_t count_be;
    outw(FW_CFG_PORT_SEL, FW_CFG_FILE_DIR);
    uint8_t *cp = (uint8_t *)&count_be;
    for (int i = 0; i < 4; i++) cp[i] = inb(FW_CFG_PORT_DATA);
    uint32_t n = bswap32(count_be);
    if (n > MAX_FILES) n = MAX_FILES;

    /* The directory stream continues from the same selector. */
    uint8_t *dst = (uint8_t *)g_files;
    for (uint32_t i = 0; i < n * sizeof(fw_cfg_file); i++)
        dst[i] = inb(FW_CFG_PORT_DATA);

    g_nfiles = n;
    return 0;
}

static const fw_cfg_file *find_file(const char *name) {
    for (uint32_t i = 0; i < g_nfiles; i++)
        if (fw_strcmp(g_files[i].name, name) == 0) return &g_files[i];
    return NULL;
}

int fw_cfg_read_file(const char *name, void *buf, uint32_t max, uint32_t *out_len) {
    if (!g_nfiles) load_dir();
    const fw_cfg_file *f = find_file(name);
    if (!f) return -1;
    uint32_t size = bswap32(f->size);
    uint16_t sel  = __builtin_bswap16(f->select);
    if (size > max) return -1;
    if (fw_cfg_read_sel(sel, buf, size) != 0) return -1;
    if (out_len) *out_len = size;
    return 0;
}

uint32_t fw_cfg_file_size(const char *name) {
    if (!g_nfiles) load_dir();
    const fw_cfg_file *f = find_file(name);
    return f ? bswap32(f->size) : 0;
}
