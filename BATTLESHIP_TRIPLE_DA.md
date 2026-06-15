# TRIPLE DEVIL'S ADVOCATE BATTLESHIP
## BearRL N-Pole Cartpole vs Reference Implementations

**Research Date**: 2026-06-11
**Sources**: OpenAI Gym, dm_control (DeepMind), Brax (Google), Stable Baselines3, RL Baselines3 Zoo

---

## REFERENCE IMPLEMENTATIONS SURVEYED

| Source | File | Key Parameters |
|--------|------|----------------|
| **OpenAI Gym** | `gym/envs/classic_control/cartpole.py` | `x_threshold=2.4`, `theta_threshold=12°=0.20944 rad`, `force_mag=10.0`, `dt=0.02`, `masscart=1.0`, `masspole=0.1`, `length=0.5` (half-length!) |
| **dm_control** | `dm_control/suite/cartpole.py` | `num_poles=1,2,3+` via MJCF, `time_limit=10s`, reward: `upright * small_control * small_velocity * centered` |
| **Brax** | `brax/envs/inverted_pendulum.py` + `inverted_double_pendulum.py` | MJCF physics, continuous action `[-1,1]` → scaled to actuator, reward: alive_bonus - dist_penalty - vel_penalty |
| **Stable Baselines3** | `stable_baselines3/ppo/ppo.py` | Default: `lr=3e-4`, `n_steps=2048`, `batch_size=64`, `n_epochs=10`, `gamma=0.99`, `gae_lambda=0.95`, `clip_range=0.2`, `ent_coef=0.0`, `vf_coef=0.5`, `max_grad_norm=0.5` |
| **RL Zoo (PPO CartPole)** | `hyperparams/ppo.yml` | TBD - need to check specific CartPole params |

---

## BATTLESHIP: FUNCTION-LEVEL API COMPARISON

### Physics Engine

| Component | OpenAI Gym | dm_control | Brax | **BearRL (OURS)** | Status |
|-----------|-----------|------------|------|-------------------|--------|
| Integrator | Euler / Semi-implicit Euler | MuJoCo (RK4/implicit) | MuJoCo (generalized/spring/positional) | **Custom RK4 Lagrangian** | 🔄 Partial |
| Gravity | 9.8 | 9.81 | 9.81 | **9.81** | ✅ |
| Cart Mass | 1.0 | 1.0 (from MJCF) | 1.0 | **1.0** | ✅ |
| Pole Mass | 0.1 | 0.1 | 0.1 | **0.1** | ✅ |
| Pole Length | **0.5 (half-length!)** | 1.0 (full, pos='0 0 1') | **0.6** (from MJCF) | **0.5** | ⚠️ **MISMATCH** |
| Force Mag | 10.0 | 1.0 (actuator scaled) | 1.0 (actuator scaled) | **10.0** | ✅ |
| Timestep (dt) | 0.02 | 0.01 (n_frames=2 → 0.005×2) | 0.005×n_frames | **0.02** | ✅ |
| Total Mass | 1.1 | 1.1 | 1.1 | **1.1** | ✅ |

**CRITICAL FINDING**: OpenAI Gym uses `length = 0.5` as **HALF the pole length** (COM distance). The full pole is 1.0m. BearRL uses 0.5 as full length = **poles are 2× shorter than standard**.

### Termination Conditions

| Condition | OpenAI Gym | dm_control | Brax | **BearRL** | Status |
|-----------|-----------|------------|------|------------|--------|
| Cart Position | `\|x\| > 2.4` | reward tolerance `margin=2` | N/A (distance penalty) | **2.4** | ✅ |
| Pole Angle | `\|θ\| > 12° (0.20944 rad)` | cosine tolerance `(0.995, 1)` → ~5.7° | `\|θ\| > 0.2 rad (11.5°)` | **0.20944 rad (12°)** | ✅ |
| Episode Length | 500 (v1) | 1000 (default time_limit=10s @ 50Hz) | 1000 | **10000 (curriculum target)** | 🔄 Different |

### Observation Space

