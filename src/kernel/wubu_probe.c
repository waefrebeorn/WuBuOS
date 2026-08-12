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

/* ---- the matrix (built by wubu_probe_all) ---- */
static char g_matrix[16384] = "";

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

/* ---- W2: build the human-readable matrix string ---- */
void wubu_probe_build_matrix(void)
{
    char hw[512] = "";
    wubu_hw_summary(hw, sizeof(hw));

    char audio[160] = "";
    snprintf(audio, sizeof(audio),
        "audio[drv=%s hdmi=%d bt=%d]",
        wubu_audio_driver() ? wubu_audio_driver() : "none",
        wubu_audio_is_hdmi(), wubu_audio_has_bt());

    char storage[160] = "";
    snprintf(storage, sizeof(storage),
        "storage[nvme=%d sata=%d rst=%d qd=%d]",
        wubu_storage_has_nvme(), wubu_storage_has_sata(),
        wubu_storage_has_raid_rst(), wubu_storage_queue_depth());

    char net[160] = "";
    snprintf(net, sizeof(net),
        "net[wifi=%d eth=%d 2g5=%d ps=%s]",
        wubu_net_has_wifi(), wubu_net_has_eth(), wubu_net_has_2g5(),
        wubu_net_power_save_disable() ? "off" : "default");

    char input[160] = "";
    snprintf(input, sizeof(input),
        "input[gamepad=%d mouse=%dHz]",
        wubu_input_has_controller(), wubu_input_poll_hz());

    char display[160] = "";
    snprintf(display, sizeof(display),
        "display[drv=%s chip=%s render=%d atomic=%d]",
        wubu_display_driver() ? wubu_display_driver() : "none",
        wubu_display_chip_name() ? wubu_display_chip_name() : "none",
        wubu_display_has_render_node(),
        wubu_display_atomic_modeset());

    char usb[160] = "";
    wubu_usbf_summary(usb, sizeof(usb));

    char power[160] = "";
    wubu_power_summary(power, sizeof(power));

    char peri[160] = "";
    wubu_peripheral_summary(peri, sizeof(peri));

    char virt[160] = "";
    wubu_virt_summary(virt, sizeof(virt));

    char sensor[160] = "";
    wubu_sensor_summary(sensor, sizeof(sensor));

    char can[160] = "";
    wubu_can_summary(can, sizeof(can));

    char mem[160] = "";
    wubu_mem_summary(mem, sizeof(mem));

    char accel[160] = "";
    wubu_accel_summary(accel, sizeof(accel));

    char camera[160] = "";
    wubu_camera_summary(camera, sizeof(camera));

    char bt[160] = "";
    wubu_bt_summary(bt, sizeof(bt));

    char codec[160] = "";
    wubu_codec_summary(codec, sizeof(codec));

    char raid[160] = "";
    wubu_raid_summary(raid, sizeof(raid));

    char fp[160] = "";
    wubu_fingerprint_summary(fp, sizeof(fp));

    char fpga[160] = "";
    wubu_fpga_summary(fpga, sizeof(fpga));

    char wifi7[160] = "";
    wubu_wifi7_summary(wifi7, sizeof(wifi7));

    char pmicaudio[160] = "";
    wubu_pmicaudio_summary(pmicaudio, sizeof(pmicaudio));

    char sw[160] = "";
    wubu_switchdev_summary(sw, sizeof(sw));

    char sec[160] = "";
    wubu_securekey_summary(sec, sizeof(sec));

    char panel[160] = "";
    wubu_panel_summary(panel, sizeof(panel));

    char phy[160] = "";
    wubu_phy_summary(phy, sizeof(phy));

    char bus[160] = "";
    wubu_bus_summary(bus, sizeof(bus));

    char rtc[160] = "";
    wubu_clock_summary(rtc, sizeof(rtc));

    char video[160] = "";
    wubu_video_summary(video, sizeof(video));

    char nicoff[160] = "";
    wubu_nicoffload_summary(nicoff, sizeof(nicoff));

    char pm[160] = "";
    wubu_pm_summary(pm, sizeof(pm));

    char usb4[160] = "";
    wubu_usb4_summary(usb4, sizeof(usb4));

    char compute[160] = "";
    wubu_compute_summary(compute, sizeof(compute));

    char vlanaudio[160] = "";
    wubu_vlanaudio_summary(vlanaudio, sizeof(vlanaudio));

    char sata[160] = "";
    wubu_sata_summary(sata, sizeof(sata));

    char drmx[160] = "";
    wubu_drmx_summary(drmx, sizeof(drmx));

    char ptp[160] = "";
    wubu_ptp_summary(ptp, sizeof(ptp));

    char tpm[160] = "";
    wubu_tpm_summary(tpm, sizeof(tpm));

    char touch[160] = "";
    wubu_touch_summary(touch, sizeof(touch));

    char psr[160] = "";
    wubu_psr_summary(psr, sizeof(psr));

    char dspmode[160] = "";
    wubu_dspmode_summary(dspmode, sizeof(dspmode));

    char mgig[160] = "";
    wubu_multigig_summary(mgig, sizeof(mgig));

    char gamepad[160] = "";
    wubu_gamepad_summary(gamepad, sizeof(gamepad));

    char rdma[160] = "";
    wubu_rdma_summary(rdma, sizeof(rdma));

    char zoned[160] = "";
    wubu_zoned_summary(zoned, sizeof(zoned));

    char vrr[160] = "";
    wubu_vrr_summary(vrr, sizeof(vrr));

    char qos[160] = "";
    wubu_qos_summary(qos, sizeof(qos));

    char hid[160] = "";
    wubu_hidadv_summary(hid, sizeof(hid));

    char backlight[160] = "";
    wubu_backlight_summary(backlight, sizeof(backlight));

    char mixgraph[160] = "";
    wubu_mixgraph_summary(mixgraph, sizeof(mixgraph));

    char raidcache[160] = "";
    wubu_raidcache_summary(raidcache, sizeof(raidcache));

    char pd[160] = "";
    wubu_pd_summary(pd, sizeof(pd));

    char calib[160] = "";
    wubu_calib_summary(calib, sizeof(calib));

    char eq[160] = "";
    wubu_eq_summary(eq, sizeof(eq));

    char gadget[160] = "";
    wubu_gadget_summary(gadget, sizeof(gadget));

    char ucode[160] = "";
    wubu_ucode_summary(ucode, sizeof(ucode));

    char ptpsync[160] = "";
    wubu_ptp_sync_summary(ptpsync, sizeof(ptpsync));

    char hdr[160] = "";
    wubu_hdr_summary(hdr, sizeof(hdr));

    char wifireg[160] = "";
    wubu_wifi_reg_summary(wifireg, sizeof(wifireg));

    char trim[160] = "";
    wubu_trim_summary(trim, sizeof(trim));

    char mst[160] = "";
    wubu_mst_summary(mst, sizeof(mst));

    char thermal[160] = "";
    wubu_thermal_summary(thermal, sizeof(thermal));

    char ns[160] = "";
    wubu_ns_summary(ns, sizeof(ns));

    char fc[160] = "";
    wubu_fc_summary(fc, sizeof(fc));

    char gpusensor[160] = "";
    wubu_gpusensor_summary(gpusensor, sizeof(gpusensor));

    char fw[160] = "";
    wubu_fw_summary(fw, sizeof(fw));

    char ima[160] = "";
    wubu_ima_summary(ima, sizeof(ima));

    char colormgmt[160] = "";
    wubu_colormgmt_summary(colormgmt, sizeof(colormgmt));

    char loudness[160] = "";
    wubu_loudness_summary(loudness, sizeof(loudness));

    char gpusched[160] = "";
    wubu_gpusched_summary(gpusched, sizeof(gpusched));

    char porttiming[160] = "";
    wubu_porttiming_summary(porttiming, sizeof(porttiming));

    char codecgraph[160] = "";
    wubu_codecgraph_summary(codecgraph, sizeof(codecgraph));

    char flush[160] = "";
    wubu_flush_summary(flush, sizeof(flush));

    char perf[160] = "";
    wubu_perf_summary(perf, sizeof(perf));

    char pcmring[160] = "";
    wubu_pcmring_summary(pcmring, sizeof(pcmring));

    char bcache[160] = "";
    wubu_bcache_summary(bcache, sizeof(bcache));

    char fantml[160] = "";
    wubu_fantml_summary(fantml, sizeof(fantml));

    char pcmlink[160] = "";
    wubu_pcmlink_summary(pcmlink, sizeof(pcmlink));

    char lvm[160] = "";
    wubu_lvm_summary(lvm, sizeof(lvm));

    char voltagectl[160] = "";
    wubu_voltagectl_summary(voltagectl, sizeof(voltagectl));

    char dapm[160] = "";
    wubu_dapm_summary(dapm, sizeof(dapm));

    char mdraid[160] = "";
    wubu_mdraid_summary(mdraid, sizeof(mdraid));

    char gpucsched[160] = "";
    wubu_gpucsched_summary(gpucsched, sizeof(gpucsched));

    char dsptrace[160] = "";
    wubu_dsptrace_summary(dsptrace, sizeof(dsptrace));

    char nvmepower[160] = "";
    wubu_nvmepower_summary(nvmepower, sizeof(nvmepower));

    char gpufw[160] = "";
    wubu_gpufw_summary(gpufw, sizeof(gpufw));

    char btaudio[160] = "";
    wubu_btaudio_summary(btaudio, sizeof(btaudio));

    char znszone[160] = "";
    wubu_znszone_summary(znszone, sizeof(znszone));

    char gpufwupd[160] = "";
    wubu_gpufwupd_summary(gpufwupd, sizeof(gpufwupd));

    char bthfp[160] = "";
    wubu_bthfp_summary(bthfp, sizeof(bthfp));

    char zoneappend[160] = "";
    wubu_zoneappend_summary(zoneappend, sizeof(zoneappend));

    char gpushader[160] = "";
    wubu_gpushader_summary(gpushader, sizeof(gpushader));

    char bta2dp[160] = "";
    wubu_bta2dp_summary(bta2dp, sizeof(bta2dp));

    char zonefmt[160] = "";
    wubu_zonefmt_summary(zonefmt, sizeof(zonefmt));

    char gpumem[160] = "";
    wubu_gpumem_summary(gpumem, sizeof(gpumem));

    char btclassic[160] = "";
    wubu_btclassic_summary(btclassic, sizeof(btclassic));

    char zonecap[160] = "";
    wubu_zonecap_summary(zonecap, sizeof(zonecap));

    char vpudecode[160] = "";
    wubu_vpudecode_summary(vpudecode, sizeof(vpudecode));

    char btamesh[160] = "";
    wubu_btamesh_summary(btamesh, sizeof(btamesh));

    char zonseqwrite[160] = "";
    wubu_zonseqwrite_summary(zonseqwrite, sizeof(zonseqwrite));

    char vpuencode[160] = "";
    wubu_vpuencode_summary(vpuencode, sizeof(vpuencode));

    char leaudio[160] = "";
    wubu_leaudio_summary(leaudio, sizeof(leaudio));

    char nvmehotplug[160] = "";
    wubu_nvmehotplug_summary(nvmehotplug, sizeof(nvmehotplug));

    char gpudc[160] = "";
    wubu_gpudc_summary(gpudc, sizeof(gpudc));

    char btbeacon[160] = "";
    wubu_btbeacon_summary(btbeacon, sizeof(btbeacon));

    char gamepaddz[160] = "";
    wubu_gamepaddz_summary(gamepaddz, sizeof(gamepaddz));

    char gpukms[160] = "";
    wubu_gpukms_summary(gpukms, sizeof(gpukms));

    char gamepadbm[160] = "";
    wubu_gamepadbm_summary(gamepadbm, sizeof(gamepadbm));

    char leaudioldr[160] = "";
    wubu_leaudioldr_summary(leaudioldr, sizeof(leaudioldr));

    char rendernode[160] = "";
    wubu_rendernode_summary(rendernode, sizeof(rendernode));

    char auracast[160] = "";
    wubu_auracast_summary(auracast, sizeof(auracast));

    char nvme_gen5[160] = "";
    wubu_nvme_gen5_summary(nvme_gen5, sizeof(nvme_gen5));

    char intelgpu[160] = "";
    wubu_intelgpu_summary(intelgpu, sizeof(intelgpu));

    char bap[160] = "";
    wubu_bap_summary(bap, sizeof(bap));

    char nvme_gen4[160] = "";
    wubu_nvme_gen4_summary(nvme_gen4, sizeof(nvme_gen4));

    char radeon_legacy[160] = "";
    wubu_radeon_legacy_summary(radeon_legacy, sizeof(radeon_legacy));

    char radeon_6000[160] = "";
    wubu_radeon_6000_summary(radeon_6000, sizeof(radeon_6000));

    char radeon_5000[160] = "";
    wubu_radeon_5000_summary(radeon_5000, sizeof(radeon_5000));

    char intel_gma[160] = "";
    wubu_intel_gma_summary(intel_gma, sizeof(intel_gma));

    char adreno700[160] = "";
    wubu_adreno700_summary(adreno700, sizeof(adreno700));

    char mali_g52[160] = "";
    wubu_mali_g52_summary(mali_g52, sizeof(mali_g52));

    char mali_g720[160] = "";
    wubu_mali_g720_summary(mali_g720, sizeof(mali_g720));

    char adreno600[160] = "";
    wubu_adreno600_summary(adreno600, sizeof(adreno600));

    char mali_g77[160] = "";
    wubu_mali_g77_summary(mali_g77, sizeof(mali_g77));

    char vc4[160] = "";
    wubu_vc4_summary(vc4, sizeof(vc4));

    char vc6[160] = "";
    wubu_vc6_summary(vc6, sizeof(vc6));

    char powervr[160] = "";
    wubu_powervr_summary(powervr, sizeof(powervr));

    char xe3[160] = "";
    wubu_xe3_summary(xe3, sizeof(xe3));

    char nvidia_fermi[160] = "";
    wubu_nvidia_fermi_summary(nvidia_fermi, sizeof(nvidia_fermi));

    char nvidia_kepler[160] = "";
    wubu_nvidia_kepler_summary(nvidia_kepler, sizeof(nvidia_kepler));

    char nvidia_maxwell[160] = "";
    wubu_nvidia_maxwell_summary(nvidia_maxwell, sizeof(nvidia_maxwell));

    char nvidia_pascal[160] = "";
    wubu_nvidia_pascal_summary(nvidia_pascal, sizeof(nvidia_pascal));

    char nvidia_volta[160] = "";
    wubu_nvidia_volta_summary(nvidia_volta, sizeof(nvidia_volta));

    char nvidia_turing[160] = "";
    wubu_nvidia_turing_summary(nvidia_turing, sizeof(nvidia_turing));

    char navi10[160] = "";
    wubu_navi10_summary(navi10, sizeof(navi10));

    char skylake[160] = "";
    wubu_intel_skylake_summary(skylake, sizeof(skylake));

    char icelake[160] = "";
    wubu_intel_icelake_summary(icelake, sizeof(icelake));

    char volcanic_islands[160] = "";
    wubu_volcanic_islands_summary(volcanic_islands, sizeof(volcanic_islands));

    char arctic_islands[160] = "";
    wubu_arctic_islands_summary(arctic_islands, sizeof(arctic_islands));

    char vega[160] = "";
    wubu_vega_summary(vega, sizeof(vega));

    char renoir[160] = "";
    wubu_renoir_summary(renoir, sizeof(renoir));

    char ampere[160] = "";
    wubu_ampere_summary(ampere, sizeof(ampere));

    char quadro[160] = "";
    wubu_quadro_summary(quadro, sizeof(quadro));

    char gt2xx[160] = "";
    wubu_gt2xx_summary(gt2xx, sizeof(gt2xx));

    char opencl[160] = "";
    wubu_opencl_summary(opencl, sizeof(opencl));

    char cuda[160] = "";
    wubu_cuda_summary(cuda, sizeof(cuda));

    char instinct[160] = "";
    wubu_instinct_summary(instinct, sizeof(instinct));

    char vulkan14[160] = "";
    wubu_vulkan14_summary(vulkan14, sizeof(vulkan14));

    char vram[160] = "";
    wubu_vram_summary(vram, sizeof(vram));

    char spdif[160] = "";
    wubu_spdif_summary(spdif, sizeof(spdif));

    char cmb[160] = "";
    wubu_cmb_summary(cmb, sizeof(cmb));

    char backlightpwm[160] = "";
    wubu_backlightpwm_summary(backlightpwm, sizeof(backlightpwm));

    char aec[160] = "";
    wubu_aec_summary(aec, sizeof(aec));

    char dedup[160] = "";
    wubu_dedup_summary(dedup, sizeof(dedup));

    char gpuband[160] = "";
    wubu_gpuband_summary(gpuband, sizeof(gpuband));

    char compress[160] = "";
    wubu_compress_summary(compress, sizeof(compress));

    char filter[160] = "";
    wubu_filter_summary(filter, sizeof(filter));

    char ducking[160] = "";
    wubu_ducking_summary(ducking, sizeof(ducking));

    char pdpolicy[160] = "";
    wubu_pdpolicy_summary(pdpolicy, sizeof(pdpolicy));

    char gpurst[160] = "";
    wubu_gpurst_summary(gpurst, sizeof(gpurst));

    char iosched[160] = "";
    wubu_iosched_summary(iosched, sizeof(iosched));

    char wifiutil[160] = "";
    wubu_wifiutil_summary(wifiutil, sizeof(wifiutil));

    char ddcci[160] = "";
    wubu_ddcci_summary(ddcci, sizeof(ddcci));

    char samplerate[160] = "";
    wubu_samplerate_summary(samplerate, sizeof(samplerate));

    char smart[160] = "";
    wubu_smart_summary(smart, sizeof(smart));

    char overclock[160] = "";
    wubu_overclock_summary(overclock, sizeof(overclock));

    char dspgraph[160] = "";
    wubu_dspgraph_summary(dspgraph, sizeof(dspgraph));

    char smr[160] = "";
    wubu_smr_summary(smr, sizeof(smr));

    char computectx[160] = "";
    wubu_computectx_summary(computectx, sizeof(computectx));

    char chanmap[160] = "";
    wubu_chanmap_summary(chanmap, sizeof(chanmap));

    char dmcrypt[160] = "";
    wubu_dmcrypt_summary(dmcrypt, sizeof(dmcrypt));


    char spdiftx[160] = "";
    wubu_spdiftx_summary(spdiftx, sizeof(spdiftx));

    char blkqos[160] = "";
    wubu_blkqos_summary(blkqos, sizeof(blkqos));

    char memmgr[160] = "";
    wubu_memmgr_summary(memmgr, sizeof(memmgr));

    char jackdetect[160] = "";
    wubu_jackdetect_summary(jackdetect, sizeof(jackdetect));

    char ioprio[160] = "";
    wubu_ioprio_summary(ioprio, sizeof(ioprio));

    char perfmon[160] = "";
    wubu_perfmon_summary(perfmon, sizeof(perfmon));

    char pcmplugin[160] = "";
    wubu_pcmplugin_summary(pcmplugin, sizeof(pcmplugin));

    char dax[160] = "";
    wubu_dax_summary(dax, sizeof(dax));

    char fbcon[160] = "";
    wubu_fbcon_summary(fbcon, sizeof(fbcon));

    char jackstate[160] = "";
    wubu_jackstate_summary(jackstate, sizeof(jackstate));

    char storagesched[160] = "";
    wubu_storagesched_summary(storagesched, sizeof(storagesched));

    char fencesync[160] = "";
    wubu_fencesync_summary(fencesync, sizeof(fencesync));

    char jackimpedance[160] = "";
    wubu_jackimpedance_summary(jackimpedance, sizeof(jackimpedance));

    char writeback[160] = "";
    wubu_writeback_summary(writeback, sizeof(writeback));

    char thermalthrottle[160] = "";
    wubu_thermalthrottle_summary(thermalthrottle, sizeof(thermalthrottle));

    char compressor[160] = "";
    wubu_compressor_summary(compressor, sizeof(compressor));

    char raid5[160] = "";
    wubu_raid5_summary(raid5, sizeof(raid5));

    char smc[160] = "";
    wubu_smc_summary(smc, sizeof(smc));

    char ieccontrol[160] = "";
    wubu_ieccontrol_summary(ieccontrol, sizeof(ieccontrol));

    char flush2[160] = "";
    wubu_flush2_summary(flush2, sizeof(flush2));

    char mmu[160] = "";
    wubu_mmu_summary(mmu, sizeof(mmu));

    char dappath[160] = "";
    wubu_dappath_summary(dappath, sizeof(dappath));

    char bio[160] = "";
    wubu_bio_summary(bio, sizeof(bio));

    char encode[160] = "";
    wubu_encode_summary(encode, sizeof(encode));

    char spdifstatus[160] = "";
    wubu_spdifstatus_summary(spdifstatus, sizeof(spdifstatus));

    char devmapper[160] = "";
    wubu_devmapper_summary(devmapper, sizeof(devmapper));

    char decode[160] = "";
    wubu_decode_summary(decode, sizeof(decode));

    char audiofw[160] = "";
    wubu_audiofw_summary(audiofw, sizeof(audiofw));

    char nfsmount[160] = "";
    wubu_nfsmount_summary(nfsmount, sizeof(nfsmount));

    char drm[160] = "";
    wubu_drm_summary(drm, sizeof(drm));

    char mixergraph[160] = "";
    wubu_mixergraph_summary(mixergraph, sizeof(mixergraph));

    char nfsclient[160] = "";
    wubu_nfsclient_summary(nfsclient, sizeof(nfsclient));

    char vblank[160] = "";
    wubu_vblank_summary(vblank, sizeof(vblank));

    char spdifrx[160] = "";
    wubu_spdifrx_summary(spdifrx, sizeof(spdifrx));

    char fusefs[160] = "";
    wubu_fusefs_summary(fusefs, sizeof(fusefs));

    char jack[160] = "";
    wubu_jack_summary(jack, sizeof(jack));

    char uas[160] = "";
    wubu_uas_summary(uas, sizeof(uas));

    char powergate[160] = "";
    wubu_powergate_summary(powergate, sizeof(powergate));

    char dapmwidget[160] = "";
    wubu_dapmwidget_summary(dapmwidget, sizeof(dapmwidget));

    char uac[160] = "";
    wubu_uac_summary(uac, sizeof(uac));

    char pcipme[160] = "";
    wubu_pcipme_summary(pcipme, sizeof(pcipme));

    char fence[160] = "";
    wubu_fence_summary(fence, sizeof(fence));

    char drvpower[128] = "";
    snprintf(drvpower, sizeof(drvpower),
        "drvpower[drivers=%d devices=%d]",
        wubu_drv_driver_count(), wubu_drv_device_count());

    snprintf(g_matrix, sizeof(g_matrix),
        "WuBuOS machine matrix\n"
        "--------------------\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "drv[%s]\n",

        hw, audio, storage, net, input, display, usb, power, peri, virt,
        sensor, can, mem, accel, camera, bt, codec, raid, fp, fpga, wifi7,
        pmicaudio, sw, sec, panel, phy, bus, rtc, video, nicoff, pm, usb4,
        compute, vlanaudio, sata, drmx, ptp, tpm, touch, psr, dspmode, mgig,
        gamepad, rdma, zoned, vrr, qos, hid, backlight, mixgraph, raidcache,
        pd, calib, eq, gadget, ucode, ptpsync, hdr, wifireg, trim, mst,
        thermal, ns, fc, gpusensor, fw, ima, colormgmt, loudness, gpusched,
        porttiming, codecgraph, flush, perf, pcmring, bcache, vram, spdif, cmb, backlightpwm, aec, dedup,
        gpuband, compress, filter, ducking, pdpolicy, gpurst, iosched, wifiutil,
        ddcci, samplerate, smart, overclock, dspgraph, smr, computectx,
        chanmap, dmcrypt, powergate, dapmwidget, znszone, drvpower,
        uac, pcipme, fence, spdiftx, blkqos, memmgr,
        jackdetect, ioprio, perfmon, pcmplugin, dax, fbcon,
        jackstate, storagesched, fencesync, jackimpedance, writeback,
        thermalthrottle, compressor, raid5, smc, ieccontrol, flush2,
        mmu, dappath, bio, encode, spdifstatus, devmapper, decode, audiofw, nfsmount,
        drm, mixergraph, nfsclient, vblank, spdifrx, fusefs, jack, uas, fantml, pcmlink, lvm, voltagectl, dapm, mdraid, gpucsched, dsptrace, nvmepower, gpufwupd, bthfp, zoneappend, gpufw, btaudio, gpushader, bta2dp, zonefmt, gpumem, btclassic, zonecap, vpudecode, btamesh, zonseqwrite, vpuencode, leaudio, nvmehotplug, gpudc, btbeacon, gamepaddz, gpukms, gamepadbm, leaudioldr, rendernode, auracast, nvme_gen5, intelgpu, bap, nvme_gen4, radeon_legacy, radeon_6000, radeon_5000, intel_gma, adreno700, mali_g52, mali_g720, adreno600, mali_g77, vc4, vc6, powervr, xe3, nvidia_fermi, nvidia_kepler, nvidia_maxwell, nvidia_pascal, nvidia_volta, nvidia_turing,
 icelake,
  navi10,
  skylake,
  arctic_islands,
  vega,
  volcanic_islands,
  ampere,
  quadro,
  renoir,
  cuda,
  gt2xx,
  opencl,
  instinct,
  vulkan14,
  wubu_probe_drv_matrix());
 }

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
        size_t len = strlen(g_matrix);
        size_t cap = g_wubu_kv_capacity;
        if (cap > 1024) cap = 1024;   /* matrix vector, bounded */
        if (len > cap) len = cap;
        for (size_t i = 0; i < len; i++)
            g_wubu_kv_base[i] = (float)(uint8_t)g_matrix[i];
    }
}

/* ---- W5: accessors ---- */
const char *wubu_probe_matrix(void) { return g_matrix; }
