/*
 * fw_acpiload.c  --  Execute QEMU's etc/table-loader linker script.
 *
 * The loader is a sequence of fixed-size commands describing how to build
 * the ACPI tables in guest memory:
 *
 *   ALLOCATE      : read a fw_cfg blob into an allocation with a given
 *                   alignment and zone (high memory or F-segment)
 *   ADD_POINTER   : patch a pointer field in one blob so it points at
 *                   another blob's final address
 *   ADD_CHECKSUM  : recompute a one-byte checksum over a range
 *   WRITE_POINTER : write a blob address back out to a fw_cfg file
 *
 * Executing this correctly is what produces a working RSDP/XSDT/MCFG/TPM2 —
 * skipping it (the usual shortcut) leaves the guest with no ACPI at all.
 */

#include "fw.h"
#include "fw_fwcfg.h"
#include "fw_acpi.h"

#define CMD_ALLOCATE      1
#define CMD_ADD_POINTER   2
#define CMD_ADD_CHECKSUM  3
#define CMD_WRITE_POINTER 4

#define ZONE_HIGH 1
#define ZONE_FSEG 2

typedef struct __attribute__((packed)) {
    uint32_t type;
    union {
        struct __attribute__((packed)) {
            char     file[56];
            uint32_t align;
            uint8_t  zone;
            uint8_t  pad[63];
        } allocate;
        struct __attribute__((packed)) {
            char     dest_file[56];
            char     src_file[56];
            uint32_t offset;
            uint8_t  size;
            uint8_t  pad[7];
        } pointer;
        struct __attribute__((packed)) {
            char     file[56];
            uint32_t offset;
            uint32_t start;
            uint32_t length;
            uint8_t  pad[56];
        } checksum;
        uint8_t raw[124];
    } u;
} loader_cmd;

typedef struct {
    char     name[56];
    uint8_t *addr;
    uint32_t size;
} blob;

#define MAX_BLOBS 16
static blob     g_blobs[MAX_BLOBS];
static uint32_t g_nblobs;

static blob *blob_find(const char *name) {
    for (uint32_t i = 0; i < g_nblobs; i++)
        if (fw_strcmp(g_blobs[i].name, name) == 0) return &g_blobs[i];
    return NULL;
}

static int do_allocate(const loader_cmd *c) {
    if (g_nblobs >= MAX_BLOBS) return -1;
    uint32_t size = fw_cfg_file_size(c->u.allocate.file);
    if (!size) return -1;

    uint32_t align = c->u.allocate.align ? c->u.allocate.align : 16;
    size_t pages = (size + align + 4095) / 4096;
    uint8_t *mem = fw_alloc_pages_aligned(pages ? pages : 1,
                                          align < 4096 ? 4096 : align);
    if (!mem) return -1;
    fw_memset(mem, 0, pages * 4096);

    uint32_t got = 0;
    if (fw_cfg_read_file(c->u.allocate.file, mem, size, &got) != 0) return -1;

    blob *b = &g_blobs[g_nblobs++];
    fw_memcpy(b->name, c->u.allocate.file, 56);
    b->addr = mem;
    b->size = got;
    return 0;
}

static int do_add_pointer(const loader_cmd *c) {
    blob *dst = blob_find(c->u.pointer.dest_file);
    blob *src = blob_find(c->u.pointer.src_file);
    if (!dst || !src) return -1;
    uint8_t sz = c->u.pointer.size;
    if (sz != 1 && sz != 2 && sz != 4 && sz != 8) return -1;
    if (c->u.pointer.offset + sz > dst->size) return -1;

    /* The field holds an offset into the source blob; add its base. */
    uint64_t cur = 0;
    fw_memcpy(&cur, dst->addr + c->u.pointer.offset, sz);
    uint64_t val = (uint64_t)(uintptr_t)src->addr + cur;
    fw_memcpy(dst->addr + c->u.pointer.offset, &val, sz);
    return 0;
}

static int do_add_checksum(const loader_cmd *c) {
    blob *b = blob_find(c->u.checksum.file);
    if (!b) return -1;
    if (c->u.checksum.start + c->u.checksum.length > b->size) return -1;
    if (c->u.checksum.offset >= b->size) return -1;

    b->addr[c->u.checksum.offset] = 0;
    uint8_t sum = 0;
    for (uint32_t i = 0; i < c->u.checksum.length; i++)
        sum = (uint8_t)(sum + b->addr[c->u.checksum.start + i]);
    b->addr[c->u.checksum.offset] = (uint8_t)(0x100 - sum);
    return 0;
}

/* Returns a pointer to the built RSDP, or NULL. */
void *fw_acpi_load_from_fwcfg(void) {
    static uint8_t script[64 * 1024];
    uint32_t len = 0;

    if (fw_cfg_read_file("etc/table-loader", script, sizeof(script), &len) != 0) {
        fw_puts("[acpi] no etc/table-loader in fw_cfg\n");
        return NULL;
    }

    g_nblobs = 0;
    uint32_t ncmd = len / sizeof(loader_cmd);
    uint32_t done = 0, failed = 0;

    for (uint32_t i = 0; i < ncmd; i++) {
        const loader_cmd *c = (const loader_cmd *)(script + i * sizeof(loader_cmd));
        int rc = 0;
        switch (c->type) {
        case CMD_ALLOCATE:      rc = do_allocate(c);     break;
        case CMD_ADD_POINTER:   rc = do_add_pointer(c);  break;
        case CMD_ADD_CHECKSUM:  rc = do_add_checksum(c); break;
        case CMD_WRITE_POINTER: rc = 0; break;   /* only needed for NVDIMM/VMGENID */
        case 0:                 continue;        /* padding */
        default:                rc = 0; break;   /* unknown: ignore, per spec */
        }
        if (rc == 0) done++; else failed++;
    }

    fw_printf("[acpi] table-loader: %u commands ok, %u failed, %u blobs\n",
              done, failed, g_nblobs);

    /* The RSDP blob is conventionally etc/acpi/rsdp. */
    blob *rsdp = blob_find("etc/acpi/rsdp");
    if (!rsdp) {
        for (uint32_t i = 0; i < g_nblobs; i++)
            if (fw_memcmp(g_blobs[i].addr, "RSD PTR ", 8) == 0) { rsdp = &g_blobs[i]; break; }
    }
    if (!rsdp) { fw_puts("[acpi] loader produced no RSDP\n"); return NULL; }

    fw_printf("[acpi] RSDP built at %p\n", (void *)rsdp->addr);
    return rsdp->addr;
}
