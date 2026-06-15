# BearRL Handoff: Next Agent Session

## Context
We fixed 4 physics bugs in `bear_env.c` `npole_compute_accelerations()` on 2026-06-13.
All existing `cartpole_*.mp4` videos were generated with **BUGGY physics** — they are **INVALID**.

## The Task
> "ok now properly prove and train a 7-10 cartpole and then a 10-20 cartpole. send me video evidence of the 7-10 when done"

## What to Do

### Phase 1: Rebuild & Verify
1. **Rebuild curriculum_train** with corrected bear_env.o:
   ```bash
   cd /home/wubu/.hermes/profiles/mind-palace/home/myseed
   make src/bear/bear_env.o  # recompile with fixed physics
   # Find curriculum_train source and rebuild:
   src/cartpole/curriculum_train.c  # (if exists) or
   gcc -std=c11 -O2 -g src/bear/bear_env.o ... -o curriculum_train -lm
   ```

2. **Run analytic tests** to confirm physics is correct:
   ```bash
   gcc -std=c11 -O2 src/bear/test_physics_analytic.c -lm -o /tmp/test_phys
   /tmp/test_phys
   ```

### Phase 2: Train 7-10 Poles + Generate Videos
1. **Train with GAAD optimizer** (RL approach):
   ```bash
   cd /home/wubu/.hermes/profiles/mind-palace/home/myseed/src/bear
   ./bear_train --poles 7 --envs 64 --iters 5000 --optimizer gaad --hid1 128 --hid2 128
   ```
   - This trains for 7-pole, 64 parallel envs, 5000 iterations
   - On WSL (14GB RAM, no GPU): expect ~1M steps/min → ~90 min for 5000 iters

2. **Or use model-based control** (LQR + energy swing-up, faster):
   ```bash
   cd /home/wubu/.hermes/profiles/mind-palace/home/myseed
   ./bear_cartpole_physics --from 7 --to 10
   ```
   - This uses model-based control (no RL training needed)
   - Generates videos automatically via PPM → ffmpeg pipeline

3. **Video generation**: After training, run inference with video recording:
   ```python
   import subprocess
   # Encode PPM frames to MP4
   subprocess.run(['ffmpeg', '-y', '-framerate', '30', '-pattern_type', 'glob',
                   '-i', '/tmp/frames/*.ppm', '-c:v', 'libx264', '-crf', '23',
                   'cartpole_7pole.mp4'])
   ```

### Phase 3: Train 10-20 Poles + Generate Videos
Same as Phase 2 but with pole counts 10-20.

## Key Files

### Physics (FIXED)
| File | Purpose |
|------|---------|
| `src/bear/bear_env.c` (lines 642-751) | `npole_compute_accelerations()` — **4 bugs fixed** |
| `src/bear/bear_gaad.h/.c` | GAAD optimizer (TGT, anisotropic, resonant, Poincaré) |
| `src/bear/bear_gaad_train.h/.c` | GAAD-PPO flat-buffer bridge |
| `src/bear/bear_train.c` | Training binary with `--optimizer gaad\|adam` |
| `src/bear/bear_mujoco.h/.c` | MuJoCo reference backend (optional) |

### Training
- `src/bear/bear_train` — GAAD-optimized PPO training binary
- `curriculum_train` — **EXISTING BUT NEEDS REBUILD** (uses old buggy physics)
- `src/bear/bear_cartpole_physics` — Model-based LQR/energy swing-up demo

### Video Pipeline
- `src/bear/bear_cartpole_physics.c` (lines 182-211) — PPM → ffmpeg encoding
- `scripts/curriculum_train_runner.py` — Automation script (calls curriculum_train)
- `/tmp/phys_*pole_*/` — Temp directory for PPM frames

### Tests
- `src/bear/test_physics_analytic.c` — Run: `gcc -lm ... && ./test_physics_analytic`
- `src/bear/test_mujoco.c` — MuJoCo cross-validation (needs LD_LIBRARY_PATH)

## Critical Pitfalls
1. **All existing MP4s are invalid** — Delete before re-running:
   ```bash
   rm cartpole_*.mp4
   ```
2. **Dynamically linked binaries don't auto-update** — `bear_cartpole_physics` and `curriculum_train` must be **REBUILT** to get corrected physics
3. **GAAD uses calloc** for anisotropic scales — freed in `bear_gaad_destroy()`. Always call cleanup.
4. **MuJoCo not in standard LD_LIBRARY_PATH** — prefix with `LD_LIBRARY_PATH=/home/wubu/mujoco_local/mujoco-3.2.7/lib`
5. **No GPU** — training is CPU-only. 64 envs × 1024 steps × 5000 iters = 328M steps ≈ 5 hours for 7-pole
6. **WSL RAM** — 14GB available. Arena caps at 256MB for global + 32MB each for rollout/step. Should be fine.

## GAAD Config for Training
```c
base_lr = 3e-4
beta1 = 0.9, beta2 = 0.999
use_anisotropic = 1, use_resonant = 1, use_poincare = 1
curvature = 0.1f (GENTLE — too high causes NaN)
resonant_window = 50
```

## Expected Training Signal
- Initial return: ~6-7 (7 poles, random policy, 150-step episodes)
- After 500 iters: ~50-100 (policy learns basic balancing)
- After 5000 iters: ~500+ (longer episodes via curriculum)
- Target: 9500+ (10K-step episodes, near-perfect balance)
