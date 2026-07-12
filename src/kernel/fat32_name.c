/*
 * WuBuOS -- extracted module (auto-split, C11, opaque-safe)
 */

#include "fat32.h"
#include "fat32_internal.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void name_to_83(const char *src, char name83[11]) {
    memset(name83, ' ', 11);

    if (strcmp(src, ".") == 0) {
        name83[0] = '.';
        return;
    }
    if (strcmp(src, "..") == 0) {
        name83[0] = '.';
        name83[1] = '.';
        return;
    }

    /* Copy name part (before '.') */
    int i = 0;
    while (i < 8 && *src && *src != '.')
        name83[i++] = (char)toupper((unsigned char)*src++);

    /* Skip dot */
    if (*src == '.') src++;

    /* Copy extension */
    int j = 8;
    while (j < 11 && *src)
        name83[j++] = (char)toupper((unsigned char)*src++);
}

/* Convert 8.3 format → readable name */
void name_from_83(const char name83[11], char *dst, size_t dst_size) {
    size_t k = 0;

    /* Find end of name part */
    int name_end = 7;
    while (name_end >= 0 && name83[name_end] == ' ') name_end--;

    for (int i = 0; i <= name_end && k < dst_size - 1; i++)
        dst[k++] = name83[i];

    /* Find end of extension */
    int ext_end = 10;
    while (ext_end >= 8 && name83[ext_end] == ' ') ext_end--;

    if (ext_end >= 8 && name83[0] != '.') {
        if (k < dst_size - 1) dst[k++] = '.';
        for (int i = 8; i <= ext_end && k < dst_size - 1; i++)
            dst[k++] = name83[i];
    }

    dst[k] = '\0';
}

/* LFN checksum (ZealOS FATNameXSum) */
uint8_t lfn_checksum(const char name83[11]) {
    uint8_t sum = 0;
    for (int i = 0; i < 11; i++) {
        sum = (uint8_t)(((sum & 1) ? 0x80 : 0) + (sum >> 1) + (uint8_t)name83[i]);
    }
    return sum;
}

/* Case-insensitive compare */
int name_cmp(const char *a, const char *b) {
    while (*a && *b) {
        int ca = toupper((unsigned char)*a);
        int cb = toupper((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return toupper((unsigned char)*a) - toupper((unsigned char)*b);
}
