/*
 * ahci.h  --  WuBuOS AHCI (SATA) Disk Driver
 *
 * Cell 072: Advanced Host Controller Interface driver for SATA disks.
 * Ported from ZealOS Kernel/BlkDev/DiskAHCI.ZC design principles.
 *
 * AHCI provides:
 *   - Command list + command table management
 *   - Port enumeration (up to 32 ports)
 *   - DMA read/write via PRDT (Physical Region Descriptor Table)
 *   - IDENTIFY DEVICE parsing
 *   - ATA command issuing (READ DMA, WRITE DMA)
 *
 * In hosted mode, provides a simulated disk backed by a memory buffer
 * so all functionality can be tested without real hardware.
 *
 * All C11, no external deps.
 */

#ifndef WUBU_AHCI_H
#define WUBU_AHCI_H

#include <stdint.h>
#include <stddef.h>

/* -- AHCI Constants ------------------------------------------ */

#define AHCI_MAX_PORTS       32
#define AHCI_CMD_SLOTS       32     /* Command slots per port */
#define AHCI_PRDT_ENTRIES    8      /* PRDT entries per command */
#define AHCI_SECTOR_SIZE     512
#define AHCI_CMD_TIMEOUT_MS  5000

/* AHCI Host Bus Adapter (HBA) registers */
#define AHCI_HBA_CAP    0x00   /* HBA Capabilities */
#define AHCI_HBA_GHC   0x04   /* Global HBA Control */
#define AHCI_HBA_IS    0x08   /* Interrupt Status */
#define AHCI_HBA_PI    0x0C   /* Port Implemented */
#define AHCI_HBA_VS    0x10   /* Version */
#define AHCI_HBA_CCC   0x14   /* Command Completion Coalesce */
#define AHCI_HBA_CCCP  0x18   /* CCC Ports */
#define AHCI_HBA_EMLOC 0x1C   /* Enclosure Management Location */
#define AHCI_HBA_EMCAP 0x20   /* Enclosure Management Capabilities */
#define AHCI_HBA_CAP2  0x24   /* HBA Capabilities Extended */
#define AHCI_HBA_BOHC  0x28   /* BIOS/OS Handoff Control and Status */

/* AHCI Port registers (offset from port base) */
#define AHCI_PxCLB    0x00   /* Command List Base Address */
#define AHCI_PxCLBU   0x04   /* Command List Base Address Upper */
#define AHCI_PxFB     0x08   /* FIS Base Address */
#define AHCI_PxFBU    0x0C   /* FIS Base Address Upper */
#define AHCI_PxIS     0x10   /* Interrupt Status */
#define AHCI_PxIE     0x14   /* Interrupt Enable */
#define AHCI_PxCMD    0x18   /* Command and Status */
#define AHCI_PxTFD    0x20   /* Task File Data */
#define AHCI_PxSIG    0x24   /* Signature */
#define AHCI_PxSSTS   0x28   /* SATA Status */
#define AHCI_PxSCTL   0x2C   /* SATA Control */
#define AHCI_PxSERR   0x30   /* SATA Error */
#define AHCI_PxSACT   0x34   /* SATA Active */
#define AHCI_PxCI     0x38   /* Command Issue */
#define AHCI_PxSNTF   0x3C   /* SNotification */
#define AHCI_PxFBS    0x40   /* FIS-Based Switching Control */

/* GHC flags */
#define AHCI_GHC_AE   (1 << 31)  /* AHCI Enable */
#define AHCI_GHC_MRSM (1 << 2)   /* MSI Revert to Single Message */
#define AHCI_GHC_IE   (1 << 1)   /* Interrupt Enable */
#define AHCI_GHC_HR   (1 << 0)   /* HBA Reset */

