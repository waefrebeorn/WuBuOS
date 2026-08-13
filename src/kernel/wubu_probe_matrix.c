#include "wubu_probe_matrix.h"
#include "wubu_probe.h"
#include <string.h>
#include <stdio.h>

/* ---- the matrix buffer (definition; decl is in wubu_probe_matrix.h) ---- */
char g_matrix[WUBU_MATRIX_SIZE] = "";

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


/* ---- W5: accessor ---- */
const char *wubu_probe_matrix(void) { return g_matrix; }
