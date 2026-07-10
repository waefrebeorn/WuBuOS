/* wubu_proton_pe.c -- PE (Portable Executable) validation/parsing subsystem.
 *
 * Self-contained: wubu_proton_is_pe / validate_pe / parse_pe. Uses only the
 * PE type/define vocabulary from wubu_proton.h (PE_MAGIC, pe_coff_header_t,
 * pe_opt_header_std_t, wubu_proton_t). Minimal includes.
 */

#include "wubu_proton.h"
#include <string.h>

int wubu_proton_is_pe(const uint8_t *data, size_t size) {
    /* Minimum: MZ header + PE signature */
    if (size < 64) return 0;  /* Need at least DOS header */

    /* Check MZ signature */
    if (data[0] != 'M' || data[1] != 'Z') return 0;

    /* Find PE header offset (offset 0x3C in DOS header) */
    uint32_t pe_offset = *(uint32_t *)&data[0x3C];
    if (pe_offset == 0 || pe_offset + 4 > size) return 0;

    /* Check PE signature */
    uint32_t sig = *(uint32_t *)&data[pe_offset];
    if (sig != PE_MAGIC) return 0;

    return 1;
}

int wubu_proton_validate_pe(wubu_proton_t *p, const uint8_t *data, size_t size) {
    if (!data || size < 64) return -1;

    /* Check MZ header */
    if (data[0] != 'M' || data[1] != 'Z') return -1;

    /* Get PE offset */
    uint32_t pe_offset = *(uint32_t *)&data[0x3C];
    if (pe_offset == 0 || pe_offset + 24 > size) return -1;

    /* Check PE signature */
    uint32_t sig = *(uint32_t *)&data[pe_offset];
    if (sig != PE_MAGIC) return -1;

    /* Read COFF header */
    pe_coff_header_t coff;
    memcpy(&coff, &data[pe_offset + 4], sizeof(coff));

    p->machine = coff.machine;
    if (coff.machine == PE_MACHINE_AMD64) {
        p->is_pe64 = 1;
    } else if (coff.machine == PE_MACHINE_I386) {
        p->is_pe64 = 0;
    } else {
        return -1; /* Unsupported architecture */
    }

    /* Read optional header */
    if (coff.opt_header_size >= sizeof(pe_opt_header_std_t)) {
        pe_opt_header_std_t opt;
        memcpy(&opt, &data[pe_offset + 4 + sizeof(pe_coff_header_t)], sizeof(opt));

        p->entry_point = opt.entry_point;
        uint32_t opt_start = pe_offset + 4 + sizeof(pe_coff_header_t);
        if (opt.magic == PE_OPT_MAGIC_PE32P) {
            p->is_pe64 = 1;
            /* PE32+: ImageBase is 8 bytes at offset 24 */
            if (opt_start + 24 + 8 <= size) {
                uint64_t base64;
                memcpy(&base64, &data[opt_start + 24], 8);
                p->image_base = (uint32_t)base64;
            }
        } else {
            /* PE32: ImageBase is 4 bytes at offset 28 */
            if (opt_start + 28 + 4 <= size) {
                memcpy(&p->image_base, &data[opt_start + 28], 4);
            }
        }
    }

    p->num_sections = 0;
    return 0;
}

int wubu_proton_parse_pe(wubu_proton_t *p, const uint8_t *data, size_t size) {
    if (!data || size < 64) return -1;

    /* Get PE offset */
    uint32_t pe_offset = *(uint32_t *)&data[0x3C];
    if (pe_offset + 24 > size) return -1;

    pe_coff_header_t coff;
    memcpy(&coff, &data[pe_offset + 4], sizeof(coff));

    /* Parse sections */
    uint32_t section_offset = pe_offset + 4 + sizeof(pe_coff_header_t) + coff.opt_header_size;
    int max_sections = coff.num_sections;
    if (max_sections > 32) max_sections = 32;

    p->num_sections = 0;
    for (int i = 0; i < max_sections; i++) {
        if (section_offset + sizeof(pe_section_t) > size) break;
        memcpy(&p->sections[i], &data[section_offset], sizeof(pe_section_t));
        p->num_sections++;
        section_offset += sizeof(pe_section_t);
    }

    /* Calculate total image size from sections */
    p->image_size = 0;
    for (int i = 0; i < p->num_sections; i++) {
        uint32_t end = p->sections[i].virtual_addr + p->sections[i].virtual_size;
        if (end > p->image_size) p->image_size = end;
    }

    return p->num_sections;
}
