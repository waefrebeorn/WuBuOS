/*
 * wubu_swap.c  --  demand-page swap (gap B3)
 *
 * The swap area lives on the AHCI port-0 sim disk past the FAT32
 * volume: 4096 sectors (4 MB) starting at LBA WUBU_SWAP_SECTOR. Each
 * slot is one page. The slot map is a fixed table indexed by frame
 * (phys - WUBU_VMM_PHYS_BASE)/PAGE; -1 = not swapped out.
 *
 * The disk writes are raw ahci IO (no heap, ISR-adjacent-safe); the
 * PTE invalidation reuses the vmm's walk helpers.
 */
#include "wubu_swap.h"
#include "wubu_vmm.h"

extern int ahci_read(void *hba, int port, uint64_t lba, uint32_t n, void *buf);
extern int ahci_write(void *hba, int port, uint64_t lba, uint32_t n,
                      const void *buf);
extern int ahci_hba_init(void *hba);
extern int ahci_enumerate_ports(void *hba);
extern int ahci_port_init(void *hba, int port);
extern int ahci_sim_disk_create(void *hba, int port, int mb);

#define PAGE 4096ull

static uint8_t g_hba[512];
static int g_hba_ready = 0;
static int g_slots[WUBU_SWAP_SLOTS];     /* free-slot bitmap: -1 free */
static uint32_t g_swap_count = 0;
/* frame -> slot: index = (phys - base)/PAGE; 0 = not swapped */
static uint16_t g_frame_slot[4096];      /* covers the first 16 MB of
                                            vmm frames */
/* swapped VA -> slot+1 (the fault path looks up by VA) */
static uint16_t g_va_slot[WUBU_SWAP_SLOTS];
static uint64_t g_va_addr[WUBU_SWAP_SLOTS];

static void disk_ensure(void)
{
    if (!g_hba_ready) {
        if (ahci_hba_init(g_hba) == 0 &&
            ahci_enumerate_ports(g_hba) > 0 &&
            ahci_port_init(g_hba, 0) == 0)
            ahci_sim_disk_create(g_hba, 0, 8);
        g_hba_ready = 1;
    }
}

static void slot_init(void)
{
    static int done = 0;
    if (done) return;
    done = 1;
    for (int i = 0; i < WUBU_SWAP_SLOTS; i++) g_slots[i] = -1;
    for (int i = 0; i < 4096; i++) g_frame_slot[i] = 0;
}

int wubu_swap_slot_of(uint64_t phys)
{
    if (phys < WUBU_VMM_PHYS_BASE) return -1;
    uint64_t idx = (phys - WUBU_VMM_PHYS_BASE) / PAGE;
    if (idx >= 4096) return -1;
    uint16_t s = g_frame_slot[idx];
    return s ? (int)(s - 1) : -1;      /* stored as slot+1 (0 = absent) */
}

int wubu_swap_out(uint64_t va, uint64_t phys)
{
    slot_init();
    if (phys < WUBU_VMM_PHYS_BASE) return -1;
    uint64_t idx = (phys - WUBU_VMM_PHYS_BASE) / PAGE;
    if (idx >= 4096) return -1;
    if (g_frame_slot[idx]) return (int)g_frame_slot[idx];  /* already out */

    /* find a free slot */
    int slot = -1;
    for (int i = 0; i < WUBU_SWAP_SLOTS; i++) {
        if (g_slots[i] == -1) { slot = i; break; }
    }
    if (slot < 0) return -1;

    disk_ensure();
    if (!g_hba_ready) return -1;
    uint64_t lba = WUBU_SWAP_SECTOR + (uint64_t)slot;
    if (ahci_write(g_hba, 0, lba, 1, (const void *)(uintptr_t)phys) != 1)
        return -1;

    g_slots[slot] = 1;
    g_frame_slot[idx] = (uint16_t)(slot + 1);
    g_va_slot[slot] = (uint16_t)(slot + 1);
    g_va_addr[slot] = va;
    g_swap_count++;

    /* invalidate the VA's mapping (the fault path will swap it back) */
    wubu_vmm_unmap(va);
    return slot;
}

int wubu_swap_in(uint64_t va, uint32_t slot)
{
    slot_init();
    if (slot >= WUBU_SWAP_SLOTS || g_slots[slot] == -1) return -1;

    disk_ensure();
    if (!g_hba_ready) return -1;

    /* find the frame that owns this slot */
    int frame = -1;
    for (int i = 0; i < 4096; i++) {
        if (g_frame_slot[i] == (uint16_t)(slot + 1)) { frame = i; break; }
    }
    if (frame < 0) return -1;

    uint64_t phys = WUBU_VMM_PHYS_BASE + (uint64_t)frame * PAGE;
    uint64_t lba = WUBU_SWAP_SECTOR + (uint64_t)slot;
    if (ahci_read(g_hba, 0, lba, 1, (void *)(uintptr_t)phys) != 1)
        return -1;

    /* restore the mapping + release the slot */
    extern int wubu_vmm_map_page(uint64_t, uint64_t, uint32_t);
    if (wubu_vmm_map_page(va, phys, 3) != 0) return -1;
    g_slots[slot] = -1;
    g_frame_slot[frame] = 0;
    g_va_slot[slot] = 0;
    g_va_addr[slot] = 0;
    g_swap_count--;
    return 0;
}

/* The slot swapped out at `va`, or -1 (the fault path's lookup). */
int wubu_swap_va_slot(uint64_t va)
{
    va &= ~(PAGE - 1);
    for (int i = 0; i < WUBU_SWAP_SLOTS; i++) {
        if (g_va_slot[i] && g_va_addr[i] == va)
            return i;
    }
    return -1;
}

uint32_t wubu_swap_count(void) { return g_swap_count; }
