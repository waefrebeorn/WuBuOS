/*
 * ahci.c — WuBuOS AHCI (SATA) Disk Driver Implementation
 *
 * Cell 072: AHCI driver with hosted simulation mode.
 * In hosted mode, provides simulated SATA ports backed by
 * memory buffers for testing without real hardware.
 */

#include "ahci.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── Device type detection ─────────────────────────────────── */

ahci_dev_type_t ahci_dev_type_from_sig(uint32_t signature) {
    switch (signature) {
        case AHCI_SIG_SATA:   return AHCI_DEV_SATA;
        case AHCI_SIG_SATAPI: return AHCI_DEV_SATAPI;
        case AHCI_SIG_SEMB:   return AHCI_DEV_SEMB;
        default:              return AHCI_DEV_NONE;
    }
}

/* ── HBA Lifecycle ─────────────────────────────────────────── */

int ahci_hba_init(ahci_hba_t *hba) {
    memset(hba, 0, sizeof(*hba));

    /* Simulated HBA capabilities:
     * - 32 ports, 32 command slots, 64-bit DMA, staggered spin-up */
    hba->cap = (32 << 0) |          /* NCP: Number Command Slots */
               (1 << 5) |           /* S64A: 64-bit Addressing */
               (1 << 6) |           /* SSC: Staggered Spin-up */
               (1 << 8) |           /* SSS: Staggered Spin-up Switch */
               (1 << 15) |          /* SPM: Port Multiplier */
               (1 << 20);           /* SNCQ: Native Command Queuing */
    hba->ghc = AHCI_GHC_AE;  /* AHCI enabled */
    hba->version = 0x00010301; /* AHCI version 1.3.1 */
    hba->pi = 0x00000003;     /* Ports 0 and 1 implemented */
    hba->initialized = 1;

    /* Initialize port structures */
    for (int i = 0; i < AHCI_MAX_PORTS; i++) {
        hba->ports[i].port_num = i;
        hba->ports[i].state = AHCI_PORT_EMPTY;
        hba->ports[i].dev_type = AHCI_DEV_NONE;
    }

    return 0;
}

void ahci_hba_shutdown(ahci_hba_t *hba) {
    if (!hba) return;

    /* Cleanup all ports */
    for (int i = 0; i < AHCI_MAX_PORTS; i++) {
        ahci_port_t *p = &hba->ports[i];
        if (p->cmd_list) {
            free(p->cmd_list);
            p->cmd_list = NULL;
        }
        if (p->cmd_table) {
            free(p->cmd_table);
            p->cmd_table = NULL;
        }
        ahci_sim_disk_destroy(hba, i);
    }

    hba->initialized = 0;
    hba->num_ports = 0;
}

/* ── Port Management ──────────────────────────────────────── */

int ahci_enumerate_ports(ahci_hba_t *hba) {
    if (!hba || !hba->initialized) return -1;

    int count = 0;
    for (int i = 0; i < AHCI_MAX_PORTS; i++) {
        if (hba->pi & (1 << i)) {
            ahci_port_t *p = &hba->ports[i];
            p->port_num = i;
            p->signature = AHCI_SIG_SATA; /* Assume SATA for sim */
            p->dev_type = ahci_dev_type_from_sig(p->signature);
            p->state = AHCI_PORT_PRESENT;
            count++;
        }
    }

    hba->num_ports = count;
    return count;
}

int ahci_port_init(ahci_hba_t *hba, int port_num) {
    if (!hba || port_num < 0 || port_num >= AHCI_MAX_PORTS) return -1;

    ahci_port_t *p = &hba->ports[port_num];
    if (p->state == AHCI_PORT_EMPTY) return -1;

    /* Allocate command list (32 command headers) */
    p->cmd_list = (ahci_cmd_header_t *)calloc(AHCI_CMD_SLOTS, sizeof(ahci_cmd_header_t));
    if (!p->cmd_list) return -1;

    /* Allocate command table + PRDT */
    size_t ct_size = sizeof(ahci_cmd_table_t) + AHCI_PRDT_ENTRIES * sizeof(ahci_prdt_t);
    p->cmd_table = (ahci_cmd_table_t *)calloc(1, ct_size);
    if (!p->cmd_table) {
        free(p->cmd_list);
        p->cmd_list = NULL;
        return -1;
    }

    p->state = AHCI_PORT_ACTIVE;
    p->sector_size = AHCI_SECTOR_SIZE;

    return 0;
}

