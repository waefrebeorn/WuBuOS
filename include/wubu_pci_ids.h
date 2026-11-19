/*
 * wubu_pci_ids.h -- PCI vendor/class IDs. ONE owner.
 *
 * Format facts from the PCI SIG registry, shared by every kernel driver
 * that matches devices (audio, codec, accel, net, raid). Created by the
 * org-wide hardcoding census (2026-11-19): five drivers kept private
 * subsets, and two names were overloaded:
 *   - PCI_VENDOR_AMD meant 0x1022 (AMD chipset) in accel but 0x1002
 *     (AMD/ATI graphics) in audio. Both are AMD; the IDs are distinct:
 *     AMD vs AMD_ATI.
 *   - PCI_VENDOR_BROADCOM meant 0x14E4 (NetXtreme) in net but 0x1000
 *     (LSI MegaRAID, acquired by Broadcom) in raid. LSI keeps its own
 *     name; BROADCOM is 0x14E4.
 */
#ifndef WUBU_PCI_IDS_H
#define WUBU_PCI_IDS_H

/* Vendors */
#define PCI_VENDOR_INTEL     0x8086
#define PCI_VENDOR_NVIDIA    0x10DE
#define PCI_VENDOR_AMD       0x1022   /* AMD chipset/CPU side */
#define PCI_VENDOR_AMD_ATI   0x1002   /* AMD/ATI graphics (HDMI audio etc.) */
#define PCI_VENDOR_REALTEK   0x10EC
#define PCI_VENDOR_MEDIATEK  0x14C3
#define PCI_VENDOR_BROADCOM  0x14E4   /* NetXtreme etc. */
#define PCI_VENDOR_LSI       0x1000   /* MegaRAID/SAS (now Broadcom) */
#define PCI_VENDOR_ATHEROS   0x168C
#define PCI_VENDOR_QUALCOMM  0x17CB
#define PCI_VENDOR_GOOGLE    0x1AE0
#define PCI_VENDOR_IDT       0x111D
#define PCI_VENDOR_CIRRUS    0x1013
#define PCI_VENDOR_CONEXANT  0x14F1
#define PCI_VENDOR_ANALOG    0x11D4
#define PCI_VENDOR_MICROSEMI 0x9005   /* Adaptec/aacraid */
#define PCI_VENDOR_MICROCHIP 0x1137   /* SmartPQI */
#define PCI_VENDOR_MARVELL   0x1B4B
#define PCI_VENDOR_ARECA     0x1B41

/* Classes (24-bit class_code<<16 | subclass<<8 | prog_if, or the bytes) */
#define PCI_CLASS_AUDIO       0x040300  /* HD Audio, full 24-bit match */
#define PCI_CLASS_AUDIO_BASE  0x04      /* multimedia base class byte */
#define PCI_SUBCLASS_AUDIO    0x03
#define PCI_CLASS_COPROC      0x0B      /* signal processing */
#define PCI_SUBCLASS_PROC     0x40      /* processor (NPU on some) */
#define PCI_SUBCLASS_RAID     0x04
#define PCI_SUBCLASS_SAS      0x07
#define PCI_SUBCLASS_SCSI     0x00

#endif /* WUBU_PCI_IDS_H */