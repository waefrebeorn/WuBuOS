/*
 * wubu_hw_detect.c -- the HARDWARE BUS DETECTOR (the OS's magic sense of self).
 *
 * "It should work on bare metal or not bare metal because we are a magical
 *  operating system."
 *
 * This module answers ONE question at boot with zero user interaction:
 *
 *    "Am I running on bare metal or inside a VM/WSL?"
 *
 * It probes, in order of reliability:
 *   1. /dev/dgx          — WSL2 GPU paravirtualized device (definitive WSL)
 *   2. /proc/sys/kernel/osrelease — "microsoft" magic string
 *   3. CPUID hypervisor bit (Intel/AMD) — VMware/Hyper-V/QEMU/KVM
 *   4. DMI /sys/class/dmi/id/product_name — "Virtual" prefixes
 *   5. PCI scan for VMware/VirtualBox/QEMU devices
 *
 * On bare metal, the PCI bus has the REAL GPU in device 00:01.0 or 00:02.0.
 * On WSL2, /dev/dgx is the only GPU path; bare PCI scan finds nothing.
 *
 * The detector publishes its verdict to the KV-FS at:
 *   /kv/world/hw_platform  -> "bare_metal" | "wsl2" | "kvm" | "vmware" | "qemu"
 *   /kv/world/hw_gpu       -> the GPU device path (/dev/nvidia0 or /dev/dgx)
 *
 * C11. Hosted sections (fopen/access) guarded by _GNU_SOURCE; the bare-metal
 * kernel uses the CPUID + PCI path only.
 */
#include "wubu_hw_detect.h"
#include "wubu_pci.h"
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
#include "wubu_backlightpwm.h"
#include "wubu_aec.h"
#include "wubu_fantml.h"
#include "wubu_pcmlink.h"
#include "wubu_lvm.h"
#include "wubu_mdraid.h"
#include "wubu_gpucsched.h"
#include "wubu_dsptrace.h"
#include "wubu_nvmepower.h"
#include "wubu_gpufw.h"
#include "wubu_btaudio.h"
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
#include "wubu_voltagectl.h"
#include "wubu_dapm.h"
#include "wubu_dedup.h"
#include "wubu_gpuband.h"
#include "wubu_backlightpwm.h"
#include "wubu_compress.h"
#include "wubu_filter.h"
#include "wubu_kvfs.h"

#include <string.h>
#include <stdint.h>

#ifdef _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#endif

/* the verdict (populated by wubu_hw_detect()) */
static char g_platform[32] = "unknown";
static char g_gpu_path[64]  = "";
static int  g_is_wsl        = 0;
static int  g_gpu_present   = 0;
static int  g_gpu_vendor    = 0;   /* PCI vendor of the detected GPU */
static int  g_gpu_device    = 0;   /* PCI device id of the detected GPU */
static int  g_prime         = 0;   /* hybrid iGPU+dGPU present */

