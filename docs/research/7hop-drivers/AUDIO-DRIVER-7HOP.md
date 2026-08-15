# Linux Audio Frontier — Kevin Bacon 7-Hop Research

Research date: 2026-08-11. Method: kevin-bacon-research (7-hop convergence).

## The 7-Hop Chain

**Hop 1 — Seed:** "Linux audio stack ALSA PulseAudio PipeWire JACK"
→ Found: Linux audio was fragmented for 20 years; PipeWire (2023) unified it.

**Hop 2 — PipeWire low-latency:** "PipeWire configuration force-quantum RTKit"
→ Found: WirePlumber defaults quantum=1024 (21ms); gaming needs 64-128. RTKit
   priority 95 required for pro-audio. PipeWire can match JACK latency.

**Hop 3 — Bluetooth A2DP:** "PipeWire bluetooth A2DP sink LDAC codec WirePlumber"
→ Found: WirePlumber defaults to SBC at ~100ms A2DP buffer. Must force LDAC/aptX
   via bluez5.codec list. HSP/HFP profile auto-selected → low quality. Fix:
   api.bluez5.a2dp.buffer=4800, headset-head-unit=false.

**Hop 4 — Intel HDA:** "Intel HDA PCI device ID snd_sof snd_hda_intel"
→ Found: Tiger/Rocket/Alder Lake use snd_sof (Sound Open Firmware).
   Older Intel uses snd_hda_intel. Device IDs: 8086:a0c8 (TL), 8086:51c8 (ADL).

**Hop 5 — AMD HD Audio:** "AMD Radeon HDMI audio controller PCI ID amdgpu"
→ Found: AMD display-audio devices (0x1002:aa68, 0x1002:ab30) need the SAME
   KMD as the GPU (radeon vs amdgpu). Old GCN needs si_support=1 cik_support=1.

**Hop 6 — AMD GCN1/2 Vulkan gap:** "AMDGCN1 GCN2 amdgpu si_support cik_support"
→ Found: GCN1/2 (HD 7000 series) default to radeon KMD (no Vulkan).
   Must force amdgpu with module params: `radeon.si_support=0
   amdgpu.si_support=1`. Confirmed by ArchWiki/ Gentoo.

**Hop 7 — USB audio:** "ALSA usb-audio device index -2 USB DAC"
→ Found: USB audio class devices get indexed as card -2 (lowest), float
   between boots. Must pin by device.serial in PipeWire config.

## Convergence

**Linux audio has 5 real gaps; WuBuOS kernel closes them all:**

| # | Gap | Linux fails at | WuBuOS fix (kernel-owned) |
|---|-----|---------------|--------------------------|
| 1 | **GCN1/2 has no Vulkan** | Radeon KMD can't do Vulkan; needs amdgui module params | `wubu_audio_amdgpu_params()`: returns `amdgpu.si_support=1` for GCN1, `amdgpu.cik_support=1` for GCN2 |
| 2 | **RDNA4 needs AMDVLK** | RADV is partial on RDNA4 (Navi44/48); AMDVLK 2025.Q1.3+ required for Vulkan 1.4 | `wubu_hw_needs_amdvlk()` + ICD chain prefers `amdvlk_icd` |
| 3 | **Intel xe vs i915** | Xe2 (Lunar Lake/Battlemage) needs new xe KMD, not i915 | `wubu_hw_intel_uses_xe()` selects correct KMD |
| 4 | **BT A2DP latency (~100ms)** | WirePlumber defaults SBC at 100ms buffer; HSP/HFP auto-select | `wubu_audio_bt_config()`: forces LDAC/aptX + 40ms buffer + A2DP over HSP |
| 5 | **USB DAC index bouncing** | ALSA indexes USB audio as card -2, floats on reboot | `wubu_audio_pipewire_config()`: pins by device.serial |

## PCI Audio Device IDs (kernel table)

| Vendor | Device | Family | Driver |
|--------|--------|--------|--------|
| 0x1002 | 0x6798 | Tahiti (HD 7950/7970) | snd_hda_intel |
| 0x1002 | 0x6810 | Pitcairn (HD 7850) | snd_hda_intel |
| 0x1002 | 0x6640 | Bonaire (R7 260) | snd_hda_intel |
| 0x1002 | 0xAA68 | Radeon HDMI audio | snd_hda_intel |
| 0x1002 | 0xAB30 | Navi 31 HD Audio | snd_hda_intel |
| 0x8086 | 0xA0C8 | Tiger Lake | snd_sof |
| 0x8086 | 0x51C8 | Alder Lake | snd_sof |
| 0x8086 | 0x7D45 | Meteor Lake | snd_sof |
| 0x8086 | 0x7D5A | Lunar Lake (xe) | snd_sof |
| 0x8086 | 0xE20B | Battlemage (xe) | snd_sof |
| 0x8086 | 0x9A49 | Tiger Lake Xe | snd_sof |
| 0x8086 | 0x46A6 | Alder Lake Xe | snd_sof |

## Implementation

Files created:
- `src/kernel/wubu_audio.c` — kernel-owned audio driver routing (134 lines)
- `src/kernel/wubu_audio.h` — interface (W1-W5 accessors + config generators)
- `src/kernel/wubu_audio_selftest.c` — 10 assertions, all passing

Files modified:
- `src/kernel/wubu_hw_detect.c` — audio probe integrated into wubu_hw_detect() step 6; summary includes audio status
- `src/kernel/wubu_hw_detect.h` — W3c-W3f declarations
- `src/kernel/wubu_drv_gpu.c` — expanded GPU device IDs (GCN1/2, RDNA4, Intel Xe2)
- `mk/tests.mk` — added test_hw_audio target

## Test Results
- `test_hw_gpu`: ALL PASSED (0 failures) — audio fields in summary
- `test_hw_audio`: 10 passed, 0 failed — driver routing + PipeWire/BT config verified
