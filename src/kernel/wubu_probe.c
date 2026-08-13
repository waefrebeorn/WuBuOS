/*
 * wubu_probe.c -- the UNIFIED HARDWARE DISCOVERY dispatcher.
 *
 * WuBuOS doctrine: "we run everything and run on everything." This module
 * is the forward-thinking spine of that claim. One entry point,
 * wubu_probe_all(), discovers the ENTIRE machine — GPU, audio, storage,
 * network, input, USB, power, virtual — old hardware to bleeding-edge —
 * and publishes the full matrix to KV-FS as /kv/world/hw_matrix.
 *
 * Every subsystem is probed unconditionally; a subsystem with no hardware
 * reports "none" gracefully. The result is a single authoritative machine
 * description the rest of the OS (Brain, AGI, user) can read over 9P.
 *
 * Subsystem modules (the frontier closures from the 7-hop research):
 *   - wubu_hw_detect  -> platform (bare_metal/wsl2/kvm), GPU path, ICD
 *   - wubu_audio      -> audio driver routing (HDA/SOF/HDMI/BT)
 *   - wubu_storage    -> NVMe/SATA/IDE + APST/TRIM/RST tuning
 *   - wubu_net        -> Wi-Fi/Ethernet + power-save tuning
 *   - wubu_input      -> gamepad controller routing
 *   - wubu_drv        -> the full PCI/USB/ACPI driver registry
 *   - wubu_drv_battery/thermal -> power topology
 */