/* ---- W1: the detector ---- */
void wubu_hw_detect(void)
{
    /* start clean */
    g_platform[0] = '\0';
    g_gpu_path[0]  = '\0';
    g_is_wsl = 0;
    g_gpu_present = 0;
    g_gpu_vendor = 0;
    g_gpu_device = 0;

#ifdef _GNU_SOURCE
    /* 1. /proc/sys/kernel/osrelease — the WSL2 magic string */
    FILE *f = fopen("/proc/sys/kernel/osrelease", "r");
    if (f) {
        char buf[256] = "";
        if (fgets(buf, sizeof(buf), f)) {
            if (strstr(buf, "microsoft") || strstr(buf, "Microsoft")) {
                strcpy(g_platform, "wsl2");
                g_is_wsl = 1;
            }
        }
        fclose(f);
    }

    /* 2. /dev/dgx — WSL2 GPU paravirtualized device */
    if (access("/dev/dxg", R_OK) == 0) {
        strcpy(g_gpu_path, "/dev/dxg");
        g_gpu_present = 1;
        strcpy(g_platform, "wsl2");
        g_is_wsl = 1;
    }
#else
    /* Bare-metal kernel: no /proc, no stdio. The CPUID + PCI path below
     * is the only detection available. */
#endif

    /* 3. CPUID hypervisor bit — if no /dev/dgx, check for VMs
     * (works in both bare-metal and hosted builds — CPUID is a real
     * instruction on both Intel and AMD). */
    if (!g_is_wsl) {
        uint32_t eax, ebx, ecx, edx;
        __asm__ __volatile__(
            "cpuid"
            : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
            : "a"(1), "c"(0)
        );
        if (ecx & (1u << 31)) {
            uint32_t max;
            __asm__ __volatile__("cpuid" : "=a"(max) : "a"(0x40000000) : "ebx", "ecx", "edx");
            if (max >= 0x40000000) {
                char hv_id[13] = {0};
                __asm__ __volatile__(
                    "cpuid"
                    : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                    : "a"(0x40000000)
                );
                memcpy(hv_id + 0, &ebx, 4);
                memcpy(hv_id + 4, &ecx, 4);
                memcpy(hv_id + 8, &edx, 4);
                if (strstr(hv_id, "Microsoft")) {
                    strcpy(g_platform, "hyperv");
                    g_is_wsl = 1;
                } else if (strstr(hv_id, "VMware")) {
                    strcpy(g_platform, "vmware");
                } else if (strstr(hv_id, "KVM")) {
                    strcpy(g_platform, "kvm");
                } else if (strstr(hv_id, "QEMU")) {
                    strcpy(g_platform, "qemu");
                }
            }
        }
    }

    /* 4. Bare-metal GPU detection via PCI scan */
    if (!g_gpu_present) {
        wubu_pci_dev_t devs[WUBU_PCI_MAX_DEVS];
        int n = wubu_pci_scan(devs, WUBU_PCI_MAX_DEVS);
        int prime_count = 0;  /* distinct GPUs (for DRI_PRIME hybrid) */
        for (int i = 0; i < n; i++) {
            /* VGA-class device (class 0x03 = display controller) */
            int is_gpu = (devs[i].class_code == 0x03 ||
                          devs[i].vendor == 0x10DE ||   /* nvidia dGPU */
                          devs[i].vendor == 0x1002 ||   /* AMD */
                          devs[i].vendor == 0x8086);    /* Intel */
            if (!is_gpu) continue;

            /* NVIDIA: vendor 0x10DE */
            if (devs[i].vendor == 0x10DE) {
                if (!g_gpu_present) {
                    strcpy(g_gpu_path, "/dev/nvidia0");
                    g_gpu_present = 1;
                    g_gpu_vendor = 0x10DE;
                    g_gpu_device = devs[i].device;
                    if (g_platform[0] == '\0') strcpy(g_platform, "bare_metal");
                } else {
                    prime_count++;
                }
            }
            /* AMD iGPU (0x1002): Van Gogh, Rembrandt, Cezanne, Phoenix,
             * Strix Point + RDNA2/3 APUs + GCN1/2. amdgpu KMD -> /dev/dri/card0. */
            else if (devs[i].vendor == 0x1002) {
                if (!g_gpu_present) {
                    strcpy(g_gpu_path, "/dev/dri/card0");
                    g_gpu_present = 1;
                    g_gpu_vendor = 0x1002;
                    g_gpu_device = devs[i].device;
                    if (g_platform[0] == '\0') strcpy(g_platform, "bare_metal");
                } else {
                    prime_count++;
                }
            }
            /* Intel iGPU (0x8086): Tiger Lake/Alder Lake Xe, Arc,
             * Meteor/Lunar Lake, Broadwell+. i915/xe KMD -> /dev/dri/card0. */
            else if (devs[i].vendor == 0x8086) {
                if (!g_gpu_present) {
                    strcpy(g_gpu_path, "/dev/dri/card0");
                    g_gpu_present = 1;
                    g_gpu_vendor = 0x8086;
                    g_gpu_device = devs[i].device;
                    if (g_platform[0] == '\0') strcpy(g_platform, "bare_metal");
                } else {
                    prime_count++;
                }
            }
        }
        g_prime = (prime_count > 0);   /* 2+ GPUs = hybrid laptop */
    }

    /* 5. If still unknown, call it bare metal */
    if (g_platform[0] == '\0') strcpy(g_platform, "bare_metal");

    /* 6. Probe audio controllers (PCI class 0x0403 + USB). */
    wubu_audio_probe();

    /* 7. Probe storage topology (NVMe/SATA/IDE + Intel RST lockout). */
    wubu_storage_probe();

    /* 8. Probe network topology (Wi-Fi chip + ethernet + 2.5GbE). */
    wubu_net_probe();

    /* 9. Probe display topology (DRM/KMS driver per GPU generation). */
    wubu_display_probe();

    /* 10. Probe USB topology (host controllers + connector + gadget). */
    wubu_usbf_probe();

    /* 11. Probe power/CPU topology (cpufreq/C-state/battery/thermal). */
    wubu_power_probe();

    /* 12. Probe peripheral topology (serial/parallel/GPIO/hwmon). */
    wubu_peripheral_probe();

    /* 13. Probe virtualization (hypervisor + PV driver set). */
    wubu_virt_probe();

    /* 14. Probe IIO sensors (accel/gyro/IMU/baro/ALS). */
    wubu_sensor_probe();

    /* 15. Probe CAN bus (SocketCAN automotive/industrial). */
    wubu_can_probe();

    /* 16. Probe memory/ECC (EDAC + SPD health). */
    wubu_mem_probe();

    /* 17. Probe NPU/accelerator (AI compute). */
    wubu_accel_probe();

    /* 18. Probe V4L2 camera/ISP. */
    wubu_camera_probe();

    /* 19. Probe Bluetooth (controller + LE Audio). */
    wubu_bt_probe();

    /* 20. Probe audio codec/DSP (HD-Audio + ASoC + SOF). */
    wubu_codec_probe();

    /* 21. Probe RAID/SAS storage. */
    wubu_raid_probe();

    /* 22. Probe fingerprint/biometric. */
    wubu_fingerprint_probe();

    /* 23. Probe FPGA (manager/region/bridge). */
    wubu_fpga_probe();

    /* 24. Probe Wi-Fi 7 / 6GHz. */
    wubu_wifi7_probe();

    /* 25. Probe PMIC + audio amp/DAC. */
    wubu_pmicaudio_probe();

    /* 26. Probe network switch fabric (switchdev/DSA). */
    wubu_switchdev_probe();

    /* 27. Probe security key (FIDO2/CCID/TPM). */
    wubu_securekey_probe();

    /* 28. Probe DRM panel + watchdog + fuel gauge. */
    wubu_panel_probe();

    /* 29. Probe Ethernet PHY/MDIO. */
    wubu_phy_probe();

    /* 30. Probe I2C/SPI bus controllers. */
    wubu_bus_probe();

    /* 31. Probe RTC + thermal sensors. */
    wubu_clock_probe();

    /* 32. Probe video codec/VA-API. */
    wubu_video_probe();

    /* 33. Probe NIC offload + multi-queue. */
    wubu_nicoffload_probe();

    /* 34. Probe power modes (S0ix/sleep/runtime PM). */
    wubu_pm_probe();

    /* 35. Probe USB4/Thunderbolt. */
    wubu_usb4_probe();

    /* 36. Probe graphics compute (OpenCL/Vulkan). */
    wubu_compute_probe();

    /* 37. Probe NIC VLAN + audio DSP. */
    wubu_vlanaudio_probe();

    /* 38. Probe advanced SATA/NCQ. */
    wubu_sata_probe();

    /* 39. Probe DRM writeback/HDR/color. */
    wubu_drmx_probe();

    /* 40. Probe Ethernet PTP/TSN + haptics. */
    wubu_ptp_probe();

    /* 41. Probe TPM 2.0 full stack. */
    wubu_tpm_probe();

    /* 42. Probe touch/trackpad. */
    wubu_touch_probe();

    /* 43. Probe display PSR + NIC SR-IOV. */
    wubu_psr_probe();

    /* 44. Probe audio codec DSP modes. */
    wubu_dspmode_probe();

    /* 45. Probe Ethernet multi-gig (2.5/5/10G) PHY. */
    wubu_multigig_probe();

    /* 46. Probe game controllers + display DSC. */
    wubu_gamepad_probe();

    /* 47. Probe NIC RDMA/InfiniBand. */
    wubu_rdma_probe();

    /* 48. Probe SMR/Zoned storage. */
    wubu_zoned_probe();

    /* 49. Probe display VRR + spatial audio. */
    wubu_vrr_probe();

    /* 50. Probe Ethernet switch QoS/ACL. */
    wubu_qos_probe();

    /* 51. Probe USB HID advanced. */
    wubu_hidadv_probe();

    /* 52. Probe display backlight + NIC WoL. */
    wubu_backlight_probe();

    /* 53. Probe audio mixing graph. */
    wubu_mixgraph_probe();

    /* 54. Probe storage RAID cache. */
    wubu_raidcache_probe();

    /* 55. Probe USB PD + NIC flow steering. */
    wubu_pd_probe();

    /* 56. Probe display brightness/gamma calibration. */
    wubu_calib_probe();

    /* 57. Probe audio equalizer DSP coefficients. */
    wubu_eq_probe();

    /* 58. Probe USB gadget + NVMe endurance. */
    wubu_gadget_probe();

    /* 59. Probe CPU microcode loading. */
    wubu_ucode_probe();

    /* 60. Probe NIC PTP time sync. */
    wubu_ptp_sync_probe();

    /* 61. Probe display HDR + audio jack detection. */
    wubu_hdr_probe();

    /* 62. Probe WiFi regulatory/DFS. */
    wubu_wifi_reg_probe();

    /* 63. Probe storage TRIM + USB-C alt mode. */
    wubu_trim_probe();

    /* 64. Probe DisplayPort MST + audio SRC. */
    wubu_mst_probe();

    /* 65. Probe fan/thermal control. */
    wubu_thermal_probe();

    /* 66. Probe NVMe namespace/multipath. */
    wubu_ns_probe();

    /* 67. Probe ethernet flow control. */
    wubu_fc_probe();

    /* 68. Probe GPU sensor + fan curve. */
    wubu_gpusensor_probe();

    /* 69. Probe storage controller firmware. */
    wubu_fw_probe();

    /* 70. Probe IMA/EVM measured boot. */
    wubu_ima_probe();

    /* 71. Probe display color management. */
    wubu_colormgmt_probe();

    /* 72. Probe audio loudness normalization. */
    wubu_loudness_probe();

    /* 73. Probe GPU compute scheduler. */
    wubu_gpusched_probe();

    /* 74. Probe display port timing. */
    wubu_porttiming_probe();

    /* 75. Probe audio codec graph. */
    wubu_codecgraph_probe();

    /* 76. Probe storage cache flush/barrier. */
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

    /* 77. Probe display backlight PWM. */
    wubu_backlightpwm_probe();

    /* 78. Probe audio AEC + noise suppression. */
    wubu_aec_probe();

    /* 79. Probe storage deduplication. */
    wubu_dedup_probe();

    /* 80. Probe GPU scheduler priority bands. */
    wubu_gpuband_probe();

    /* 81. Probe storage transparent compression. */
    wubu_compress_probe();

    /* 82. Probe audio DSP filter + EQ. */
    wubu_filter_probe();

    /* publish verdict to KV-FS */
    if (g_wubu_kvfs && g_wubu_kv_base && g_wubu_kv_capacity > 8) {
        float vec[8] = {0};
        for (int i = 0; i < 8 && g_platform[i] && g_wubu_kv_capacity > (size_t)(i * 4 + 3); i++) {
            vec[i] = (float)(uint8_t)g_platform[i];
        }
        wubu_kvfs_write(g_wubu_kvfs, "/kv/world/hw_platform", g_wubu_kv_base, vec, 8);
    }
}

