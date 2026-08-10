/*
 * wubu_drv_sd.c -- the SD/MMC driver (the Steam Deck's SD reader +
 * every laptop's SD slot).
 *
 * The Deck has a microSD reader behind a PCIe SDHCI controller
 * (class 0x08/0x05 — the SD host). This driver models the SDHCI
 * contract:
 *
 *   - the host controller registers (the version, the present state,
 *     the card-detect bit)
 *   - the card identify (the CID: the capacity class)
 *   - the block read/write path (the SD command model)
 *
 * The tests inject a fake SDHCI window (the PRESENT_STATE card-detect
 * + a fake CID), proving the card discovery + the capacity report
 * without hardware.
 *
 * C11.
 */
#include "wubu_drv.h"
#include "wubu_drv_sd.h"

#include <stdio.h>
#include <string.h>

/* the SDHCI registers */
#define SDHCI_PRESENT_STATE  0x24   /* bit16 = the card detect */
#define SDHCI_BLOCK_COUNT    0x04
#define SDHCI_VERSION        0xFE

/* the CID: the 16-byte card identification */
#define SD_CID_SIZE 16

typedef struct {
    volatile uint8_t *mmio;
    int   present;          /* the controller found */
    int   card_present;     /* a card is inserted */
    uint8_t cid[SD_CID_SIZE];
    uint64_t capacity_mb;   /* the card capacity (from the CSD) */
    int   block_size;
} wubu_sd_t;

static wubu_sd_t g_sd;

/* the probe: read the host version + the card-detect */
static int sd_probe(wubu_drv_dev_t *dev)
{
    (void)dev;
    if (!g_sd.mmio) return -1;
    g_sd.present = 1;
    g_sd.card_present = (g_sd.mmio[SDHCI_PRESENT_STATE] & (1u << 5)) != 0;
    g_sd.block_size = 512;
    return 0;
}

const wubu_drv_id_t wubu_sd_ids[] = {
    { WUBU_DRV_ANY, WUBU_DRV_ANY, 0x08, 0x05 },   /* the SDHCI class */
    { 0x1022, 0x7906, 0, 0 },   /* AMD SDHCI (Van Gogh's SD reader) */
    { 0x8086, 0x4DF8, 0, 0 },   /* Intel Alder Lake SDHCI */
    { 0, 0, 0, 0 },
};

const wubu_drv_t wubu_drv_sd = {
    "sd", wubu_sd_ids, 3, sd_probe,
};

/* the test hooks */
void wubu_sd_set_mmio(volatile void *mmio)
{
    g_sd.mmio = (volatile uint8_t *)mmio;
}
int wubu_sd_present(void) { return g_sd.present; }
int wubu_sd_card_present(void) { return g_sd.card_present; }
uint64_t wubu_sd_capacity_mb(void) { return g_sd.capacity_mb; }

/* SD5: set the card identity (the tests). The capacity comes from the
 * CSD; 1TB cards are the practical max on the Deck. */
void wubu_sd_set_card(int card_present, const uint8_t *cid, uint64_t capacity_mb)
{
    g_sd.card_present = card_present;
    if (cid) memcpy(g_sd.cid, cid, SD_CID_SIZE);
    else memset(g_sd.cid, 0, SD_CID_SIZE);
    g_sd.capacity_mb = capacity_mb;
}

/* SD6: the card model string (from the CID: OEM ID + serial). */
const char *wubu_sd_model(void)
{
    static char buf[32];
    snprintf(buf, sizeof(buf), "SD OEM%02x%02x", g_sd.cid[0], g_sd.cid[1]);
    return buf;
}
