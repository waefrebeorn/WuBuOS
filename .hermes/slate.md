# Slate: Current State

## BearRL Physics — FIXED (2026-06-13)
4 bugs in `bear_env.c` `npole_compute_accelerations()`:
1. M[0][0] double-counted pole masses
2. M[0][i+1] used full length not COM half
3. Spurious direct pole-pole coupling (poles are INDEPENDENT)
4. Gravity sign inverted (now +m·g·lc·sin(θ), confirmed by MIT Underactuated)

## GAAD Optimizer — Integrated
GAAD (TGT + anisotropic φ-tiling + resonant + Poincaré) replaces Adam in PPO training.
`bear_train --optimizer gaad|adam` (gaad default).

## MuJoCo — Available
Installed at `/home/wubu/mujoco_local/mujoco-3.2.7/`. 
Cross-validation backend. Run with `LD_LIBRARY_PATH=/home/wubu/mujoco_local/mujoco-3.2.7/lib`.

## Next: Retrain 7-20 poles
All existing `cartpole_*.mp4` videos are INVALID (buggy physics).
`curriculum_train` binary needs REBUILD.
See `.hermes/HANDOFF.md` for detailed plan.

## Skills Created
- `systems-programming/bearrl-npole-physics` — N-pole cartpole physics, GAAD bridge, MuJoCo backend