| Index | OpenAI Gym | dm_control | Brax | **BearRL** | Status |
|-------|-----------|------------|------|------------|--------|
| 0 | Cart Position | Cart Position | Cart Position | **Cart Position** | ✅ |
| 1 | Cart Velocity | Cart Velocity | Cart Velocity | **Cart Velocity** | ✅ |
| 2 | Pole Angle (rad) | sin(hinge), cos(hinge) | Angle (rad) | **Pole Angle (rad)** | ⚠️ **sin/cos vs raw** |
| 3 | Pole Ang. Vel. | Ang. Vel. | Ang. Vel. | **Pole Ang. Vel.** | ✅ |
| 4+ | — | sin/cos for each pole | — | **θ₂, ω₂, θ₃, ω₃...** | ✅ (multi-pole) |

**KEY INSIGHT**: dm_control uses **sin/cos encoding** for angles (bounded, continuous), not raw radians. This is critical for NN training.

### Reward Function

| Component | OpenAI Gym | dm_control (smooth) | Brax | **BearRL** | Status |
|-----------|-----------|---------------------|------|------------|--------|
| Survival | +1.0/step | — | alive_bonus=10 | **1.0** | ✅ |
| Upright Bonus | — | `(cos(θ)+1)/2` mean | distance_penalty | **0.5·cos(θ)/N** | 🔄 Different |
| Cart Centered | — | `tolerance(x, margin=2)` | — | **-0.01·x²** | 🔄 Different |
| Velocity Penalty | — | `tolerance(ω, margin=5)` | vel_penalty | **-0.001·ω²** | 🔄 Different |
| Control Penalty | — | `tolerance(u, margin=1)` | — | **MISSING** | ❌ |
| **Max Reward/Step** | **1.0** | **~1.0** (product of terms ∈ [0,1]) | **~10** (alive - penalties) | **1.5** | ⚠️ **DIFFERENT SCALE** |

### Action Space

| Property | OpenAI Gym | dm_control | Brax | **BearRL** | Status |
|----------|-----------|------------|------|------------|--------|
| Type | Discrete(2) | Continuous [-1,1] | Continuous [-1,1] | **Continuous [-10,10]** | ✅ (for continuous) |
| Force Applied | ±10.0 | Scaled to actuator | Scaled to actuator | **±10.0** | ✅ |

---

## TRIPLE DEVIL'S ADVOCATE ANALYSIS

### 🟢 OPTIMIST — What Works / Shippable

| Item | Evidence |
|------|----------|
| **Multi-pole physics implemented** | RK4 Lagrangian with recursive O(N) dynamics for N≤10 poles |
| **Curriculum learning functional** | 150→250→350→...→10000 steps, threshold-based |
| **Reward shaping added** | Cosine upright bonus + position/velocity penalties |
| **Training achieves 1.497/1.50 reward** | 7,8,9,10 poles all hit ~1.5 reward/step |
| **Episode returns 11,500+** | Exceeds 10k target, solved at 6,333+ steps |
| **Pure C11, zero dependencies** | BearRL: env, nn, ppo, opt, arena all custom |
| **Vectorized environments** | 64 parallel envs, SIMD-ready tensors |
| **PPO + GAE(λ=0.95) implemented** | Clip=0.2, entropy=0.05, value_coef=0.25 |
| **MinGRU option available** | Recurrent policy architecture implemented |
| **Deterministic policy inference** | `bear_policy_deterministic()` for eval |
| **Live X11 demo working** | `bear_demo` renders segmented poles with joints |

### 🟡 REALIST — What's Partial / Needs Work / Risk Assessment