/* ---- W2: accessors (read-only to the rest of the kernel) ---- */
int  wubu_hw_is_wsl(void)          { return g_is_wsl; }
int  wubu_hw_gpu_present(void)      { return g_gpu_present; }
const char *wubu_hw_platform(void)  { return g_platform; }
const char *wubu_hw_gpu_path(void)  { return g_gpu_path[0] ? g_gpu_path : NULL; }
int  wubu_hw_gpu_vendor(void)       { return g_gpu_vendor; }
int  wubu_hw_gpu_device(void)       { return g_gpu_device; }

/* ---- W3c-W3f: driver-routing decisions from the device ID ---- */

/* GCN1/2 (Southern Islands / Sea Islands) default to the radeon KMD, which
 * has NO Vulkan. The kernel must force amdgpu via module params. */
const char *wubu_hw_amdgpu_params(void)
{
    if (g_gpu_vendor != 0x1002 || g_is_wsl) return NULL;
    switch (g_gpu_device) {
    /* Southern Islands (GCN1) */
    case 0x6798: case 0x6810: case 0x6821: case 0x6600:
    case 0x6780:
        return "amdgpu.si_support=1";
    /* Sea Islands (GCN2) */
    case 0x6640: case 0x6658: case 0x130F: case 0x1313:
        return "amdgpu.cik_support=1";
    default:
        return NULL;   /* RDNA needs no params */
    }
}

