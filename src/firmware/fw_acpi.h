/*
 * fw_acpi.h  --  WuBuFW ACPI table access.
 */

#ifndef WUBUFW_ACPI_H
#define WUBUFW_ACPI_H

#include <stdint.h>

typedef struct __attribute__((packed)) {
    char     signature[8];
    uint8_t  checksum;
    char     oem_id[6];
    uint8_t  revision;
    uint32_t rsdt_addr;
    uint32_t length;
    uint64_t xsdt_addr;
    uint8_t  ext_checksum;
    uint8_t  reserved[3];
} fw_acpi_rsdp;

typedef struct __attribute__((packed)) {
    char     signature[4];
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    char     oem_id[6];
    char     oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} fw_acpi_hdr;

int          fw_acpi_init(void);
void         fw_acpi_set_rsdp(void *rsdp);
fw_acpi_hdr *fw_acpi_find(const char *sig);
void        *fw_acpi_rsdp_ptr(void);

uint64_t fw_acpi_tpm2_control_area(void);
uint32_t fw_acpi_tpm2_start_method(void);
uint64_t fw_acpi_tpm_log_addr(void);
uint64_t fw_acpi_tpm_log_size(void);

#endif