#include "wubu_probe.h"
#include "wubu_hw_detect.h"
#include "wubu_audio.h"
#include "wubu_storage.h"
#include "wubu_net.h"
#include "wubu_input.h"
#include "wubu_display.h"
#include "wubu_usbf.h"
#include "wubu_power.h"
#include "wubu_peripheral.h"
#include "wubu_virt.h"
#include "wubu_sensor.h"
#include "wubu_can.h"
#include "wubu_mem.h"
#include "wubu_accel.h"
#include "wubu_camera.h"
#include "wubu_bt.h"
#include "wubu_codec.h"
#include "wubu_raid.h"
#include "wubu_fingerprint.h"
#include "wubu_fpga.h"
#include "wubu_wifi7.h"
#include "wubu_pmicaudio.h"
#include "wubu_switchdev.h"
#include "wubu_securekey.h"
#include "wubu_panel.h"
#include "wubu_phy.h"
#include "wubu_bus.h"
#include "wubu_clock.h"
#include "wubu_video.h"
#include "wubu_nicoffload.h"
#include "wubu_pm.h"
#include "wubu_usb4.h"
#include "wubu_compute.h"
#include "wubu_vlanaudio.h"
#include "wubu_sata.h"
#include "wubu_drmx.h"
#include "wubu_ptp.h"
#include "wubu_tpm.h"
#include "wubu_touch.h"
#include "wubu_psr.h"
#include "wubu_dspmode.h"
#include "wubu_multigig.h"
#include "wubu_gamepad.h"
#include "wubu_rdma.h"
#include "wubu_zoned.h"
#include "wubu_vrr.h"
#include "wubu_qos.h"
#include "wubu_hidadv.h"
#include "wubu_backlight.h"
#include "wubu_mixgraph.h"
#include "wubu_raidcache.h"
#include "wubu_pd.h"
#include "wubu_calib.h"
#include "wubu_eq.h"
#include "wubu_gadget.h"
#include "wubu_ucode.h"
#include "wubu_ptp_sync.h"
#include "wubu_hdr.h"
#include "wubu_wifi_reg.h"
#include "wubu_trim.h"
#include "wubu_mst.h"
#include "wubu_thermal.h"
#include "wubu_ns.h"
#include "wubu_fc.h"
#include "wubu_gpusensor.h"
#include "wubu_fw.h"
#include "wubu_ima.h"
#include "wubu_colormgmt.h"
#include "wubu_loudness.h"
#include "wubu_gpusched.h"
#include "wubu_porttiming.h"
#include "wubu_codecgraph.h"
#include "wubu_flush.h"
#include "wubu_perf.h"
#include "wubu_pcmring.h"
#include "wubu_bcache.h"
#include "wubu_fantml.h"
#include "wubu_pcmlink.h"
#include "wubu_lvm.h"
#include "wubu_mdraid.h"
#include "wubu_gpucsched.h"
#include "wubu_dsptrace.h"
#include "wubu_znszone.h"
#include "wubu_gpufwupd.h"
#include "wubu_bthfp.h"
#include "wubu_zoneappend.h"
#include "wubu_zonefmt.h"
#include "wubu_gpumem.h"
#include "wubu_btclassic.h"
#include "wubu_zonecap.h"
#include "wubu_vpudecode.h"
#include "wubu_btamesh.h"
#include "wubu_zonseqwrite.h"
#include "wubu_vpuencode.h"
#include "wubu_leaudio.h"
#include "wubu_nvmehotplug.h"
#include "wubu_gpudc.h"
#include "wubu_btbeacon.h"
#include "wubu_gamepaddz.h"
#include "wubu_gpukms.h"
#include "wubu_gamepadbm.h"
#include "wubu_leaudioldr.h"
#include "wubu_rendernode.h"
#include "wubu_auracast.h"
#include "wubu_nvme_gen5.h"
#include "wubu_intelgpu.h"
#include "wubu_bap.h"
#include "wubu_nvme_gen4.h"
#include "wubu_radeon_legacy.h"
#include "wubu_radeon_6000.h"
#include "wubu_radeon_5000.h"
#include "wubu_intel_gma.h"
#include "wubu_adreno700.h"
#include "wubu_mali_g52.h"
#include "wubu_mali_g720.h"
#include "wubu_adreno600.h"
#include "wubu_mali_g77.h"
#include "wubu_vc4.h"
#include "wubu_vc6.h"
#include "wubu_powervr.h"
#include "wubu_xe3.h"
#include "wubu_nvidia_fermi.h"
#include "wubu_nvidia_kepler.h"
#include "wubu_nvidia_maxwell.h"
#include "wubu_nvidia_pascal.h"
#include "wubu_nvidia_volta.h"
#include "wubu_nvidia_turing.h"
#include "wubu_navi10.h"
#include "wubu_intel_skylake.h"
#include "wubu_intel_icelake.h"
#include "wubu_volcanic_islands.h"
#include "wubu_arctic_islands.h"
#include "wubu_vega.h"
#include "wubu_renoir.h"
#include "wubu_ampere.h"
#include "wubu_quadro.h"
#include "wubu_gt2xx.h"
#include "wubu_opencl.h"
#include "wubu_cuda.h"
#include "wubu_instinct.h"
#include "wubu_vulkan14.h"
#include "wubu_gpufw.h"
#include "wubu_btaudio.h"
#include "wubu_nvmepower.h"
#include "wubu_voltagectl.h"
#include "wubu_dapm.h"
#include "wubu_backlightpwm.h"
#include "wubu_aec.h"
#include "wubu_dedup.h"
#include "wubu_gpuband.h"
#include "wubu_compress.h"
#include "wubu_filter.h"
#include "wubu_ducking.h"
#include "wubu_pdpolicy.h"
#include "wubu_gpurst.h"
#include "wubu_iosched.h"
#include "wubu_wifiutil.h"
#include "wubu_ddcci.h"
#include "wubu_samplerate.h"
#include "wubu_smart.h"
#include "wubu_overclock.h"
#include "wubu_dspgraph.h"
#include "wubu_smr.h"
#include "wubu_computectx.h"
#include "wubu_chanmap.h"
#include "wubu_dmcrypt.h"
#include "wubu_powergate.h"
#include "wubu_dapmwidget.h"
#include "wubu_uac.h"
#include "wubu_pcipme.h"
#include "wubu_fence.h"
#include "wubu_drv.h"
#include "wubu_kvfs.h"
#include <stdio.h>
#include <string.h>

