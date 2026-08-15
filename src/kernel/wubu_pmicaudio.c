/*
 * wubu_pmicaudio.c -- kernel-owned PMIC + audio amp/DAC driver routing.
 *
 * Three connected analog-front-end families:
 *   - PMIC (power management IC): regulators, charge, fuel gauge,
 *     watchdogs. Exposed via the regulator subsystem + /sys/class/regulator.
 *   - Audio DAC: ESS Sabre (es9038/es9018), AKM (ak4490/ak4497),
 *     Burr-Brown (pcm1792), Cirrus (cs4398) — the high-fi output stage.
 *   - Audio amplifier: TI (tas5805/tpa3116), NXP (tda7), Maxim — the
 *     speaker/headphone driver.
 *
 * WuBuOS owns this: detect the PMIC + audio DAC/amp (I2C/regulator sysfs),
 * route to the right driver, and expose the analog-front-end topology.
 *
 * Research (Kevin-Bacon 7-hop on the PMIC/audio-analog frontier):
 *   - PMIC: regulator subsystem (regulator.ko), pmic-8xxx, pm8921, bq25890
 *   - Audio DAC: es9038q2m (sabre), ak4490 (AKM), pcm1792 (TI),
 *     cs4398 (Cirrus), via I2S/ASoC
 *   - Audio amp: tas5805m, tpa3116 (TI I2C amps), max98357a, tda7498
 */
#include "wubu_pmicaudio.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_pmic = 0;
static int  g_audio_dac = 0;
static int  g_audio_amp = 0;
static int  g_regulator = 0;
static char g_dac_drv[32] = "";
static char g_amp_drv[32] = "";
static char g_pmic_drv[32] = "";

/* ---- W1: probe the PMIC/audio-analog topology ---- */
void wubu_pmicaudio_probe(void)
{
    g_pmic = 0; g_audio_dac = 0; g_audio_amp = 0; g_regulator = 0;
    g_dac_drv[0] = '\0'; g_amp_drv[0] = '\0'; g_pmic_drv[0] = '\0';

#ifdef WUBU_HOSTED
    /* Regulator subsystem (PMIC rails) present? */
    if (access("/sys/class/regulator", R_OK) == 0) {
        g_regulator = 1; g_pmic = 1;
        strcpy(g_pmic_drv, "regulator");
    }
    /* PMIC drivers (Qualcomm pmic-8xxx, TI bq25890 charger)? */
    if (access("/sys/bus/platform/drivers/qcom-spmi-pmic", R_OK) == 0 ||
        access("/sys/bus/i2c/drivers/bq25890", R_OK) == 0) {
        g_pmic = 1;
        if (!g_pmic_drv[0]) strcpy(g_pmic_drv, "pmic");
    }
    /* Audio DAC (ESS Sabre / AKM / TI) via I2C/ASoC? */
    if (access("/sys/bus/i2c/drivers/es9038q2m", R_OK) == 0 ||
        access("/sys/bus/i2c/drivers/ak4490", R_OK) == 0 ||
        access("/sys/bus/i2c/drivers/pcm1792", R_OK) == 0 ||
        access("/sys/bus/i2c/drivers/cs4398", R_OK) == 0) {
        g_audio_dac = 1;
        if (access("/sys/bus/i2c/drivers/es9038q2m", R_OK) == 0)
            strcpy(g_dac_drv, "es9038q2m");
        else if (access("/sys/bus/i2c/drivers/ak4490", R_OK) == 0)
            strcpy(g_dac_drv, "ak4490");
        else if (access("/sys/bus/i2c/drivers/pcm1792", R_OK) == 0)
            strcpy(g_dac_drv, "pcm1792");
        else
            strcpy(g_dac_drv, "cs4398");
    }
    /* Audio amplifier (TI tas5805 / tpa3116, Maxim max98357)? */
    if (access("/sys/bus/i2c/drivers/tas5805m", R_OK) == 0 ||
        access("/sys/bus/i2c/drivers/tpa3116", R_OK) == 0 ||
        access("/sys/bus/i2c/drivers/max98357a", R_OK) == 0) {
        g_audio_amp = 1;
        if (access("/sys/bus/i2c/drivers/tas5805m", R_OK) == 0)
            strcpy(g_amp_drv, "tas5805m");
        else if (access("/sys/bus/i2c/drivers/tpa3116", R_OK) == 0)
            strcpy(g_amp_drv, "tpa3116");
        else
            strcpy(g_amp_drv, "max98357a");
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_pmicaudio_pmic(void)     { return g_pmic; }
int  wubu_pmicaudio_dac(void)      { return g_audio_dac; }
int  wubu_pmicaudio_amp(void)      { return g_audio_amp; }
int  wubu_pmicaudio_regulator(void){ return g_regulator; }
const char *wubu_pmicaudio_pmic_driver(void){ return g_pmic_drv[0] ? g_pmic_drv : NULL; }
const char *wubu_pmicaudio_dac_driver(void){ return g_dac_drv[0] ? g_dac_drv : NULL; }
const char *wubu_pmicaudio_amp_driver(void){ return g_amp_drv[0] ? g_amp_drv : NULL; }

/* ---- W3: driver routing ---- */
const char *wubu_pmicaudio_dac_route(const char *dac)
{
    if (!dac) return NULL;
    if (strstr(dac, "es9038") || strstr(dac, "sabre")) return "es9038q2m";
    if (strstr(dac, "ak4490") || strstr(dac, "akm"))   return "ak4490";
    if (strstr(dac, "pcm1792")) return "pcm1792";
    if (strstr(dac, "cs4398") || strstr(dac, "cirrus")) return "cs4398";
    return "snd_soc_dac";
}

const char *wubu_pmicaudio_amp_route(const char *amp)
{
    if (!amp) return NULL;
    if (strstr(amp, "tas5805")) return "tas5805m";
    if (strstr(amp, "tpa3116")) return "tpa3116";
    if (strstr(amp, "max98357")) return "max98357a";
    if (strstr(amp, "tda7498")) return "tda7498";
    return "snd_soc_amp";
}

/* ---- W4: summary ---- */
int wubu_pmicaudio_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "pmicaudio[pmic=%d(%s) reg=%d dac=%d(%s) amp=%d(%s)]",
        g_pmic, wubu_pmicaudio_pmic_driver() ? wubu_pmicaudio_pmic_driver() : "none",
        g_regulator,
        g_audio_dac, wubu_pmicaudio_dac_driver() ? wubu_pmicaudio_dac_driver() : "none",
        g_audio_amp, wubu_pmicaudio_amp_driver() ? wubu_pmicaudio_amp_driver() : "none");
}
