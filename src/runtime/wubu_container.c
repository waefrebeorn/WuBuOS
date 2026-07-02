/*
 * wubu_container.c  --  WuBuOS Universal Container Format Implementation
 *
 * .wubu: one extension, infinite formats. The header does the magic.
 */

#include "wubu_container.h"
#include "wubu_crypto.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>

/* CRC32 is in wubu_crypto.h (included above) */

/* -- Payload Detection -------------------------------------------- */

WUBU_PAYLOAD_TYPE wubu_detect_payload_type(const void *data, size_t size) {
    if (!data || size < 2) return WUBU_PAYLOAD_DATA;

    const uint8_t *p = (const uint8_t *)data;

    /* .wubu container  --  recursive */
    if (memcmp(p, WUBU_MAGIC, WUBU_MAGIC_SIZE) == 0)
        return WUBU_PAYLOAD_NESTED_WUBU;

    /* ELF magic: 0x7F 'E' 'L' 'F' */
    if (size >= 4 && p[0] == 0x7F && p[1] == 'E' && p[2] == 'L' && p[3] == 'F')
        return WUBU_PAYLOAD_LINUX_ELF;

    /* PE/COFF magic: "MZ" */
    if (size >= 2 && p[0] == 'M' && p[1] == 'Z')
        return WUBU_PAYLOAD_WIN_PE;

    /* Mach-O magic: 0xFEEDFACE or 0xFEEDFACF or 0xCEFAEDFE or 0xCFFAEDFE */
    if (size >= 4) {
        uint32_t magic;
        memcpy(&magic, p, 4);
        if (magic == 0xFEEDFACE || magic == 0xFEEDFACF ||
            magic == 0xCEFAEDFE || magic == 0xCFFAEDFE)
            return WUBU_PAYLOAD_MAC_MACHO;
    }

    /* WASM magic: 0x00 'a' 's' 'm' */
    if (size >= 4 && p[0] == 0x00 && p[1] == 'a' && p[2] == 's' && p[3] == 'm')
        return WUBU_PAYLOAD_WASM;

    /* Shebang: "#!" */
    if (size >= 2 && p[0] == '#' && p[1] == '!') {
        /* Check for python specifically */
        if (size >= 20) {
            /* Look for "python" in the shebang line */
            for (size_t i = 2; i < size - 6; i++) {
                if (memcmp(p + i, "python", 6) == 0)
                    return WUBU_PAYLOAD_PYTHON;
            }
        }
        return WUBU_PAYLOAD_SHELL_SCRIPT;
    }

    /* HolyC: look for common HolyC keywords */
    if (size >= 3) {
        if (memcmp(p, "U0 ", 3) == 0 || memcmp(p, "I64", 3) == 0 ||
            memcmp(p, "U8 ", 3) == 0  || memcmp(p, "I0 ", 3) == 0 ||
            memcmp(p, "F64", 3) == 0 || memcmp(p, "Bool", 4) == 0)
            return WUBU_PAYLOAD_HOLYC_SRC;
        /* Also detect by common HolyC patterns */
        if (size >= 6 && memcmp(p, "return", 6) == 0)
            return WUBU_PAYLOAD_HOLYC_SRC;
    }

    /* C source: look for #include, int main, etc. */
    if (size >= 8) {
        if (memcmp(p, "#include", 8) == 0 || memcmp(p, "int main", 8) == 0 ||
            memcmp(p, "void   ", 8) == 0)
            return WUBU_PAYLOAD_C_SRC;
    }

    return WUBU_PAYLOAD_DATA;
}

/* -- Container Create --------------------------------------------- */

int wubu_container_create(const WUBU_HEADER *header,
                          const void *payload, size_t payload_size,
                          void *out_buf, size_t out_buf_size,
                          size_t *out_size) {
    size_t total = WUBU_HEADER_SIZE + payload_size;
    if (!out_buf || out_buf_size < total) return -1;
    if (!header || (!payload && payload_size > 0)) return -1;

    /* Copy header */
    WUBU_HEADER hdr = *header;

    /* Force magic */
    memcpy(hdr.magic, WUBU_MAGIC, WUBU_MAGIC_SIZE);
    hdr.version_major = WUBU_VERSION_MAJOR;
    hdr.version_minor = WUBU_VERSION_MINOR;
    hdr.payload_size = payload_size;

    /* Compute header CRC (over header minus the crc field itself) */
    size_t crc_offset = offsetof(WUBU_HEADER, header_crc);
    hdr.header_crc = 0; /* Zero before computing */
    hdr.header_crc = wubu_crc32(&hdr, WUBU_HEADER_SIZE);

    /* Write header + payload */
    memcpy(out_buf, &hdr, WUBU_HEADER_SIZE);
    if (payload && payload_size > 0)
        memcpy((uint8_t *)out_buf + WUBU_HEADER_SIZE, payload, payload_size);

    if (out_size) *out_size = total;
    return 0;
}

