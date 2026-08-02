/*
 * fw_drivers.c  --  WuBuFW driver manager and boot-stage measurement.
 *
 * Binds real drivers to the devices PCI enumeration found, and performs the
 * TCG-required measurements that are not tied to a single stage (GPT into
 * PCR5, secure-boot policy into PCR7).
 */

#include "fw.h"
#include "fw_pci.h"
#include "fw_tpm.h"
#include "fw_block.h"

int  fw_ahci_init(fw_pci_dev *d);
int  fw_e1000_init(fw_pci_dev *d);
int  fw_nvme_init(fw_pci_dev *d);
int  fw_xhci_init(fw_pci_dev *d);
int  fw_gop_init(fw_pci_dev *d);

static int g_bound;

static void bind_class(uint8_t cls, uint8_t sub, int8_t progif,
                       int (*probe)(fw_pci_dev *), const char *what) {
    for (int nth = 0; nth < 8; nth++) {
        fw_pci_dev *d = fw_pci_find_class(cls, sub, progif, nth);
        if (!d) break;
        fw_pci_enable(d, 1);
        if (probe(d) == 0) {
            g_bound++;
            fw_printf("[drv] bound %s at %d:%d.%d\n", what, d->bus, d->dev, d->fn);
        }
    }
}

int fw_drivers_init(void) {
    g_bound = 0;
    bind_class(0x01, 0x06, 0x01, fw_ahci_init, "AHCI");
    bind_class(0x01, 0x08, 0x02, fw_nvme_init, "NVMe");
    bind_class(0x0C, 0x03, 0x30, fw_xhci_init, "XHCI");
    bind_class(0x02, 0x00, -1,   fw_e1000_init, "e1000");
    bind_class(0x03, 0x00, -1,   fw_gop_init,  "GPU/GOP");
    fw_printf("[drv] %d driver(s) bound\n", g_bound);
    return g_bound;
}

/*
 * Measure the GPT into PCR5. The TCG profile hashes the protective MBR's
 * partition-table region plus the GPT header and the *used* partition
 * entries, so an attacker adding a partition changes the measurement.
 */
void fw_measure_gpt(void) {
    if (fw_block_count() == 0) return;

    static uint8_t buf[512 * 34];
    if (fw_block_read(0, 0, 34, buf) != 0) {
        fw_puts("[measure] GPT read failed; PCR5 not extended\n");
        return;
    }

    /* Hash header (LBA1) + entries (LBA2..33): the exact bytes an OS or an
     * attestation verifier can re-read from the same disk. */
    if (fw_memcmp(buf + 512, "EFI PART", 8) == 0) {
        fw_tpm_measure(PCR_BOOT_CONFIG, EV_EFI_GPT_EVENT,
                       buf + 512, 512 * 33, "GPT header + entries");
    } else {
        fw_tpm_measure(PCR_BOOT_CONFIG, EV_EFI_GPT_EVENT,
                       buf, 512, "MBR partition table");
    }
}

/*
 * PCR7 records the Secure Boot policy. Even with verification disabled the
 * policy state itself must be measured, otherwise "SecureBoot off" and
 * "SecureBoot on" would produce identical PCRs.
 */
void fw_measure_secureboot(int enabled, int setup_mode) {
    uint8_t state[2] = { (uint8_t)(enabled ? 1 : 0), (uint8_t)(setup_mode ? 1 : 0) };
    fw_tpm_measure(PCR_SECURE_BOOT, EV_EFI_VARIABLE_DRIVER_CONFIG,
                   state, sizeof(state),
                   enabled ? "SecureBoot=1" : "SecureBoot=0");
}