/* RDNA4 (Navi44/48) prefers AMDVLK over RADV for full Vulkan 1.4. */
int wubu_hw_needs_amdvlk(void)
{
    if (g_gpu_vendor != 0x1002 || g_is_wsl) return 0;
    return (g_gpu_device == 0x74C4 || g_gpu_device == 0x74C2);
}

/* Xe2 Intel (Lunar Lake Arc 140V, Battlemage, Celestial) prefer the xe KMD. */
int wubu_hw_intel_uses_xe(void)
{
    if (g_gpu_vendor != 0x8086 || g_is_wsl) return 0;
    return (g_gpu_device == 0x7D5A || g_gpu_device == 0xE20B);
}

/* Hybrid iGPU+dGPU laptop -> expose DRI_PRIME. */
int wubu_hw_has_prime(void)
{
    if (g_is_wsl) return 0;
    return g_prime;
}

/* ---- W3: boot summary (the console's `hw` command output) ---- */
#ifdef _GNU_SOURCE
/* forward-declare helper (defined below with the W4/W5/W6/W7 section) */
static int wubu_file_exists(const char *path);
#endif

int wubu_hw_summary(char *out, size_t cap)
{
    if (!out || cap == 0) return -1;
    int dxg = 0;
#ifdef _GNU_SOURCE
    dxg = (access("/dev/dxg", R_OK) == 0);
#endif
#ifdef _GNU_SOURCE
    int n = snprintf(out, cap,
        "hw[platform=%s gpu=%s dxg=%d wsl=%d vendor=%04x/%04x vulkan=%s dzn=%d nvidia_icd=%d radv=%d anv=%d amdgpu=%s prime=%d xe=%d amdvlk=%d audio=%s snd=%s hdmi=%d bt=%d nvme=%d sata=%d rst=%d wifi=%d eth=%d eth2g5=%d input=%d mouse=%dHz gpu_driver=%s chip=%s usb=%s cpu=%d cores=%d hyper=%s]",
        g_platform,
        g_gpu_path[0] ? g_gpu_path : "none",
        dxg,
        g_is_wsl,
        g_gpu_vendor, g_gpu_device,
        wubu_hw_vulkan_icd() ? wubu_hw_vulkan_icd() : "llvmpipe_fallback",
        wubu_hw_has_dzn(),
        wubu_hw_has_nvidia_icd(),
        wubu_file_exists("/usr/share/vulkan/icd.d/radeon_icd.json"),
        wubu_file_exists("/usr/share/vulkan/icd.d/intel_icd.json"),
        wubu_hw_amdgpu_params() ? wubu_hw_amdgpu_params() : "none",
        wubu_hw_has_prime(),
        wubu_hw_intel_uses_xe(),
        wubu_hw_needs_amdvlk(),
        wubu_audio_present() ? "snd_hda" : "no",
        wubu_audio_driver() ? wubu_audio_driver() : "none",
        wubu_audio_is_hdmi(),
        wubu_audio_has_bt(),
        wubu_storage_has_nvme(), wubu_storage_has_sata(),
        wubu_storage_has_raid_rst(),
        wubu_net_has_wifi(), wubu_net_has_eth(), wubu_net_has_2g5(),
        wubu_input_has_controller(), wubu_input_poll_hz(),
        wubu_display_driver() ? wubu_display_driver() : "none",
        wubu_display_chip_name() ? wubu_display_chip_name() : "none",
        wubu_usbf_hcd() ? wubu_usbf_hcd() : "none",
        wubu_power_cpu_vendor(), wubu_power_ncores(),
        wubu_virt_hypervisor_name() ? wubu_virt_hypervisor_name() : "bare");
    return n < 0 ? -1 : 0;
#else
    /* bare-metal kernel libc stub */
    (void)out; (void)cap; (void)dxg;
    return -1;
#endif
}

