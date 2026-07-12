/*
 * WuBuOS -- extracted module (auto-split, C11, opaque-safe)
 */

#include "fat32.h"
#include "fat32_internal.h"
#include <stdlib.h>
#include <stdint.h>

uint32_t fat32_next_cluster(fat32_volume *vol, uint32_t cluster) {
    if (cluster < 2) return 0;
    uint32_t entry;
    if (fat_read_entry(vol, cluster, &entry) != 0) return 0;
    if (entry >= FAT32_CLUSTER_EOC) return 0;  /* End of chain */
    return entry;
}

uint64_t fat32_cluster_to_lba(fat32_volume *vol, uint32_t cluster) {
    if (cluster < 2) return 0;
    return vol->data_lba + (uint64_t)(cluster - 2) * vol->sectors_per_cluster;
}

uint32_t fat32_lba_to_cluster(fat32_volume *vol, uint64_t lba) {
    if (lba < vol->data_lba) return 0;
    uint32_t cluster = (uint32_t)((lba - vol->data_lba) / vol->sectors_per_cluster) + 2;
    return cluster;
}

uint32_t fat32_alloc_clusters(fat32_volume *vol, uint32_t count, uint32_t hint) {
    if (count == 0) return 0;

    uint32_t start = (hint >= 2) ? hint : 2;
    if (start < 2) start = 2;

    /* Scan for free clusters */
    uint32_t first = 0;
    uint32_t found = 0;
    uint32_t cluster = start;

    for (uint32_t scanned = 0; scanned < vol->total_clusters; scanned++) {
        if (cluster >= vol->total_clusters) cluster = 2;

        uint32_t entry;
        if (fat_read_entry(vol, cluster, &entry) != 0) return 0;

        if (entry == FAT32_CLUSTER_FREE) {
            if (found == 0) first = cluster;
            found++;
            if (found >= count) break;
        } else {
            found = 0;
            first = 0;
        }
        cluster++;
    }

    if (found < count) return 0;  /* Not enough free clusters */

    /* Link the chain */
    for (uint32_t i = 0; i < count; i++) {
        uint32_t c = first + i;
        uint32_t next = (i < count - 1) ? (c + 1) : FAT32_CLUSTER_EOC;
        if (fat_write_entry(vol, c, next) != 0) return 0;
    }

    /* Update hint */
    vol->next_free = first + count;
    if (vol->next_free >= vol->total_clusters) vol->next_free = 2;
    if (vol->free_clusters > count) vol->free_clusters -= count;

    return first;
}

void fat32_free_chain(fat32_volume *vol, uint32_t cluster) {
    while (cluster >= 2 && cluster < FAT32_CLUSTER_EOC) {
        uint32_t next;
        if (fat_read_entry(vol, cluster, &next) != 0) break;
        fat_write_entry(vol, cluster, FAT32_CLUSTER_FREE);
        vol->free_clusters++;
        if (next >= FAT32_CLUSTER_EOC) break;
        cluster = next;
    }
}

uint32_t fat32_count_free(fat32_volume *vol) {
    uint32_t count = 0;
    for (uint32_t c = 2; c < vol->total_clusters; c++) {
        uint32_t entry;
        if (fat_read_entry(vol, c, &entry) != 0) break;
        if (entry == FAT32_CLUSTER_FREE) count++;
    }
    vol->free_clusters = count;
    return count;
}