/* CMD flags */
#define AHCI_CMD_ICC_SHIFT  28
#define AHCI_CMD_AIA  (1 << 26)  /* Aggressive Interface Power Management */
#define AHCI_CMD_ALPE (1 << 25)  /* Aggressive Link Power Management */
#define AHCI_CMD_ASLE (1 << 24)  /* Aggressive Slumber */
#define AHCI_CMD_FRE  (1 << 4)   /* FIS Receive Enable */
#define AHCI_CMD_CLO  (1 << 3)   /* Command List Override */
#define AHCI_CMD_PMA  (1 << 2)   /* Port Multiplier Attached */
#define AHCI_CMD_ST   (1 << 0)   /* Start */

/* Port signature for SATA devices */
#define AHCI_SIG_SATA   0x00000101  /* SATA disk */
#define AHCI_SIG_SATAPI 0xEB140101  /* SATAPI (optical) */
#define AHCI_SIG_SEMB   0xEF0101    /* Enclosure Management Bridge */
#define AHCI_SIG_PM     0x9669      /* Port Multiplier */

/* -- AHCI Data Structures ------------------------------------ */

/* Command Header */
typedef struct {
    uint8_t  desc;         /* Command descriptor: PRDT length (5:0), A (5), W (6), C (7) */
    uint8_t  prdtl;        /* PRDT length (lower 5 bits of desc + this) */
    uint16_t prdbc;        /* PRDT byte count transferred */
    uint32_t ctba;         /* Command Table Descriptor Base Address */
    uint32_t ctbau;        /* Command Table Descriptor Base Address Upper */
    uint32_t reserved[4];
} ahci_cmd_header_t;

/* Command Table */
typedef struct {
    uint8_t  cfis[64];     /* Command FIS */
    uint8_t  acmd[16];     /* ATAPI Command */
    uint8_t  reserved[48];
    /* PRDT follows at offset 128 */
} ahci_cmd_table_t;

/* Physical Region Descriptor Table entry */
typedef struct {
    uint32_t dba;         /* Data Byte Address */
    uint32_t dbau;        /* Data Byte Address Upper */
    uint32_t reserved;
    uint32_t dbc;         /* Data Byte Count (22:0), interrupt (31) */
} ahci_prdt_t;

/* Port state */
typedef enum {
    AHCI_PORT_EMPTY = 0,
    AHCI_PORT_PRESENT = 1,
    AHCI_PORT_ACTIVE = 2,
    AHCI_PORT_FAULT = 3
} ahci_port_state_t;

/* Device type from IDENTIFY */
typedef enum {
    AHCI_DEV_NONE = 0,
    AHCI_DEV_SATA = 1,
    AHCI_DEV_SATAPI = 2,
    AHCI_DEV_SEMB = 3
} ahci_dev_type_t;

/*_IDENTIFY DEVICE data (partial  --  512 bytes total) */
typedef struct {
    uint16_t config;           /* 0: General configuration */
    uint16_t logical_cylinders;/* 1: Logical cylinders */
    uint16_t reserved2[2];
    uint16_t logical_heads;    /* 5: Logical heads */
    uint16_t reserved5[3];
    uint16_t logical_sectors;  /* 9: Logical sectors per track */
    uint16_t serial[10];       /* 10-19: Serial number (ASCII) */
    uint16_t reserved20[3];
    uint16_t firmware[4];      /* 23-26: Firmware revision */
    uint16_t model[20];        /* 27-46: Model number */
    uint16_t reserved47[2];
    uint16_t capabilities49;   /* 49: Capabilities */
    uint16_t capabilities50;
    uint16_t reserved51[7];
    uint16_t lba28_sectors[2]; /* 60-61: Total sectors (LBA28) */
    uint16_t reserved62[98];
    uint16_t lba48_sectors[4]; /* 100-103: Total sectors (LBA48) */
    uint16_t reserved104[152];
    /* Total: 256 words = 512 bytes */
} ahci_identify_t;

