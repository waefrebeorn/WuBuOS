# WuBuOS Reference Library

Study references for architectural parity with major open-source projects.
These are **local reference copies** (git clones, not submodules).

## What's Here

| Directory | Project | Size | Purpose |
|-----------|---------|------|---------|
| `ardour-study/` | Ardour DAW | ~161M | DAW mixer, track/bus routing, audio processing pipeline |
| `fluidsynth-study/` | FluidSynth | ~11M | SF2 SoundFont loading, voices, ADSR, effects, MIDI | 
| `furnace-study/` | Furnace Tracker | ~311M | 80 chip emulations, note/effect dispatch, song format |

## When to Study

- **Ardour**: When implementing DAW features (tracks, buses, sends, automation, session lifecycle)
- **FluidSynth**: When implementing SF2 voice management, pitch bend, CC routing, reverb/chorus
- **Furnace**: When implementing or debugging chip emulations (NES, GameBoy, YM2612, etc.)

All three are added to `.gitignore` — they won't be committed.