/* ---- W4/W5/W6/W7: Vulkan ICD selection ----
 *
 * On WSL2, the real GPU is only reachable through the Dozen (dzn) driver:
 *   Vulkan -> dzn ICD -> D3D12 -> /dev/dxg -> GPU
 * Without dzn, only llvmpipe (software Vulkan) is available.
 *
 * On bare metal NVIDIA, the nvidia_icd.json provides direct Vulkan access.
 *
 * The ICD chain always appends lvp_icd.json (llvmpipe) as the last fallback
 * so the Vulkan loader always enumerates at least one device. */

#ifdef _GNU_SOURCE

/* Check if a file exists at the given path */
static int wubu_file_exists(const char *path)
{
    return access(path, R_OK) == 0;
}

/* Check if dzn (Dozen) driver JSON exists in the standard Vulkan ICD dir.
 * The dzn ICD is: /usr/share/vulkan/icd.d/microsoft_dzn_icd.x86_64.json */
int wubu_hw_has_dzn(void)
{
    /* Common dzn ICD filenames across distributions */
    const char *candidates[] = {
        "/usr/share/vulkan/icd.d/microsoft_dzn_icd.x86_64.json",
        "/usr/share/vulkan/icd.d/microsoft_dzn_icd.json",
        "/usr/share/vulkan/icd.d/dzn_icd.x86_64.json",
        "/usr/share/vulkan/icd.d/dzn_icd.json",
        "/usr/share/vulkan/icd.d/99-bifrost-Dzn.json",
        NULL
    };
    for (int i = 0; candidates[i]; i++) {
        if (wubu_file_exists(candidates[i])) return 1;
    }
    /* Check if the Vulkan loader has a "microsoft" entry in its ICD search */
    /* Also check the lib path directly */
    if (wubu_file_exists("/usr/lib/x86_64-linux-gnu/libvulkan_dzn.so") ||
        wubu_file_exists("/usr/lib/x86_64-linux-gnu/libvulkan_dzn.so.1")) {
        return 1;
    }
    return 0;
}