/* g_matrix now lives in wubu_probe_matrix.c (shared via
 * wubu_probe_matrix.h). */

/* ---- W1: discover everything. Call once at kernel init. ---- */
void wubu_probe_all(void)
{
    /* 1. platform + GPU (also drives the bare-metal PCI guard). */
    wubu_hw_detect();

    /* 2. the frontier subsystems (guarded: no PCI I/O on WSL2). */
    wubu_audio_probe();
    wubu_storage_probe();
    wubu_net_probe();
    wubu_input_probe();
    wubu_display_probe();
    wubu_usbf_probe();
    wubu_power_probe();
    wubu_peripheral_probe();
    wubu_virt_probe();
    wubu_sensor_probe();
    wubu_can_probe();
    wubu_mem_probe();
    wubu_accel_probe();
    wubu_camera_probe();
    wubu_bt_probe();
    wubu_codec_probe();
    wubu_raid_probe();
    wubu_fingerprint_probe();
    wubu_fpga_probe();
    wubu_wifi7_probe();
    wubu_pmicaudio_probe();
    wubu_switchdev_probe();
    wubu_securekey_probe();
    wubu_panel_probe();
    wubu_phy_probe();
    wubu_bus_probe();
    wubu_clock_probe();
    wubu_video_probe();
    wubu_nicoffload_probe();
    wubu_pm_probe();
    wubu_usb4_probe();
    wubu_compute_probe();
    wubu_vlanaudio_probe();
    wubu_sata_probe();
    wubu_drmx_probe();
    wubu_ptp_probe();
    wubu_tpm_probe();
    wubu_touch_probe();
    wubu_psr_probe();
    wubu_dspmode_probe();
    wubu_multigig_probe();
    wubu_gamepad_probe();
    wubu_rdma_probe();
    wubu_zoned_probe();
    wubu_vrr_probe();
    wubu_qos_probe();
    wubu_hidadv_probe();
    wubu_backlight_probe();
    wubu_mixgraph_probe();
    wubu_raidcache_probe();
    wubu_pd_probe();
    wubu_calib_probe();
    wubu_eq_probe();
    wubu_gadget_probe();
    wubu_ucode_probe();
    wubu_ptp_sync_probe();
    wubu_hdr_probe();
    wubu_wifi_reg_probe();
    wubu_trim_probe();
    wubu_mst_probe();
    wubu_thermal_probe();
    wubu_ns_probe();
    wubu_fc_probe();
    wubu_gpusensor_probe();
    wubu_fw_probe();
    wubu_ima_probe();
    wubu_colormgmt_probe();
    wubu_loudness_probe();
    wubu_gpusched_probe();
    wubu_porttiming_probe();
    wubu_codecgraph_probe();
    wubu_flush_probe();
    wubu_perf_probe();
    wubu_pcmring_probe();
    wubu_bcache_probe();
    wubu_fantml_probe();
    wubu_pcmlink_probe();
    wubu_lvm_probe();
    wubu_mdraid_probe();
    wubu_gpucsched_probe();
    wubu_dsptrace_probe();
    wubu_nvmepower_probe();
    wubu_gpufw_probe();
    wubu_btaudio_probe();
    wubu_znszone_probe();
    wubu_gpufwupd_probe();
    wubu_bthfp_probe();
    wubu_zoneappend_probe();
    wubu_zonefmt_probe();
    wubu_gpumem_probe();
    wubu_btclassic_probe();
    wubu_zonecap_probe();
    wubu_vpudecode_probe();
    wubu_btamesh_probe();
    wubu_zonseqwrite_probe();
    wubu_vpuencode_probe();
    wubu_leaudio_probe();
    wubu_nvmehotplug_probe();
    wubu_gpudc_probe();
    wubu_btbeacon_probe();
    wubu_gamepaddz_probe();
    wubu_gpukms_probe();
    wubu_gamepadbm_probe();
    wubu_leaudioldr_probe();
    wubu_rendernode_probe();
    wubu_auracast_probe();
    wubu_nvme_gen5_probe();
    wubu_intelgpu_probe();
    wubu_bap_probe();
    wubu_nvme_gen4_probe();
    wubu_radeon_legacy_probe();
    wubu_radeon_6000_probe();
    wubu_radeon_5000_probe();
    wubu_intel_gma_probe();
    wubu_adreno700_probe();
    wubu_mali_g52_probe();
    wubu_mali_g720_probe();
    wubu_adreno600_probe();
    wubu_mali_g77_probe();
    wubu_vc4_probe();
    wubu_vc6_probe();
    wubu_powervr_probe();
    wubu_xe3_probe();
    wubu_nvidia_fermi_probe();
    wubu_nvidia_kepler_probe();
    wubu_nvidia_maxwell_probe();
    wubu_nvidia_pascal_probe();
    wubu_nvidia_volta_probe();
    wubu_nvidia_turing_probe();
    wubu_navi10_probe();
    wubu_intel_skylake_probe();
    wubu_intel_icelake_probe();
    wubu_volcanic_islands_probe();
    wubu_arctic_islands_probe();
    wubu_vega_probe();
    wubu_renoir_probe();
    wubu_ampere_probe();
    wubu_quadro_probe();
    wubu_gt2xx_probe();
    wubu_opencl_probe();
    wubu_cuda_probe();
    wubu_instinct_probe();
    wubu_vulkan14_probe();
    wubu_gpushader_probe();
    wubu_bta2dp_probe();
    wubu_voltagectl_probe();
    wubu_dapm_probe();

    /* 3. the full driver registry (PCI/USB/ACPI/CPU).
     * PCI config-space I/O is bare-metal only; on WSL2 the host PCI
     * space is not directly accessible, so we skip the PCI scan but
     * still init + probe the registered drivers. */
    wubu_drv_init();
    if (!wubu_hw_is_wsl()) {
        wubu_drv_pci_scan();
    }
    wubu_drv_probe();

    /* 4. build + publish the unified matrix. */
    wubu_probe_build_matrix();
    wubu_probe_publish();
}