/* ── IDENTIFY DEVICE ───────────────────────────────────────── */

int ahci_identify(ahci_hba_t *hba, int port_num) {
    if (!hba || port_num < 0 || port_num >= AHCI_MAX_PORTS) return -1;

    ahci_port_t *p = &hba->ports[port_num];
    if (p->state != AHCI_PORT_ACTIVE) return -1;

    /* In hosted mode, generate simulated IDENTIFY data */
    memset(&p->identify, 0, sizeof(p->identify));

    /* Model: "WuBuOS SIM SATA" */
    const char *model = "WuBuOS SIM SATA    ";
    for (int i = 0; i < 20 && model[i]; i++) {
        /* ATA strings are byte-swapped */
        p->identify.model[i] = ((uint16_t)model[i] << 8) | (model[i + 1] & 0xFF);
        if (model[i + 1]) i++;
    }

    /* Serial: "SIM0001" */
    const char *serial = "SIM0001   ";
    for (int i = 0; i < 10 && serial[i]; i++) {
        p->identify.serial[i] = ((uint16_t)serial[i] << 8) | (serial[i + 1] & 0xFF);
        if (serial[i + 1]) i++;
    }

    /* LBA count from sim_disk size */
    if (p->sim_disk && p->sim_disk_size > 0) {
        p->lba_count = p->sim_disk_size / AHCI_SECTOR_SIZE;
    } else {
        p->lba_count = 0;
    }

    /* Populate LBA28 fields */
    if (p->lba_count <= 0x0FFFFFFFULL) {
        p->identify.lba28_sectors[0] = (uint16_t)(p->lba_count & 0xFFFF);
        p->identify.lba28_sectors[1] = (uint16_t)((p->lba_count >> 16) & 0xFFFF);
    }

    /* Populate LBA48 fields */
    p->identify.lba48_sectors[0] = (uint16_t)(p->lba_count & 0xFFFF);
    p->identify.lba48_sectors[1] = (uint16_t)((p->lba_count >> 16) & 0xFFFF);
    p->identify.lba48_sectors[2] = (uint16_t)((p->lba_count >> 32) & 0xFFFF);
    p->identify.lba48_sectors[3] = (uint16_t)((p->lba_count >> 48) & 0xFFFF);

    return 0;
}

void ahci_identify_model(const ahci_identify_t *id, char *buf, int bufsz) {
    if (!id || !buf || bufsz <= 0) return;
    /* ATA model strings are byte-swapped pairs */
    int j = 0;
    for (int i = 0; i < 20 && j < bufsz - 1; i++) {
        uint8_t hi = (id->model[i] >> 8) & 0xFF;
        uint8_t lo = id->model[i] & 0xFF;
        if (hi > 0x20 && hi < 0x7F) buf[j++] = hi;
        if (lo > 0x20 && lo < 0x7F) buf[j++] = lo;
    }
    buf[j] = '\0';
    /* Trim trailing spaces */
    while (j > 0 && buf[j - 1] == ' ') buf[--j] = '\0';
}

void ahci_identify_serial(const ahci_identify_t *id, char *buf, int bufsz) {
    if (!id || !buf || bufsz <= 0) return;
    int j = 0;
    for (int i = 0; i < 10 && j < bufsz - 1; i++) {
        uint8_t hi = (id->serial[i] >> 8) & 0xFF;
        uint8_t lo = id->serial[i] & 0xFF;
        if (hi > 0x20 && hi < 0x7F) buf[j++] = hi;
        if (lo > 0x20 && lo < 0x7F) buf[j++] = lo;
    }
    buf[j] = '\0';
}

/* ── I/O Operations ────────────────────────────────────────── */