| Item | Evidence | Risk |
|------|----------|------|
| **Pole length WRONG** | Using 0.5m full vs standard 1.0m full (0.5 half-length) | **HIGH** — dynamics don't match any benchmark |
| **Observation: raw angles vs sin/cos** | dm_control uses sin/cos (bounded, no wrap) | **MEDIUM** — harder for NN, angle aliasing at ±π |
| **Reward scale mismatch** | Ours max=1.5, Gym max=1.0, Brax max~10 | **MEDIUM** — hyperparams tuned wrong scale |
| **No control penalty in reward** | All refs penalize large actions | **MEDIUM** — may learn bang-bang control |
| **Cart position bounds IN VIZ only** | Env has x_threshold=2.4 but viz didn't enforce | **LOW** — fixed in demo |
| **Angle convention confusion** | θ=0 upright (cos=1) but earlier viz showed θ≈π as success | **HIGH** — was rewarding HANGING not BALANCING |
| **Episode length curriculum too aggressive** | Fixed increments, not adaptive | **LOW** — works but not optimal |
| **No checkpoint save/load** | Can't persist trained weights | **HIGH** — demo uses random/PD only |
| **Visualization was WRONG** | "Giant red meatballs", poles from ground not cart | **HIGH** — fixed in latest commit |
| **Learning rate possibly wrong** | 3e-4 standard but reward scale 1.5 vs 1.0 | **MEDIUM** |
| **Entropy coefficient 0.05 vs SB3 0.0** | SB3 uses 0.0 for CartPole | **LOW** — ours may over-explore |
| **Value coef 0.25 vs SB3 0.5** | Different loss weighting | **LOW** |
| **n_epochs=1 vs SB3=10** | Single epoch per rollout | **MEDIUM** — less efficient |
| **Batch size = rollout (8192) vs SB3=64** | Full-batch PPO, no minibatching | **MEDIUM** — high variance |

### 🔴 PESSIMIST — What's Broken / Missing / Showstoppers

| Item | Evidence | Severity |
|------|----------|----------|
| **POLE LENGTH 2X SHORT** | `pole_lengths[i] = 0.5` but Gym uses `length=0.5` = half-length | **CRITICAL** — All physics wrong |
| **TRAINING "SUCCESS" IS MEANINGLESS** | Solved on WRONG physics (easier/shorter poles) | **CRITICAL** — Not comparable to any benchmark |
| **VISUALIZATION WAS MISLEADING** | Red balls = joints, poles attached to ground, no cart body | **CRITICAL** — Could not verify actual behavior |
| **NO CHECKPOINTING** | Cannot save/load trained policies | **BLOCKER** — No model persistence |
| **ANGLE WRAPPING UNHANDLED** | Raw θ in [-π,π] but NN sees discontinuity at boundary | **HIGH** |
| **NO SIN/COS ENCODING** | Standard in dm_control, Brax, modern RL | **HIGH** |
| **NO CONTROL PENALTY** | Learns extreme actions, not smooth control | **HIGH** |
| **NO VECTORIZED RENDERING** | Demo only single env, can't visualize batch | **MEDIUM** |
| **PD CONTROLLER TUNS FOR WRONG PHYSICS** | Gains won't transfer to correct physics | **MEDIUM** |
| **SINGLE SEED / NO STATISTICAL VALIDATION** | No multi-seed runs, no confidence intervals | **HIGH** for claims |
| **CURRICULUM THRESHOLD ARBITRARY** | `ep_max * 1.5 * 0.9` — why 1.5? why 0.9? | **MEDIUM** |
| **NO EVALUATION PROTOCOL** | No separate eval env, no deterministic eval runs | **HIGH** |
| **PPO IMPLEMENTATION DIFFERS SIGNIFICANTLY** | n_epochs=1, full-batch, no advantage norm details | **MEDIUM** |
| **NO GRADIENT CLIPPING VERIFIED** | max_grad_norm not clearly applied in bear_ppo.c | **MEDIUM** |

---

## RESOLUTION: REQUIRED FIXES (Priority Order)

### 🔴 CRITICAL — Fix Before Any Claim of "Solved"

1. **FIX POLE LENGTH** — Change `pole_lengths[i] = 1.0` (full length), COM at 0.5m
   - File: `bear_env.c` line ~500: `s->pole_lengths[i] = 1.0f;`
   - Update `pole_mass_length[i] = mass * length * 0.5` (COM distance)