/* -- Container Parse ---------------------------------------------- */

int wubu_container_parse(const void *data, size_t data_size,
                         WUBU_HEADER *out_header,
                         const void **out_payload, size_t *out_payload_size) {
    if (!data || data_size < WUBU_HEADER_SIZE) return -1;

    const WUBU_HEADER *hdr = (const WUBU_HEADER *)data;

    /* Validate magic */
    if (memcmp(hdr->magic, WUBU_MAGIC, WUBU_MAGIC_SIZE) != 0) return -1;

    /* Validate version */
    if (hdr->version_major != WUBU_VERSION_MAJOR) return -1;

    /* Validate payload size */
    if (WUBU_HEADER_SIZE + hdr->payload_size > data_size) return -1;

    /* Validate CRC */
    WUBU_HEADER check = *hdr;
    uint32_t saved_crc = check.header_crc;
    check.header_crc = 0;
    uint32_t computed_crc = wubu_crc32(&check, WUBU_HEADER_SIZE);
    if (saved_crc != computed_crc) return -1;

    /* Output */
    if (out_header) *out_header = *hdr;
    if (out_payload) *out_payload = (const uint8_t *)data + WUBU_HEADER_SIZE;
    if (out_payload_size) *out_payload_size = hdr->payload_size;

    return 0;
}

/* -- Container Validate ------------------------------------------- */

int wubu_container_validate(const void *data, size_t data_size) {
    return wubu_container_parse(data, data_size, NULL, NULL, NULL);
}

/* -- Metadata Read ------------------------------------------------ */

int wubu_container_read_meta(const void *data, size_t data_size,
                             WUBU_METADATA *out_meta) {
    WUBU_HEADER hdr;
    if (wubu_container_parse(data, data_size, &hdr, NULL, NULL) != 0)
        return -1;

    if (!out_meta) return -1;
    memset(out_meta, 0, sizeof(*out_meta));

    if (hdr.meta_offset == 0 || hdr.meta_size == 0) return 0;

    /* Meta is within the payload region */
    if (hdr.meta_offset + hdr.meta_size > hdr.payload_size) return -1;

    /* Parse simple KV pairs from payload
     * Format: key\0value\0key\0value\0... */
    const uint8_t *payload = (const uint8_t *)data + WUBU_HEADER_SIZE;
    const char *meta_data = (const char *)(payload + hdr.meta_offset);
    const char *meta_end = meta_data + hdr.meta_size;

    int count = 0;
    const char *p = meta_data;
    while (p < meta_end && count < WUBU_META_MAX_ENTRIES) {
        /* Read key */
        size_t klen = strnlen(p, (size_t)(meta_end - p));
        if (klen >= 32) break;
        strncpy(out_meta->entries[count].key, p, 31);
        p += klen + 1;
        if (p >= meta_end) break;

        /* Read value */
        size_t vlen = strnlen(p, (size_t)(meta_end - p));
        if (vlen >= 64) break;
        strncpy(out_meta->entries[count].value, p, 63);
        p += vlen + 1;

        count++;
    }

    out_meta->n_entries = (uint32_t)count;
    return count;
}

/* -- Convenience Wrappers ----------------------------------------- */

int wubu_container_native_exec(const void *code, size_t code_size,
                               uint64_t entry_offset,
                               void *out_buf, size_t out_buf_size,
                               size_t *out_size) {
    WUBU_HEADER hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.payload_type = WUBU_PAYLOAD_NATIVE_EXEC;
    hdr.arch = WUBU_ARCH_X86_64;
    hdr.flags = WUBU_FLAG_JIT_COMPILE;
    hdr.handler_id = 1; /* WuBuOS native */
    hdr.os_persona = WUBU_OS_NATIVE;
    hdr.entry_offset = entry_offset;
    return wubu_container_create(&hdr, code, code_size,
                                out_buf, out_buf_size, out_size);
}

int wubu_container_linux_elf(const void *elf_data, size_t elf_size,
                             void *out_buf, size_t out_buf_size,
                             size_t *out_size) {
    WUBU_HEADER hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.payload_type = WUBU_PAYLOAD_LINUX_ELF;
    hdr.arch = WUBU_ARCH_X86_64;
    hdr.flags = WUBU_FLAG_SANDBOXED | WUBU_FLAG_PERSIST_VSL;
    hdr.handler_id = 10; /* VSL */
    hdr.os_persona = WUBU_OS_LINUX;
    hdr.entry_offset = 0;
    return wubu_container_create(&hdr, elf_data, elf_size,
                                out_buf, out_buf_size, out_size);
}