int ahci_read(ahci_hba_t *hba, int port_num,
              uint64_t lba, uint32_t count, void *buf) {
    if (!hba || !buf) return -1;
    if (port_num < 0 || port_num >= AHCI_MAX_PORTS) return -1;

    ahci_port_t *p = &hba->ports[port_num];
    if (p->state != AHCI_PORT_ACTIVE) return -1;
    if (!p->sim_disk) return -1;

    /* Bounds check */
    uint64_t offset = lba * AHCI_SECTOR_SIZE;
    uint64_t nbytes = (uint64_t)count * AHCI_SECTOR_SIZE;
    if (offset + nbytes > p->sim_disk_size) return -1;

    /* In hosted mode, copy from sim disk */
    memcpy(buf, p->sim_disk + offset, nbytes);

    hba->reads++;
    return (int)count;
}

int ahci_write(ahci_hba_t *hba, int port_num,
               uint64_t lba, uint32_t count, const void *buf) {
    if (!hba || !buf) return -1;
    if (port_num < 0 || port_num >= AHCI_MAX_PORTS) return -1;

    ahci_port_t *p = &hba->ports[port_num];
    if (p->state != AHCI_PORT_ACTIVE) return -1;
    if (!p->sim_disk) return -1;

    /* Bounds check */
    uint64_t offset = lba * AHCI_SECTOR_SIZE;
    uint64_t nbytes = (uint64_t)count * AHCI_SECTOR_SIZE;
    if (offset + nbytes > p->sim_disk_size) return -1;

    /* In hosted mode, copy to sim disk */
    memcpy(p->sim_disk + offset, buf, nbytes);

    hba->writes++;
    return (int)count;
}

/* ── Hosted Simulation ─────────────────────────────────────── */

int ahci_sim_disk_create(ahci_hba_t *hba, int port_num, int size_mb) {
    if (!hba || port_num < 0 || port_num >= AHCI_MAX_PORTS) return -1;

    ahci_port_t *p = &hba->ports[port_num];

    /* Allocate simulated disk buffer */
    uint64_t size = (uint64_t)size_mb * 1024 * 1024;
    p->sim_disk = (uint8_t *)calloc(1, (size_t)size);
    if (!p->sim_disk) return -1;

    p->sim_disk_size = size;
    p->lba_count = size / AHCI_SECTOR_SIZE;
    p->sector_size = AHCI_SECTOR_SIZE;

    return 0;
}

void ahci_sim_disk_destroy(ahci_hba_t *hba, int port_num) {
    if (!hba || port_num < 0 || port_num >= AHCI_MAX_PORTS) return;

    ahci_port_t *p = &hba->ports[port_num];
    if (p->sim_disk) {
        free(p->sim_disk);
        p->sim_disk = NULL;
        p->sim_disk_size = 0;
        p->lba_count = 0;
    }
}

/* ── Diagnostics ───────────────────────────────────────────── */

void ahci_hba_dump(const ahci_hba_t *hba) {
    if (!hba) return;
    printf("AHCI HBA: version=0x%08X, ports=%d, initialized=%d\n",
           hba->version, hba->num_ports, hba->initialized);
    printf("  CAP: 0x%08X, GHC: 0x%08X, PI: 0x%08X\n",
           hba->cap, hba->ghc, hba->pi);
    printf("  Reads: %lu, Writes: %lu, Errors: %lu\n",
           (unsigned long)hba->reads, (unsigned long)hba->writes,
           (unsigned long)hba->errors);

    for (int i = 0; i < AHCI_MAX_PORTS; i++) {
        const ahci_port_t *p = &hba->ports[i];
        if (p->state == AHCI_PORT_EMPTY) continue;
        const char *stype = "NONE";
        switch (p->dev_type) {
            case AHCI_DEV_SATA:   stype = "SATA"; break;
            case AHCI_DEV_SATAPI: stype = "SATAPI"; break;
            case AHCI_DEV_SEMB:   stype = "SEMB"; break;
            default: break;
        }
        printf("  Port %d: state=%d type=%s lba=%lu size=%lluMB\n",
               i, p->state, stype, (unsigned long)p->lba_count,
               (unsigned long long)(p->sim_disk_size / (1024*1024)));
    }
}

uint64_t ahci_hba_reads(const ahci_hba_t *hba) {
    return hba ? hba->reads : 0;
}

uint64_t ahci_hba_writes(const ahci_hba_t *hba) {
    return hba ? hba->writes : 0;
}
