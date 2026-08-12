/*
 * wubu_probe_selftest.c -- verifies the UNIFIED HARDWARE DISCOVERY.
 *
 * Tests:
 * 1. wubu_probe_all() runs every subsystem without crashing
 * 2. The machine matrix is built and non-empty
 * 3. The matrix contains the GPU/audio/storage/net/input fragments
 * 4. The driver registry count is sane
 */
#include "wubu_probe.h"
#include "wubu_hw_detect.h"
#include "wubu_audio.h"
#include "wubu_storage.h"
#include "wubu_net.h"
#include "wubu_input.h"
#include <stdio.h>
#include <string.h>

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else { printf("  ok: %s\n", msg); passed++; } \
} while(0)

static int failures = 0;
static int passed = 0;

int main(void)
{
    printf("=== wubu_probe_selftest ===\n\n");

    /* 1. Discover everything (the forward-thinking dispatcher). */
    wubu_probe_all();

    /* 2. Matrix built. */
    const char *m = wubu_probe_matrix();
    printf("%s\n", m);
    CHECK(m != NULL && m[0] != '\0', "machine matrix built");

    /* 3. Matrix contains the subsystem fragments. */
    CHECK(strstr(m, "WuBuOS machine matrix") != NULL,
          "matrix has the header");
    CHECK(strstr(m, "audio[") != NULL, "matrix has audio fragment");
    CHECK(strstr(m, "storage[") != NULL, "matrix has storage fragment");
    CHECK(strstr(m, "net[") != NULL, "matrix has net fragment");
    CHECK(strstr(m, "input[") != NULL, "matrix has input fragment");
    CHECK(strstr(m, "power[") != NULL, "matrix has power fragment");
    CHECK(strstr(m, "display[") != NULL, "matrix has display fragment");
    CHECK(strstr(m, "usb[") != NULL, "matrix has usb fragment");
    CHECK(strstr(m, "peri[") != NULL, "matrix has peripheral fragment");
    CHECK(strstr(m, "virt[") != NULL, "matrix has virt fragment");
    CHECK(strstr(m, "sensor[") != NULL, "matrix has sensor fragment");
    CHECK(strstr(m, "can[") != NULL, "matrix has can fragment");
    CHECK(strstr(m, "mem[") != NULL, "matrix has mem fragment");
    CHECK(strstr(m, "accel[") != NULL, "matrix has accel fragment");
    CHECK(strstr(m, "camera[") != NULL, "matrix has camera fragment");
    CHECK(strstr(m, "bt[") != NULL, "matrix has bt fragment");
    CHECK(strstr(m, "codec[") != NULL, "matrix has codec fragment");
    CHECK(strstr(m, "raid[") != NULL, "matrix has raid fragment");
    CHECK(strstr(m, "fp[") != NULL, "matrix has fingerprint fragment");
    CHECK(strstr(m, "fpga[") != NULL, "matrix has fpga fragment");
    CHECK(strstr(m, "wifi7[") != NULL, "matrix has wifi7 fragment");
    CHECK(strstr(m, "pmicaudio[") != NULL, "matrix has pmicaudio fragment");
    CHECK(strstr(m, "sw[") != NULL, "matrix has switchdev fragment");
    CHECK(strstr(m, "sec[") != NULL, "matrix has securekey fragment");
    CHECK(strstr(m, "panel[") != NULL, "matrix has panel fragment");
    CHECK(strstr(m, "phy[") != NULL, "matrix has phy fragment");
    CHECK(strstr(m, "bus[") != NULL, "matrix has bus fragment");
    CHECK(strstr(m, "clock[") != NULL, "matrix has clock fragment");
    CHECK(strstr(m, "video[") != NULL, "matrix has video fragment");
    CHECK(strstr(m, "nicoff[") != NULL, "matrix has nicoffload fragment");
    CHECK(strstr(m, "pm[") != NULL, "matrix has pm fragment");
    CHECK(strstr(m, "usb4[") != NULL, "matrix has usb4 fragment");
    CHECK(strstr(m, "compute[") != NULL, "matrix has compute fragment");
    CHECK(strstr(m, "vlanaudio[") != NULL, "matrix has vlanaudio fragment");
    CHECK(strstr(m, "sata[") != NULL, "matrix has sata fragment");
    CHECK(strstr(m, "drmx[") != NULL, "matrix has drmx fragment");
    CHECK(strstr(m, "ptp[") != NULL, "matrix has ptp fragment");
    CHECK(strstr(m, "tpm[") != NULL, "matrix has tpm fragment");
    CHECK(strstr(m, "touch[") != NULL, "matrix has touch fragment");
    CHECK(strstr(m, "psr[") != NULL, "matrix has psr fragment");
    CHECK(strstr(m, "dspmode[") != NULL, "matrix has dspmode fragment");
    CHECK(strstr(m, "mgig[") != NULL, "matrix has multigig fragment");
    CHECK(strstr(m, "gamepad[") != NULL, "matrix has gamepad fragment");
    CHECK(strstr(m, "rdma[") != NULL, "matrix has rdma fragment");
    CHECK(strstr(m, "zoned[") != NULL, "matrix has zoned fragment");
    CHECK(strstr(m, "vrr[") != NULL, "matrix has vrr fragment");
    CHECK(strstr(m, "qos[") != NULL, "matrix has qos fragment");
    CHECK(strstr(m, "hidadv[") != NULL, "matrix has hidadv fragment");
    CHECK(strstr(m, "backlight[") != NULL, "matrix has backlight fragment");
    CHECK(strstr(m, "mixgraph[") != NULL, "matrix has mixgraph fragment");
    CHECK(strstr(m, "raidcache[") != NULL, "matrix has raidcache fragment");
    CHECK(strstr(m, "pd[") != NULL, "matrix has pd fragment");
    CHECK(strstr(m, "calib[") != NULL, "matrix has calib fragment");
    CHECK(strstr(m, "eq[") != NULL, "matrix has eq fragment");
    CHECK(strstr(m, "gadget[") != NULL, "matrix has gadget fragment");
    CHECK(strstr(m, "ucode[") != NULL, "matrix has ucode fragment");
    CHECK(strstr(m, "ptpsync[") != NULL, "matrix has ptpsync fragment");
    CHECK(strstr(m, "hdr[") != NULL, "matrix has hdr fragment");
    CHECK(strstr(m, "wifireg[") != NULL, "matrix has wifireg fragment");
    CHECK(strstr(m, "trim[") != NULL, "matrix has trim fragment");
    CHECK(strstr(m, "mst[") != NULL, "matrix has mst fragment");
    CHECK(strstr(m, "thermal[") != NULL, "matrix has thermal fragment");
    CHECK(strstr(m, "ns[") != NULL, "matrix has ns fragment");
    CHECK(strstr(m, "fc[") != NULL, "matrix has fc fragment");
    CHECK(strstr(m, "gpusensor[") != NULL, "matrix has gpusensor fragment");
    CHECK(strstr(m, "fw[") != NULL, "matrix has fw fragment");
    CHECK(strstr(m, "ima[") != NULL, "matrix has ima fragment");
    CHECK(strstr(m, "colormgmt[") != NULL, "matrix has colormgmt fragment");
    CHECK(strstr(m, "loudness[") != NULL, "matrix has loudness fragment");
    CHECK(strstr(m, "gpusched[") != NULL, "matrix has gpusched fragment");
    CHECK(strstr(m, "porttiming[") != NULL, "matrix has porttiming fragment");
    CHECK(strstr(m, "codecgraph[") != NULL, "matrix has codecgraph fragment");
    CHECK(strstr(m, "flush[") != NULL, "matrix has flush fragment");
        CHECK(strstr(m, "vram[") != NULL, "matrix has vram fragment");
    CHECK(strstr(m, "spdif[") != NULL, "matrix has spdif fragment");
    CHECK(strstr(m, "cmb[") != NULL, "matrix has cmb fragment");
CHECK(strstr(m, "backlightpwm[") != NULL, "matrix has backlightpwm fragment");
    CHECK(strstr(m, "aec[") != NULL, "matrix has aec fragment");
    CHECK(strstr(m, "dedup[") != NULL, "matrix has dedup fragment");
    CHECK(strstr(m, "gpuband[") != NULL, "matrix has gpuband fragment");
    CHECK(strstr(m, "compress[") != NULL, "matrix has compress fragment");
    CHECK(strstr(m, "filter[") != NULL, "matrix has filter fragment");

    /* Matrix fragments: wave 8 (ducking / pdpolicy) */
    CHECK(strstr(m, "ducking[") != NULL, "matrix has ducking fragment");
    CHECK(strstr(m, "pdpolicy[") != NULL, "matrix has pdpolicy fragment");

    /* Matrix fragments: wave 9 (gpurst / iosched / wifiutil) */
    CHECK(strstr(m, "gpurst[") != NULL, "matrix has gpurst fragment");
    CHECK(strstr(m, "iosched[") != NULL, "matrix has iosched fragment");
    CHECK(strstr(m, "wifiutil[") != NULL, "matrix has wifiutil fragment");

    /* Matrix fragments: wave 10 (ddcci / samplerate / smart) */
    CHECK(strstr(m, "ddcci[") != NULL, "matrix has ddcci fragment");
    CHECK(strstr(m, "samplerate[") != NULL, "matrix has samplerate fragment");
    CHECK(strstr(m, "smart[") != NULL, "matrix has smart fragment");

    /* Matrix fragments: wave 11 (overclock / dspgraph / smr) */
    CHECK(strstr(m, "overclock[") != NULL, "matrix has overclock fragment");
    CHECK(strstr(m, "dspgraph[") != NULL, "matrix has dspgraph fragment");
    CHECK(strstr(m, "smr[") != NULL, "matrix has smr fragment");

    /* Matrix fragments: wave 12 (computectx / chanmap / dmcrypt) */
    CHECK(strstr(m, "computectx[") != NULL, "matrix has computectx fragment");
    CHECK(strstr(m, "chanmap[") != NULL, "matrix has chanmap fragment");
    CHECK(strstr(m, "dmcrypt[") != NULL, "matrix has dmcrypt fragment");

    /* Matrix fragments: wave 15 (voltagectl / spdiftx / blkqos) */
    CHECK(strstr(m, "voltagectl[") != NULL, "matrix has voltagectl fragment");
    CHECK(strstr(m, "spdiftx[") != NULL, "matrix has spdiftx fragment");
    CHECK(strstr(m, "blkqos[") != NULL, "matrix has blkqos fragment");

    /* Matrix fragments: wave 16 (memmgr / jackdetect / ioprio) */
    CHECK(strstr(m, "memmgr[") != NULL, "matrix has memmgr fragment");
    CHECK(strstr(m, "jackdetect[") != NULL, "matrix has jackdetect fragment");
    CHECK(strstr(m, "ioprio[") != NULL, "matrix has ioprio fragment");

    /* Matrix fragments: wave 17 (perfmon / pcmplugin / dax) */
    CHECK(strstr(m, "perfmon[") != NULL, "matrix has perfmon fragment");
    CHECK(strstr(m, "pcmplugin[") != NULL, "matrix has pcmplugin fragment");
    CHECK(strstr(m, "dax[") != NULL, "matrix has dax fragment");

    /* Matrix fragments: wave 18 (fbcon / jackstate / storagesched) */
    CHECK(strstr(m, "fbcon[") != NULL, "matrix has fbcon fragment");
    CHECK(strstr(m, "jackstate[") != NULL, "matrix has jackstate fragment");
    CHECK(strstr(m, "storagesched[") != NULL, "matrix has storagesched fragment");

    /* Matrix fragments: wave 19 (fencesync / jackimpedance / writeback) */
    CHECK(strstr(m, "fencesync[") != NULL, "matrix has fencesync fragment");
    CHECK(strstr(m, "jackimpedance[") != NULL, "matrix has jackimpedance fragment");
    CHECK(strstr(m, "writeback[") != NULL, "matrix has writeback fragment");

    /* Matrix fragments: wave 20 (thermalthrottle / compressor / raid5) */
    CHECK(strstr(m, "thermalthrottle[") != NULL, "matrix has thermalthrottle fragment");
    CHECK(strstr(m, "compressor[") != NULL, "matrix has compressor fragment");
    CHECK(strstr(m, "raid5[") != NULL, "matrix has raid5 fragment");

    /* Matrix fragments: wave 21 (smc / ieccontrol / flush2) */
    CHECK(strstr(m, "smc[") != NULL, "matrix has smc fragment");
    CHECK(strstr(m, "ieccontrol[") != NULL, "matrix has ieccontrol fragment");
    CHECK(strstr(m, "flush2[") != NULL, "matrix has flush2 fragment");

    /* Matrix fragments: wave 22 (mmu / dappath / bio) */
    CHECK(strstr(m, "mmu[") != NULL, "matrix has mmu fragment");
    CHECK(strstr(m, "dappath[") != NULL, "matrix has dappath fragment");
    CHECK(strstr(m, "bio[") != NULL, "matrix has bio fragment");

    /* Matrix fragments: wave 23 (encode / spdifstatus / devmapper) */
    CHECK(strstr(m, "encode[") != NULL, "matrix has encode fragment");
    CHECK(strstr(m, "spdifstatus[") != NULL, "matrix has spdifstatus fragment");
    CHECK(strstr(m, "devmapper[") != NULL, "matrix has devmapper fragment");

    /* Matrix fragments: wave 24 (decode / audiofw / nfsmount) */
    CHECK(strstr(m, "decode[") != NULL, "matrix has decode fragment");
    CHECK(strstr(m, "audiofw[") != NULL, "matrix has audiofw fragment");
    CHECK(strstr(m, "nfsmount[") != NULL, "matrix has nfsmount fragment");

    /* Matrix fragments: wave 25 (drm / mixergraph / nfsclient) */
    CHECK(strstr(m, "drm[") != NULL, "matrix has drm fragment");
    CHECK(strstr(m, "mixergraph[") != NULL, "matrix has mixergraph fragment");
    CHECK(strstr(m, "nfsclient[") != NULL, "matrix has nfsclient fragment");

    /* Matrix fragments: wave 26 (vblank / spdifrx / fusefs) */
    CHECK(strstr(m, "vblank[") != NULL, "matrix has vblank fragment");
    CHECK(strstr(m, "spdifrx[") != NULL, "matrix has spdifrx fragment");
    CHECK(strstr(m, "fusefs[") != NULL, "matrix has fusefs fragment");

    /* Matrix fragments: wave 27 (panel / jack / uas) */
    CHECK(strstr(m, "panel[") != NULL, "matrix has panel fragment");
    CHECK(strstr(m, "jack[") != NULL, "matrix has jack fragment");
    CHECK(strstr(m, "uas[") != NULL, "matrix has uas fragment");

    /* Matrix fragments: wave 28 (perf / pcmring / bcache) */
    CHECK(strstr(m, "perf[") != NULL, "matrix has perf fragment");
    CHECK(strstr(m, "pcmring[") != NULL, "matrix has pcmring fragment");
    CHECK(strstr(m, "bcache[") != NULL, "matrix has bcache fragment");

    /* Matrix fragments: wave 29 (fantml / pcmlink / lvm) */
    CHECK(strstr(m, "fantml[") != NULL, "matrix has fantml fragment");
    CHECK(strstr(m, "pcmlink[") != NULL, "matrix has pcmlink fragment");
    CHECK(strstr(m, "lvm[") != NULL, "matrix has lvm fragment");

    /* Matrix fragments: wave 30 (voltagectl / dapm / mdraid) */
    CHECK(strstr(m, "voltagectl[") != NULL, "matrix has voltagectl fragment");
    CHECK(strstr(m, "dapm[") != NULL, "matrix has dapm fragment");
    CHECK(strstr(m, "mdraid[") != NULL, "matrix has mdraid fragment");

    /* Matrix fragments: wave 31 (gpucsched / dsptrace / nvmepower) */
    CHECK(strstr(m, "gpucsched[") != NULL, "matrix has gpucsched fragment");
    CHECK(strstr(m, "dsptrace[") != NULL, "matrix has dsptrace fragment");
    CHECK(strstr(m, "nvmepower[") != NULL, "matrix has nvmepower fragment");

    /* Matrix fragments: wave 32 (gpufwupd / bthfp / zoneappend) */
    CHECK(strstr(m, "gpufwupd[") != NULL, "matrix has gpufwupd fragment");
    CHECK(strstr(m, "btaudio[") != NULL, "matrix has btaudio fragment");
    CHECK(strstr(m, "zoneappend[") != NULL, "matrix has zoneappend fragment");

    /* Matrix fragments: wave 33 (gpushader / bta2dp / zonefmt) */
    CHECK(strstr(m, "gpushader[") != NULL, "matrix has gpushader fragment");
    CHECK(strstr(m, "bta2dp[") != NULL, "matrix has bta2dp fragment");
    CHECK(strstr(m, "zonefmt[") != NULL, "matrix has zonefmt fragment");

    /* Matrix fragments: wave 34 (gpumem / btclassic / zonecap) */
    CHECK(strstr(m, "gpumem[") != NULL, "matrix has gpumem fragment");
    CHECK(strstr(m, "btclassic[") != NULL, "matrix has btclassic fragment");
    CHECK(strstr(m, "zonecap[") != NULL, "matrix has zonecap fragment");

    /* Matrix fragments: wave 35 (vpudecode / btamesh / zonseqwrite) */
    CHECK(strstr(m, "vpudecode[") != NULL, "matrix has vpudecode fragment");
    CHECK(strstr(m, "btamesh[") != NULL, "matrix has btamesh fragment");
    CHECK(strstr(m, "zonseqwrite[") != NULL, "matrix has zonseqwrite fragment");

    /* Matrix fragments: wave 36 (vpuencode / leaudio / nvmehotplug) */
    CHECK(strstr(m, "vpuencode[") != NULL, "matrix has vpuencode fragment");
    CHECK(strstr(m, "leaudio[") != NULL, "matrix has leaudio fragment");
    CHECK(strstr(m, "nvmehotplug[") != NULL, "matrix has nvmehotplug fragment");

    /* Matrix fragments: wave 37 (gpudc / btbeacon / gamepaddz) */
    CHECK(strstr(m, "gpudc[") != NULL, "matrix has gpudc fragment");
    CHECK(strstr(m, "btbeacon[") != NULL, "matrix has btbeacon fragment");
    CHECK(strstr(m, "gamepaddz[") != NULL, "matrix has gamepaddz fragment");

    /* Matrix fragments: wave 38 (gpukms / gamepadbm / leaudioldr) */
    CHECK(strstr(m, "gpukms[") != NULL, "matrix has gpukms fragment");
    CHECK(strstr(m, "gamepadbm[") != NULL, "matrix has gamepadbm fragment");
    CHECK(strstr(m, "leaudioldr[") != NULL, "matrix has leaudioldr fragment");

    /* Matrix fragments: wave 39 (rendernode / auracast / nvme_gen5) */
    CHECK(strstr(m, "rendernode[") != NULL, "matrix has rendernode fragment");
    CHECK(strstr(m, "auracast[") != NULL, "matrix has auracast fragment");
    CHECK(strstr(m, "nvme_gen5[") != NULL, "matrix has nvme_gen5 fragment");

    /* Matrix fragments: wave 40 (intelgpu / bap / nvme_gen4) */
    CHECK(strstr(m, "intelgpu[") != NULL, "matrix has intelgpu fragment");
    CHECK(strstr(m, "bap[") != NULL, "matrix has bap fragment");
    CHECK(strstr(m, "nvme_gen4[") != NULL, "matrix has nvme_gen4 fragment");

    /* Matrix fragments: wave 41 (radeon_legacy / radeon_6000 / radeon_5000 / intel_gma) */
    CHECK(strstr(m, "radeon_legacy[") != NULL, "matrix has radeon_legacy fragment");
    CHECK(strstr(m, "radeon_6000[") != NULL, "matrix has radeon_6000 fragment");
    CHECK(strstr(m, "radeon_5000[") != NULL, "matrix has radeon_5000 fragment");
    CHECK(strstr(m, "intel_gma[") != NULL, "matrix has intel_gma fragment");

    /* Matrix fragments: wave 42 (adreno700 / mali_g52 / mali_g720) */
    CHECK(strstr(m, "adreno700[") != NULL, "matrix has adreno700 fragment");
    CHECK(strstr(m, "mali_g52[") != NULL, "matrix has mali_g52 fragment");
    CHECK(strstr(m, "mali_g720[") != NULL, "matrix has mali_g720 fragment");

    CHECK(strstr(m, "nvidia_maxwell[") != NULL, "matrix has nvidia_maxwell fragment");
    CHECK(strstr(m, "nvidia_pascal[") != NULL, "matrix has nvidia_pascal fragment");
    CHECK(strstr(m, "nvidia_volta[") != NULL, "matrix has nvidia_volta fragment");
    CHECK(strstr(m, "nvidia_turing[") != NULL, "matrix has nvidia_turing fragment");
    CHECK(strstr(m, "navi10[") != NULL, "matrix has navi10 fragment");
    CHECK(strstr(m, "skylake[") != NULL, "matrix has skylake fragment");
    CHECK(strstr(m, "icelake[") != NULL, "matrix has icelake fragment");
    CHECK(strstr(m, "volcanic_islands[") != NULL, "matrix has volcanic_islands fragment");
    CHECK(strstr(m, "arctic_islands[") != NULL, "matrix has arctic_islands fragment");
    CHECK(strstr(m, "vega[") != NULL, "matrix has vega fragment");
    CHECK(strstr(m, "renoir[") != NULL, "matrix has renoir fragment");
    CHECK(strstr(m, "ampere[") != NULL, "matrix has ampere fragment");
    CHECK(strstr(m, "quadro[") != NULL, "matrix has quadro fragment");
    CHECK(strstr(m, "gt2xx[") != NULL, "matrix has gt2xx fragment");
    CHECK(strstr(m, "opencl[") != NULL, "matrix has opencl fragment");
    CHECK(strstr(m, "cuda[") != NULL, "matrix has cuda fragment");
    CHECK(strstr(m, "instinct[") != NULL, "matrix has instinct fragment");
    CHECK(strstr(m, "vulkan14[") != NULL, "matrix has vulkan14 fragment");

    /* Matrix fragments: wave 43 (adreno600 / mali_g77 / vc4) */
    CHECK(strstr(m, "adreno600[") != NULL, "matrix has adreno600 fragment");
    CHECK(strstr(m, "mali_g77[") != NULL, "matrix has mali_g77 fragment");
    CHECK(strstr(m, "vc4[") != NULL, "matrix has vc4 fragment");

    /* Matrix fragments: wave 44 (vc6 / powervr / xe3) */
    CHECK(strstr(m, "vc6[") != NULL, "matrix has vc6 fragment");
    CHECK(strstr(m, "powervr[") != NULL, "matrix has powervr fragment");
    CHECK(strstr(m, "xe3[") != NULL, "matrix has xe3 fragment");

    /* Matrix fragments: wave 13 (powergate / dapmwidget / znszone) */
    CHECK(strstr(m, "powergate[") != NULL, "matrix has powergate fragment");
    CHECK(strstr(m, "dapmwidget[") != NULL, "matrix has dapmwidget fragment");
    CHECK(strstr(m, "znszone[") != NULL, "matrix has znszone fragment");

    /* Matrix fragments: wave 14 (uac / pcipme / fence) */
    CHECK(strstr(m, "uac[") != NULL, "matrix has uac fragment");
    CHECK(strstr(m, "pcipme[") != NULL, "matrix has pcipme fragment");
    CHECK(strstr(m, "fence[") != NULL, "matrix has fence fragment");
    CHECK(strstr(m, "drv[") != NULL, "matrix has driver fragment");

    /* 4. Driver registry is populated. */
    CHECK(wubu_drv_driver_count() > 0, "driver registry has drivers");

    /* 5. Subsystem probes ran (WSL2 = host owns most; must not crash). */
    CHECK(wubu_audio_present() || wubu_hw_is_wsl(),
          "audio probe ran");
    CHECK(wubu_net_has_wifi() || wubu_net_has_eth() || wubu_hw_is_wsl(),
          "net probe ran");

    printf("\n=== PROBE TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