/* Check if NVIDIA Vulkan ICD JSON exists (bare metal). */
int wubu_hw_has_nvidia_icd(void)
{
    const char *candidates[] = {
        "/usr/share/vulkan/icd.d/nvidia_icd.json",
        "/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json",
        "/etc/vulkan/icd.d/nvidia_icd.json",
        NULL
    };
    for (int i = 0; candidates[i]; i++) {
        if (wubu_file_exists(candidates[i])) return 1;
    }
    if (wubu_file_exists("/usr/lib/x86_64-linux-gnu/libvulkan_nvidia.so") ||
        wubu_file_exists("/usr/lib/x86_64-linux-gnu/libvulkan_nvidia.so.1")) {
        return 1;
    }
    return 0;
}

#else

/* Bare-metal kernel libc stubs -- no filesystem access.
 * The kernel build uses its own PCI-based detection instead. */
int wubu_hw_has_dzn(void)        { return 0; }
int wubu_hw_has_nvidia_icd(void) { return g_gpu_path[0] && strstr(g_gpu_path, "/dev/nvidia0"); }

#endif

/* W4: return the preferred Vulkan ICD JSON for the detected platform.
 * WSL2 + dzn: returns the dzn ICD path.
 * Bare metal NVIDIA: returns nvidia ICD path.
 * Fallback: returns NULL (caller should use llvmpipe/lvp_icd.json). */
