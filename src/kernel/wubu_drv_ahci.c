/*
 * wubu_drv_ahci.c -- the AHCI (SATA) driver shim for the registry.
 *
 * The registry references wubu_drv_ahci; the real AHCI controller
 * driver is src/kernel/ahci.c (pre-existing). This shim binds the
 * registry's device model to it: the probe records the device's
 * presence so the registry's summary shows the SATA controller.
 *
 * C11.
 */
#include "wubu_drv.h"
#include "wubu_drv_ahci.h"

#include <stdio.h>

static int g_ahci_present;

static int ahci_probe(wubu_drv_dev_t *dev)
{
    (void)dev;
    g_ahci_present = 1;
    return 0;
}

const wubu_drv_id_t wubu_ahci_ids[] = {
    { WUBU_DRV_ANY, WUBU_DRV_ANY, 0x01, 0x06 },  /* the SATA class */
    { 0, 0, 0, 0 },
};

const wubu_drv_t wubu_drv_ahci = {
    "ahci", wubu_ahci_ids, 1, ahci_probe,
};

int wubu_ahci_present(void) { return g_ahci_present; }
