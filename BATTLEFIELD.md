# BATTLEFIELD INDEX: N-Pole Cartpole 7-10 (WuBuOS BearRL) — FINAL STATUS
# Updated: 11 June 2026 — Complete Physics + Curriculum + CLI System
# Status: PHYSICS SOLVED, CURRICULUM WORKING, NEEDS MINIGRU + HYPERPARAMETER TUNING

## ═══════════════════════════════════════════════════════════════════
## ✅ COMPLETED — PHYSICS & ENVIRONMENT (SPEC SECTIONS 1-3)
## ══════════════════════════════════════════════════════════════════

100.1 [PHYSICS] dt = 0.02f ✅ (Barto classic)
100.2 [PHYSICS] angle_threshold = 0.20944f (12°) ✅ 
100.3 [PHYSICS] cart_pos_threshold = 2.4f ✅
100.4 [PHYSICS] pole_masses = 0.1f UNIFORM ✅
100.5 [PHYSICS] pole_lengths = 0.5f UNIFORM ✅
100.6 [PHYSICS] gravity = 9.81f ✅
100.7 [PHYSICS] REWARD: r = 1.0 + Σ 0.5·cos(θᵢ) ONLY ✅
100.8 [PHYSICS] MASS MATRIX: I = (1/3)ml², COM at l/2, correct coupling ✅

110.1-110.3 [REWARD] Exact spec implementation ✅

## ═══════════════════════════════════════════════════════════════════
## ✅ COMPLETED — CURRICULUM & EPISODE LENGTH (SPEC SECTION 4)
## ═══════════════════════════════════════════════════════════════════

120.1 [ENV] episode_length_min=100, max=200 initial ✅
120.2 [ENV] Randomized per-episode length (Yacine anti-laziness) ✅
120.3 [ENV] Curriculum triggers: ep_len 200→700 at iter 25 ✅
120.4 [ENV] Threshold: best_return > ep_max * 2.0f ✅
120.5 [ENV] Max 10,000 steps cap ✅

## ═══════════════════════════════════════════════════════════════════
## ✅ COMPLETED — CLI & MINIGRU SUPPORT (SPEC SECTION 5)
## ═══════════════════════════════════════════════════════════════════

140.1 [CLI] --poles, --envs, --iters, --policy (0=MLP,1=MinGRU) ✅
140.2 [CLI] --hid, --hid1, --hid2, --layers, --lr, --epochs, --mb ✅
140.3 [CLI] Backward compat (positional args if no flags) ✅
140.4 [MINIGRU] bear_policy_create_mingru integrated ✅

## ═══════════════════════════════════════════════════════════════════
## 🔄 IN PROGRESS — HYPERPARAMETER TUNING FOR 9500+ RETURN
## ═══════════════════════════════════════════════════════════════════

190.1 [HPARAM] Need epochs_per_iter=10, minibatch=4096 — BLOCKED by 64-env segfault
190.2 [HPARAM] Need target_kl=0.02, normalize_rewards=1 — BLOCKED
190.3 [HPARAM] Need MinGRU for temporal credit assignment — INTEGRATED but untested at 64 envs
190.4 [HPARAM] Need learning rate annealing — WORKING (linear decay)

🚨 BLOCKER: 64-env segfault at larger networks/hyperparams
   - 64 envs + 16,16 hidden + 4 epochs + 2048 mb = WORKS
   - 64 envs + 64,64 hidden + 10 epochs + 4096 mb = SEGFAULT
   - Root cause: tensor dimension/arena capacity with 64 envs × 128 rollout
   - Workaround: use 32 envs for large configs, or increase arena further

## ═══════════════════════════════════════════════════════════════════
## ⏳ PENDING — REMAINING SPEC GAPS
## ═══════════════════════════════════════════════════════════════════

150.1 [EXPORT] Policy weight export to C header (bear_trainer_save/load)
150.2 [EXPORT] --save-model/--load-model/--export-header CLI flags
160.1 [ZEALOS] Bare-metal validation on ZealOS kernel
170.1 [GUI] WorldSim/GUI integration for real-time visualization
180.1 [BENCH] Formal 6-pole benchmark vs MuJoCo/Brax

## ═══════════════════════════════════════════════════════════════════
## 📊 CURRENT PERFORMANCE (7-POLE, 64 ENVS, 200 ITERS)
## ═══════════════════════════════════════════════════════════════════

Best return: 575.98 (at ep_len=700)
Avg return: ~530 (at ep_len=200→700)
Per-step: ~2.65 avg (59% of theoretical max 4.5)
Target: 9500 return @ 10,000 steps (requires ~0.95/step = 21% of max)
Gap: Need policy to sustain >0.95/step for 10,000 steps

## ═══════════════════════════════════════════════════════════════════
## 🎯 NEXT ACTIONS (AUTOPILOT CONTINUES)
## ═══════════════════════════════════════════════════════════════════

1. Fix 64-env segfault → increase arena or use gradient accumulation
2. Enable MinGRU + epochs=10 + mb=4096 + kl=0.02 + norm_rewards=1
3. Run 7-pole → 10-pole progression to 10,000 steps, 9500+ return
4. Policy distillation → C header export
5. ZealOS bare-metal test
6. GUI integration

NEXT EXECUTION: Fix segfault with gradient accumulation or larger arena, then push MinGRU config.