/* Port structure */
typedef struct {
    int              port_num;       /* Port index */
    ahci_port_state_t state;         /* Port state */
    ahci_dev_type_t   dev_type;      /* Device type */
    uint32_t          signature;     /* Port signature */
    uint64_t          lba_count;     /* Total LBA sectors */
    uint32_t          sector_size;   /* Bytes per sector */

    /* Command structures (hosted: heap-allocated) */
    ahci_cmd_header_t *cmd_list;     /* Command list (32 entries) */
    ahci_cmd_table_t  *cmd_table;    /* Command table + PRDT */
    ahci_identify_t    identify;     /* Device IDENTIFY data */

    /* Hosted simulation backing */
    uint8_t           *sim_disk;     /* Simulated disk buffer */
    uint64_t           sim_disk_size;/* Simulated disk size in bytes */
} ahci_port_t;

/* HBA (Host Bus Adapter) state */
typedef struct {
    uint32_t    cap;           /* HBA Capabilities */
    uint32_t    ghc;           /* Global HBA Control */
    uint32_t    is;            /* Interrupt Status */
    uint32_t    pi;            /* Port Implemented bitmask */
    uint32_t    version;       /* AHCI version */

    ahci_port_t ports[AHCI_MAX_PORTS];
    int         num_ports;     /* Active port count */
    int         initialized;   /* 1 if HBA is initialized */

    /* Stats */
    uint64_t    reads;
    uint64_t    writes;
    uint64_t    errors;
} ahci_hba_t;

/* -- HBA Lifecycle ------------------------------------------- */

/* Initialize HBA (in real kernel: takes ABAR address; hosted: simulated) */
int  ahci_hba_init(ahci_hba_t *hba);

/* Shutdown HBA */
void ahci_hba_shutdown(ahci_hba_t *hba);

/* -- Port Management ---------------------------------------- */

/* Enumerate ports on the HBA. Returns number of active ports. */
int  ahci_enumerate_ports(ahci_hba_t *hba);

/* Initialize a specific port. */
int  ahci_port_init(ahci_hba_t *hba, int port_num);

/* Get device type from port signature. */
ahci_dev_type_t ahci_dev_type_from_sig(uint32_t signature);

/* -- IDENTIFY DEVICE ----------------------------------------- */

/* Issue IDENTIFY DEVICE command to a port.
 * In hosted mode, populates with simulated data. */
int  ahci_identify(ahci_hba_t *hba, int port_num);

/* Extract model string from IDENTIFY data (swapped ASCII). */
void ahci_identify_model(const ahci_identify_t *id, char *buf, int bufsz);

/* Extract serial number. */
void ahci_identify_serial(const ahci_identify_t *id, char *buf, int bufsz);

/* -- I/O Operations ------------------------------------------ */

/* Read sectors from a port via DMA.
 * lba: starting LBA, count: number of sectors, buf: output buffer.
 * Returns number of sectors read, or -1 on error. */
int  ahci_read(ahci_hba_t *hba, int port_num,
               uint64_t lba, uint32_t count, void *buf);

/* Write sectors to a port via DMA. */
int  ahci_write(ahci_hba_t *hba, int port_num,
                uint64_t lba, uint32_t count, const void *buf);

/* -- Hosted Simulation --------------------------------------- */

/* Create a simulated disk backing for a port.
 * size_mb: disk size in megabytes. Returns 0 on success. */
int  ahci_sim_disk_create(ahci_hba_t *hba, int port_num, int size_mb);

/* Destroy simulated disk. */
void ahci_sim_disk_destroy(ahci_hba_t *hba, int port_num);

/* -- Diagnostics --------------------------------------------- */

/* Print HBA info. */
void ahci_hba_dump(const ahci_hba_t *hba);

/* Get HBA stats. */
uint64_t ahci_hba_reads(const ahci_hba_t *hba);
uint64_t ahci_hba_writes(const ahci_hba_t *hba);

#endif /* WUBU_AHCI_H */