const char *wubu_hw_vulkan_icd(void)
{
#ifdef _GNU_SOURCE
    static char icd_path[256] = "";
    if (icd_path[0]) return icd_path;  /* cached */

    if (g_is_wsl) {
        /* WSL2: prefer dzn (Vulkan -> D3D12 -> /dev/dxg -> GPU) */
        const char *candidates[] = {
            "/usr/share/vulkan/icd.d/microsoft_dzn_icd.x86_64.json",
            "/usr/share/vulkan/icd.d/microsoft_dzn_icd.json",
            "/usr/share/vulkan/icd.d/dzn_icd.x86_64.json",
            "/usr/share/vulkan/icd.d/dzn_icd.json",
            NULL
        };
        for (int i = 0; candidates[i]; i++) {
            if (wubu_file_exists(candidates[i])) {
                strcpy(icd_path, candidates[i]);
                return icd_path;
            }
        }
        /* No dzn — fall through to llvmpipe (returned as NULL) */
    } else {
        /* Bare metal: check for GPU vendor ICD. The PCI scan in
         * wubu_hw_detect() recorded g_gpu_vendor. RADV handles AMD,
         * ANV handles Intel, and the NVIDIA loader handles NVIDIA.
         * RDNA4 (Navi44/48) prefers AMDVLK over RADV for full Vulkan 1.4. */
        const char *candidates[][4] = {
            { "/usr/share/vulkan/icd.d/nvidia_icd.json",
              "/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json", NULL },  /* 0x10DE */
            { NULL, NULL, NULL, NULL },  /* 0x1002 AMD: filled below */
            { "/usr/share/vulkan/icd.d/intel_icd.json",
              "/usr/share/vulkan/icd.d/intel_icd.x86_64.json", NULL },  /* 0x8086 */
        };
        int vendor_idx = -1;
        if (g_gpu_vendor == 0x10DE) vendor_idx = 0;
        else if (g_gpu_vendor == 0x1002) vendor_idx = 1;
        else if (g_gpu_vendor == 0x8086) vendor_idx = 2;

        /* AMD: prefer AMDVLK on RDNA4, else RADV. */
        if (vendor_idx == 1) {
            if (wubu_hw_needs_amdvlk()) {
                candidates[1][0] = "/usr/share/vulkan/icd.d/amdvlk_icd.x86_64.json";
                candidates[1][1] = "/usr/share/vulkan/icd.d/amdvlk_icd.json";
            } else {
                candidates[1][0] = "/usr/share/vulkan/icd.d/radeon_icd.json";
                candidates[1][1] = "/usr/share/vulkan/icd.d/radeon_icd.x86_64.json";
            }
        }

        if (vendor_idx >= 0) {
            for (int i = 0; candidates[vendor_idx][i]; i++) {
                if (wubu_file_exists(candidates[vendor_idx][i])) {
                    strcpy(icd_path, candidates[vendor_idx][i]);
                    return icd_path;
                }
            }
        }
        /* GCN1/2 (Vega/SI/CIK) needs amdgpu KMD, not radeon. The kernel
         * module param amdgpu.si_support=1 amdgpu.cik_support=1 must be
         * set at boot (wubu_hw_amdgpu_params()); see W3c. That's a kernel
         * cmdline fix, not a userspace ICD fix. */
    }
    return NULL;  /* no hardware ICD found → caller should use llvmpipe */
#else
    /* Bare-metal kernel: no filesystem */
    if (g_gpu_path[0] && strstr(g_gpu_path, "/dev/nvidia0")) {
        return "/usr/share/vulkan/icd.d/nvidia_icd.json";
    }
    return NULL;
#endif
}

/* W5: build the full ICD chain as a ':'-separated string.
 * Preferred ICD first, llvmpipe (lvp_icd.json) always last as fallback. */
char *wubu_hw_vulkan_icd_chain(void)
{
#ifdef _GNU_SOURCE
    char chain[512] = "";
    const char *primary = wubu_hw_vulkan_icd();
    if (primary) {
        snprintf(chain, sizeof(chain), "%s:", primary);
    }
    /* Always append llvmpipe (lvp_icd.json) as the fallback */
    if (wubu_file_exists("/usr/share/vulkan/icd.d/lvp_icd.json")) {
        strcat(chain, "/usr/share/vulkan/icd.d/lvp_icd.json");
    } else if (wubu_file_exists("/usr/share/vulkan/icd.d/lvp_icd.x86_64.json")) {
        strcat(chain, "/usr/share/vulkan/icd.d/lvp_icd.x86_64.json");
    }
    /* Note: gfxstream_vk_icd.json is NOT included -- it fails on WSL2 with
     * "Failed to detect any valid GPUs". The correct WSL2 path is dzn
     * (Vulkan->D3D12->/dev/dxg), which is already the primary above. */
    if (chain[0] == '\0') {
        /* No ICDs found at all — only llvmpipe if available */
        return strdup("");
    }
    return strdup(chain);
#else
    /* Kernel libc stub */
    const char *primary = wubu_hw_vulkan_icd();
    if (primary) {
        char *combined = malloc(512);
        if (combined) {
            snprintf(combined, 512, "%s:/usr/share/vulkan/icd.d/lvp_icd.json", primary);
        }
        return combined;
    }
    return strdup("");
#endif
}
