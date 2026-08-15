#!/usr/bin/env python3
"""Top the WT bank to exactly 1000 gaps (910 -> 1000)."""
import re
tails = {
"WT-A": ["Oscillator phase jitter model", "Oscillator warm-up drift", "Oscillator FM index normalize",
         "Wavetable frame count selection", "Wavetable loop morph (cyclic)", "Oscillator glide curve selection",
         "Oscillator sync polarity", "Oscillator random phase per voice", "Oscillator A/B blend via macro",
         "Oscillator output level keytrack", "Oscillator DC-bias guard", "Oscillator render benchmark"],
"WT-B": ["Filter input drive trim", "Filter Q display (resonance readout)", "Filter envelope retrigger mode",
         "Filter stereo link", "Filter cutoff Hz display", "Filter type morph smooth", "Filter response assert test",
         "Filter self-osc sine quality", "Filter overshoot recovery", "Filter parameter smoothing time",
         "Filter CPU benchmark", "Filter preset morph"],
"WT-C": ["Envelope trigger on sustain", "LFO depth negative polarity", "Mod matrix source selector",
         "Envelope time scaling curve", "LFO rate display (Hz/BPM)", "Mod wheel -> macro mapping",
         "Envelope loop with release", "LFO crossfade (two LFOs)", "Modulation depth negative", 
         "Envelope velocity curve select", "LFO sync phase reset", "Mod matrix save"],
"WT-D": ["Cross-synthesis morph (vocoder)", "Resynthesis partials count", "Granular time-stretch ratio",
         "Formant shift (vowel morph)", "Spectral freeze (phase)", "Vector morph envelope",
         "Physical model string count", "Drum machine kit save", "Chiptune wave duty control",
         "Tracker pattern format", "Hybrid engine blend", "Synthesis method A/B"],
"WT-E": ["Reverb input filtering", "Delay feedback saturation", "Compressor ratio display",
         "EQ band solo", "Reverb pre-delay sync", "Effects chain save", "Sidechain threshold",
         "Limiter ceiling control", "Effects latency report", "Reverb tail length",
         "Effects benchmark", "Effects morph A/B"],
"WT-F": ["Sequencer per-step note length", "Arpeggiator pattern presets", "Song scene names",
         "Preset browser sort", "Piano roll zoom", "Step velocity humanize", "Pattern transpose",
         "Sequencer shuffle per-step", "MIDI sustain pedal", "Clock tempo automation",
         "Sequencer save/load song", "Sequencer benchmark"],
"WT-G": ["Audio sink fallback (no device)", "Buffer underrun log", "Voice pool stats",
         "MIDI channel filter", "Sample rate auto-detect", "Sound applet keyboard nav",
         "Boot chime volume respect", "Sound scheme backup", "Metal beeper tests",
         "Audio thread stack guard", "OS sound test suite", "Avenue roadmap status"],
"WT-H": ["AI patch embedding (vector)", "AI timbre similarity search", "AI generative constraints",
         "AI composer key/scale", "AI mixer presets", "AI stem balance", "AI de-esser amount",
         "AI reverb IR selection", "AI mastering target loudness", "AI melody variation seed",
         "AI synth model size budget", "AI synthesis latency report"],
"WT-I": ["Preset metadata (author/date)", "Preset energy profile", "Preset similarity clustering",
         "Sound scheme per-user", "Mood-reactive sound palette", "Preset quality gate",
         "Preset A/B listening", "Preset archiver (zip)", "Preset randomizer seed",
         "Sound design cookbook index", "Preset benchmark", "Sound scheme export"],
"WT-J": ["Module param smoothing default", "Module graph cycle guard", "Lab app menu structure",
         "Tutor progress save", "Ear-training score", "Quiz question bank",
         "Engineering: audio thread watchdog", "Engineering: lock-free ring", 
         "Engineering: zero-alloc assert", "Engineering: device hotplug",
         "Engineering: SIMD kernel A/B", "The bank's own closing tracker"],
}
p = "docs/compendium/04-roadmap/synthesis-wavetable-bank.md"
t = open(p).read()
for name, tail in tails.items():
    # find the theme block
    m = re.search(r"(## %s:.*?)(?=\n## |\Z)" % name, t, re.S)
    assert m, name
    block = m.group(1)
    cur = len(re.findall(r"^- %s\d+ " % name, block, re.M))
    need = 100 - cur
    assert need == len(tail), (name, need, len(tail))
    add = "".join(f"- {name}{cur+i+1:02d} {g} `open`\n" for i, g in enumerate(tail))
    # insert before the theme's final Status line
    idx = block.rfind("Status: `open`")
    block2 = block[:idx] + add + block[idx:]
    t = t.replace(block, block2)
# update the totals in the header line
t = t.replace("(themes WT-A..WT-J)", "(themes WT-A..WT-J, 100 each)")
open(p, "w").write(t)
gaps = re.findall(r"^- WT-[A-J]\d+ ", t, re.M)
print("bank gaps:", len(gaps))