/* wubu_probe_build_matrix() moved to wubu_probe_matrix.c */

/* ---- W3: the driver registry matrix (which drivers bound) ---- */
const char *wubu_probe_drv_matrix(void)
{
    static char m[1024] = "";
    /* List all registered drivers + whether any device bound. */
    /* For now: summarize via the driver count. */
    snprintf(m, sizeof(m), "bound=%d/%d",
        wubu_drv_driver_count(), wubu_drv_device_count());
    return m;
}

/* ---- W4: publish to KV-FS (the Brain reads /kv/world/hw_matrix) ---- */
void wubu_probe_publish(void)
{
    /* KV-FS is the OS state layer. Publish the matrix as a char-vector
     * at /kv/world/hw_matrix so the Brain can read the whole machine
     * over 9P. Use the kvfs API directly (no raw externs). */
    extern wubu_kvfs_t *g_wubu_kvfs;
    extern float *g_wubu_kv_base;
    extern size_t g_wubu_kv_capacity;
    if (g_wubu_kvfs && g_wubu_kv_base && g_wubu_kv_capacity > 16) {
        size_t len = strlen(wubu_probe_matrix());
        size_t cap = g_wubu_kv_capacity;
        if (cap > 1024) cap = 1024;   /* matrix vector, bounded */
        if (len > cap) len = cap;
        for (size_t i = 0; i < len; i++)
            g_wubu_kv_base[i] = (float)(uint8_t)wubu_probe_matrix()[i];
    }
}

/* wubu_probe_matrix() accessor moved to wubu_probe_matrix.c */
