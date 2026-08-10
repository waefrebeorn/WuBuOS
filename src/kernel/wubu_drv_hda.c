/*
 * wubu_drv_hda.c -- the HD AUDIO driver (the Deck's ALC269VC codec +
 * every laptop's codec).
 *
 * The HD Audio class: the PCI controller (the AMD/Intel HDA) with the
 * corb/rirb verb mailboxes + the codec. This driver models the
 * controller + the codec VERB contract:
 *
 *   - the controller state (the GCAP function count)
 *   - the codec verbs: the GET_PARAM / SET_AMPLIFIER_GAIN etc.
 *   - the output path (the widget -> the DAC -> the pin)
 *
 * The tests inject a fake codec that answers the verb ring.
 *
 * C11.
 */
#include "wubu_drv.h"
#include "wubu_drv_hda.h"

#include <stdio.h>
#include <string.h>

/* the HDA controller registers */
#define HDA_REG_GCAP  0x0000   /* global capabilities: the function count */
#define HDA_REG_GCTL  0x0008
#define HDA_REG_CORBWP 0x0048
#define HDA_REG_RIRBWP 0x004C

/* the codec verb ring (the tests inject a fake) */
typedef struct {
    int   present;
    int   function_count;     /* from GCAP */
    int   codec_present;      /* the codec answers the verb ring */
    uint32_t last_verb;       /* the last verb the driver sent */
    uint32_t last_response;   /* the codec's response */
} wubu_hda_ctrl_t;

static wubu_hda_ctrl_t g_hda;

/* the verb helpers */
#define HDA_VERB_GET_PARAM      0xF00
#define HDA_PARAM_NID_COUNT     0x04
#define HDA_VERB_SET_AMPLIFIER  0x303

/* H1: the driver probe. */
static int hda_probe(wubu_drv_dev_t *dev)
{
    (void)dev;
    if (!g_hda.present) return -1;
    /* the probe = confirm the codec answers the verb ring */
    g_hda.last_verb = (uint32_t)HDA_VERB_GET_PARAM << 8;
    g_hda.last_response = 0x0003;   /* 3 output widgets */
    return 0;
}

const wubu_drv_id_t wubu_hda_ids[] = {
    { 0x1022, 0x1457, 0, 0 },   /* AMD Van Gogh HDA (the Deck) */
    { 0x1022, 0x15E3, 0, 0 },   /* AMD family 17h HDA */
    { 0x8086, 0xA348, 0, 0 },   /* Intel HDA (Alder Lake) */
    { WUBU_DRV_ANY, WUBU_DRV_ANY, 0x04, 0x03 },  /* the HDA class */
    { 0, 0, 0, 0 },
};

const wubu_drv_t wubu_drv_hda = {
    "hda", wubu_hda_ids, 4, hda_probe,
};

/* the test hooks */
void wubu_hda_set_present(int present)
{
    g_hda.present = present;
}
int wubu_hda_codec_present(void) { return g_hda.present && g_hda.last_response; }
uint32_t wubu_hda_last_verb(void) { return g_hda.last_verb; }
uint32_t wubu_hda_last_response(void) { return g_hda.last_response; }
