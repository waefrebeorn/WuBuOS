/*
 * wubu_audio.c  --  WuBuOS Audio Engine (Facade)
 * Modular audio engine: chip emulations, furnace tracker, SF2 synth, DAW mixer.
 * This file is now a thin facade - real implementation in submodules.
 */

#include "wubu_audio.h"
#include "wubu_audio_internal.h"

/* This file intentionally left minimal - all implementation moved to:
 *   wubu_audio_chips.c      - Chip emulations (NES, GB, YM2612, SID, etc.)
 *   wubu_audio_furnace.c    - Furnace tracker pattern editor
 *   wubu_audio_sf2.c        - TinySoundFont SF2 synthesis
 *   wubu_audio_daw.c        - Ardour-style DAW mixer
 *   wubu_audio_engine.c     - Engine lifecycle, MIDI, AI plugins, ingestion
 *
 * Internal declarations in wubu_audio_internal.h
 */

const char *wubu_chip_name(WubuChipType chip) {
    switch (chip) {
        case CHIP_NONE:        return "None";
        case CHIP_NES_APU:     return "NES APU (2A03)";
        case CHIP_GB_APU:      return "Game Boy APU";
        case CHIP_YM2612:      return "YM2612 (Genesis FM)";
        case CHIP_SN76489:     return "SN76489 (Genesis PSG)";
        case CHIP_SID:         return "MOS6581/8580 SID";
        case CHIP_SAA1099:     return "Philips SAA1099";
        case CHIP_PCSPEAKER:   return "PC Speaker";
        case CHIP_VRC6:        return "Konami VRC6";
        case CHIP_N163:        return "Namco 163";
        case CHIP_OPLL:        return "Yamaha OPLL";
        case CHIP_OPL:         return "Yamaha OPL";
        case CHIP_OPL2:        return "Yamaha OPL2";
        case CHIP_OPL3:        return "Yamaha OPL3";
        case CHIP_SCC:         return "Konami SCC";
        case CHIP_AY8910:      return "AY-3-8910";
        case CHIP_TIA:         return "Atari TIA";
        case CHIP_POKEY:       return "Atari POKEY";
        case CHIP_C64:         return "C64 SID";
        default:               return "Unknown";
    }
}

const char *wubu_track_type_name(WubuTrackType type) {
    switch (type) {
        case TRACK_AUDIO:   return "Audio";
        case TRACK_MIDI:    return "MIDI";
        case TRACK_BUS:     return "Bus";
        case TRACK_MASTER:  return "Master";
        case TRACK_FURNACE: return "Furnace";
        case TRACK_SF2:     return "SF2";
        case TRACK_AI:      return "AI Plugin";
        default:            return "Unknown";
    }
}

const char *wubu_ai_plugin_type_name(WubuAIPluginType type) {
    switch (type) {
        case AI_PLUGIN_SEPARATION: return "Source Separation";
        case AI_PLUGIN_MASTERING:  return "Auto-Mastering";
        case AI_PLUGIN_TRANSCRIBE: return "Transcription";
        case AI_PLUGIN_STYLE:      return "Style Transfer";
        case AI_PLUGIN_GENERATE:   return "Sample Generation";
        case AI_PLUGIN_DENOISE:    return "Denoising";
        case AI_PLUGIN_UPSCALE:    return "Upscaling";
        default:                   return "Unknown";
    }
}