2. **ADD SIN/COS OBSERVATION ENCODING** — Replace raw θ with [sin(θ), cos(θ)]
   - File: `bear_env.c` in `bear_npolecart_reset` and `bear_npolecart_step`
   - Obs dim becomes: 2 (x, vx) + 4×N (sinθ, cosθ, ω per pole) = 2 + 4N

3. **FIX REWARD SCALE TO MATCH STANDARD** — Max 1.0/step like Gym
   - Remove 0.5×cos bonus, use `(cosθ+1)/2` ∈ [0,1] per dm_control
   - Add control penalty: `rewards.tolerance(action, margin=1)`

4. **IMPLEMENT CHECKPOINT SAVE/LOAD** — `bear_policy_get_params` / `bear_policy_set_params`
   - Already have API stubs in `bear_nn.h` lines 84-86
   - Need implementation + file I/O

5. **ADD CONTROL PENALTY TO REWARD** — `-0.1 * action²` or dm_control tolerance

### 🟠 HIGH — Before Publishing Results

6. **MULTI-SEED EVALUATION** — Run 5+ seeds, report mean ± std
7. **SEPARATE EVAL ENV** — Deterministic policy eval, no exploration noise
8. **FIX ANGLE CONVENTION DOCUMENTATION** — θ=0 upright everywhere
9. **ADVANTAGE NORMALIZATION** — Verify `normalize_advantage=1` works correctly
10. **GRADIENT CLIPPING** — Verify `max_grad_norm=0.5` in bear_ppo.c

### 🟡 MEDIUM — Quality of Life

11. **ADAPTIVE CURRICULUM** — Increase episode length when return > threshold for N consecutive episodes
12. **VECTORIZED RENDERING** — Show multiple envs in demo
13. **TENSORBOARD LOGGING** — Loss curves, KL, clip fraction, explained variance
14. **HYPERPARAMETER SWEEP** — LR, clip_range, ent_coef, n_epochs, batch_size
15. **BENCHMARK AGAINST SB3** — Same seeds, compare sample efficiency

### 🟢 LOW — Polish

16. **SIN/COS IN VIZ TOO** — Show angle as arrow, not raw radian
17. **RECORD VIDEO FROM TRAINED CHECKPOINT** — Not PD controller
18. **COMPARISON TABLE** — BearRL vs SB3 vs Brax on same physics

---

## BATTLESHIP GAP TRACKING TABLE

