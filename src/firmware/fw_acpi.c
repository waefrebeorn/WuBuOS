/*
 * fw_acpi.c  --  WuBuFW ACPI table discovery and publication.
 *
 * On a QEMU/x86 machine the firmware normally *builds* ACPI tables, but QEMU
 * generates them itself and hands them over through fw_cfg. We locate the
 * RSDP the platform placed in low memory, walk XSDT/RSDT, and:
 *   - pick up MCFG so PCI ECAM (extended config space) works,
 *   - pick up TPM2/TCPA so the TPM driver knows its interface and log,
 *   - publish the RSDP as an EFI configuration table so the OS finds it.
 */

#include "fw.h"
#include "fw_pci.h"
#include "fw_acpi.h"

static fw_acpi_rsdp *g_rsdp;
static uint64_t g_tpm2_control_area;
static uint32_t g_tpm2_start_method;
static uint64_t g_tpm_log_addr;
static uint64_t g_tpm_log_size;

static int sum_ok(const void *p, size_t n) {
    const uint8_t *b = p;
    uint8_t s = 0;
    for (size_t i = 0; i < n; i++) s = (uint8_t)(s + b[i]);
    return s == 0;
}

static fw_acpi_rsdp *scan_rsdp(uint64_t start, uint64_t end) {
    for (uint64_t a = start; a + 20 <= end; a += 16) {
        fw_acpi_rsdp *r = (fw_acpi_rsdp *)fw_phys((uintptr_t)a);
        if (fw_memcmp(r->signature, "RSD PTR ", 8) != 0) continue;
        if (!sum_ok(r, 20)) continue;
        if (r->revision >= 2 && r->length >= 33 && !sum_ok(r, r->length)) continue;
        return r;
    }
    return NULL;
}

fw_acpi_hdr *fw_acpi_find(const char *sig) {
    if (!g_rsdp) return NULL;

    if (g_rsdp->revision >= 2 && g_rsdp->xsdt_addr) {
        fw_acpi_hdr *xsdt = (fw_acpi_hdr *)(uintptr_t)g_rsdp->xsdt_addr;
        if (fw_memcmp(xsdt->signature, "XSDT", 4) == 0) {
            uint32_t n = (xsdt->length - sizeof(fw_acpi_hdr)) / 8;
            const uint8_t *ents = (const uint8_t *)(xsdt + 1);
            for (uint32_t i = 0; i < n; i++) {
                uint64_t a;
                fw_memcpy(&a, ents + i * 8, 8);     /* XSDT entries are unaligned */
                fw_acpi_hdr *h = (fw_acpi_hdr *)(uintptr_t)a;
                if (h && fw_memcmp(h->signature, sig, 4) == 0) return h;
            }
        }
    }
    if (g_rsdp->rsdt_addr) {
        fw_acpi_hdr *rsdt = (fw_acpi_hdr *)(uintptr_t)(uint64_t)g_rsdp->rsdt_addr;
        if (fw_memcmp(rsdt->signature, "RSDT", 4) == 0) {
            uint32_t n = (rsdt->length - sizeof(fw_acpi_hdr)) / 4;
            const uint32_t *ents = (const uint32_t *)(rsdt + 1);
            for (uint32_t i = 0; i < n; i++) {
                fw_acpi_hdr *h = (fw_acpi_hdr *)(uintptr_t)(uint64_t)ents[i];
                if (h && fw_memcmp(h->signature, sig, 4) == 0) return h;
            }
        }
    }
    return NULL;
}

void *fw_acpi_rsdp_ptr(void) { return g_rsdp; }

uint64_t fw_acpi_tpm2_control_area(void) { return g_tpm2_control_area; }
uint32_t fw_acpi_tpm2_start_method(void) { return g_tpm2_start_method; }
uint64_t fw_acpi_tpm_log_addr(void)      { return g_tpm_log_addr; }
uint64_t fw_acpi_tpm_log_size(void)      { return g_tpm_log_size; }

/* Adopt an RSDP built by the fw_cfg table-loader instead of scanning. */
void fw_acpi_set_rsdp(void *p) { g_rsdp = (fw_acpi_rsdp *)p; }

int fw_acpi_init(void) {
    /* EBDA first (segment at 0x40E), then the BIOS area. The pointer is
     * laundered through a volatile variable because GCC otherwise treats a
     * literal low address as a null-adjacent object and errors out. */
    if (!g_rsdp) {
        uint64_t ebda = (uint64_t)fw_read16_phys(0x40E) << 4;
        if (ebda >= 0x400 && ebda < 0xA0000) g_rsdp = scan_rsdp(ebda, ebda + 1024);
    }
    if (!g_rsdp) g_rsdp = scan_rsdp(0xE0000, 0x100000);
    if (!g_rsdp) { fw_puts("[acpi] no RSDP found\n"); return -1; }

    fw_printf("[acpi] RSDP at %p rev %d  OEM %c%c%c%c%c%c\n",
              (void *)g_rsdp, g_rsdp->revision,
              g_rsdp->oem_id[0], g_rsdp->oem_id[1], g_rsdp->oem_id[2],
              g_rsdp->oem_id[3], g_rsdp->oem_id[4], g_rsdp->oem_id[5]);

    /* MCFG -> PCI ECAM */
    fw_acpi_hdr *mcfg = fw_acpi_find("MCFG");
    if (mcfg && mcfg->length >= sizeof(fw_acpi_hdr) + 8 + 16) {
        const uint8_t *e = (const uint8_t *)mcfg + sizeof(fw_acpi_hdr) + 8;
        uint64_t base;
        fw_memcpy(&base, e, 8);
        uint8_t sbus = e[10], ebus = e[11];
        fw_pci_set_ecam(base, sbus, ebus);
        fw_printf("[acpi] MCFG ECAM base=0x%lx bus %d..%d\n", base, sbus, ebus);
    }

    /* TPM2 table: control area + start method (TCG ACPI spec). */
    fw_acpi_hdr *tpm2 = fw_acpi_find("TPM2");
    if (tpm2 && tpm2->length >= sizeof(fw_acpi_hdr) + 16) {
        const uint8_t *b = (const uint8_t *)tpm2 + sizeof(fw_acpi_hdr);
        fw_memcpy(&g_tpm2_start_method, b + 4, 4);
        fw_memcpy(&g_tpm2_control_area, b + 8, 8);
        fw_printf("[acpi] TPM2 start_method=%u control_area=0x%lx\n",
                  g_tpm2_start_method, g_tpm2_control_area);
        if (tpm2->length >= sizeof(fw_acpi_hdr) + 16 + 12) {
            fw_memcpy(&g_tpm_log_size, b + 16, 4);
            fw_memcpy(&g_tpm_log_addr, b + 20, 8);
        }
    }

    /* TCPA (TPM 1.2 / legacy log location) */
    fw_acpi_hdr *tcpa = fw_acpi_find("TCPA");
    if (tcpa && !g_tpm_log_addr && tcpa->length >= sizeof(fw_acpi_hdr) + 14) {
        const uint8_t *b = (const uint8_t *)tcpa + sizeof(fw_acpi_hdr);
        uint32_t laml;
        uint64_t lasa;
        fw_memcpy(&laml, b + 2, 4);
        fw_memcpy(&lasa, b + 6, 8);
        g_tpm_log_size = laml;
        g_tpm_log_addr = lasa;
        fw_printf("[acpi] TCPA log at 0x%lx (%lu bytes)\n", lasa, (uint64_t)laml);
    }

    return 0;
}