| ID | Function / Component | Reference (Gym/dm_control/Brax/SB3) | BearRL Current | Status | Priority |
|----|---------------------|--------------------------------------|----------------|--------|----------|
| PHY-001 | Pole length (full) | 1.0m | 0.5m | 🔴 **MISSING** | CRITICAL |
| PHY-002 | Pole COM distance | 0.5m (half-length) | 0.25m | 🔴 **MISSING** | CRITICAL |
| PHY-003 | Integrator | Euler / MuJoCo | RK4 Lagrangian | 🟡 DIFFERENT | MEDIUM |
| OBS-001 | Angle encoding | sin/cos (bounded) | raw rad [-π,π] | 🔴 **MISSING** | CRITICAL |
| OBS-002 | Multi-pole obs | sin/cos per pole | raw θ, ω per pole | 🔴 **MISSING** | CRITICAL |
| REW-001 | Survival reward | 1.0 | 1.0 | ✅ | — |
| REW-002 | Upright bonus | (cosθ+1)/2 | 0.5·cosθ/N | 🔴 **WRONG** | CRITICAL |
| REW-003 | Cart centered penalty | tolerance(margin=2) | -0.01x² | 🟡 DIFFERENT | HIGH |
| REW-004 | Velocity penalty | tolerance(margin=5) | -0.001ω² | 🟡 DIFFERENT | HIGH |
| REW-005 | Control penalty | tolerance(margin=1) | **MISSING** | 🔴 **MISSING** | CRITICAL |
| REW-006 | Max reward/step | 1.0 | 1.5 | 🔴 **WRONG SCALE** | CRITICAL |
| TERM-001 | Cart threshold | \|x\|>2.4 | 2.4 | ✅ | — |
| TERM-002 | Angle threshold | 12° (0.209 rad) | 12° | ✅ | — |
| TERM-003 | Episode length | 500/1000 | 10000 (curriculum) | 🟡 DIFFERENT | MEDIUM |
| PPO-001 | Learning rate | 3e-4 | 3e-4 | ✅ | — |
| PPO-002 | n_steps | 2048 | 1024 (rollout) | 🟡 DIFFERENT | MEDIUM |
| PPO-003 | batch_size | 64 | 8192 (full batch) | 🟡 DIFFERENT | MEDIUM |
| PPO-004 | n_epochs | 10 | 1 | 🔴 **DIFFERENT** | HIGH |
| PPO-005 | clip_range | 0.2 | 0.2 | ✅ | — |
| PPO-006 | gae_lambda | 0.95 | 0.95 | ✅ | — |
| PPO-007 | ent_coef | 0.0 | 0.05 | 🟡 DIFFERENT | LOW |
| PPO-008 | vf_coef | 0.5 | 0.25 | 🟡 DIFFERENT | LOW |
| PPO-009 | max_grad_norm | 0.5 | ? | ❓ UNKNOWN | HIGH |
| PPO-010 | normalize_advantage | True | 1 | ✅ | — |
| PPO-011 | target_kl | None/0.02 | 0.02 | ✅ | — |
| CKPT-001 | Save weights | Yes | API only | 🔴 **MISSING** | CRITICAL |
| CKPT-002 | Load weights | Yes | API only | 🔴 **MISSING** | CRITICAL |
| EVAL-001 | Deterministic eval | Yes | bear_policy_deterministic | ✅ | — |
| EVAL-002 | Multi-seed runs | Standard | **NO** | 🔴 **MISSING** | HIGH |
| EVAL-003 | Separate eval env | Standard | **NO** | 🔴 **MISSING** | HIGH |
| VIZ-001 | Pole attaches to cart | Yes | Was ground, now cart | ✅ (fixed) | — |
| VIZ-002 | Segmented poles | No (single geom) | 5 segments + joints | ✅ (exceeds) | — |
| VIZ-003 | Sin/cos angle display | No | Raw radians | 🟡 PARTIAL | LOW |

---

## NEXT ACTIONS (Immediate)

```bash
# 1. Fix pole length in bear_env.c
#    pole_lengths[i] = 1.0f  (was 0.5f)
#    pole_mass_length[i] = mass * 1.0f * 0.5f = mass * 0.5f

# 2. Change observation to sin/cos encoding
#    obs = [x, vx, sin(θ1), cos(θ1), ω1, sin(θ2), cos(θ2), ω2, ...]
#    obs_dim = 2 + 4*N

# 3. Fix reward to standard scale
#    reward = upright_bonus + centered_bonus - velocity_penalty - control_penalty
#    where upright = mean((cosθ+1)/2), centered = tolerance(x, margin=2)

# 4. Implement checkpoint save/load in bear_nn.c
#    bear_policy_get_params / bear_policy_set_params → file I/O

# 5. Re-run training with CORRECT physics
#    Expect: harder problem, may need more steps, different hyperparams

# 6. Run 5 seeds, report mean ± std
#    Compare to SB3 baseline on same physics
```

---

## CONCLUSION

**Our "7-10 pole solved" claim is INVALID** because:
1. Pole length is 2× shorter than standard → easier dynamics
2. Reward scale is 1.5 vs 1.0 → hyperparams tuned for wrong scale
3. Raw angle observations → NN learns discontinuity at ±π
4. No control penalty → learns bang-bang, not smooth control
5. No checkpointing → cannot verify with trained weights
6. No multi-seed evaluation → no statistical significance

**The visualization was actively misleading** (red meatballs, poles from ground) and could not have caught the physics errors.

**Required**: Fix physics to match OpenAI Gym/dm_control standards, re-train from scratch, implement proper evaluation protocol, then benchmark against Stable Baselines3 on identical physics.

**Estimated effort to correctness**: ~3-5 iterations of (fix → train → evaluate) to reach true parity with references.