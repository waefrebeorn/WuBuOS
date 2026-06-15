/**
 * MASTER RULES for N-Pole Cartpole (from clearspring3/cartpole8)
 * ============================================================
 *
 * 1. PHYSICS - Lagrangian Mechanics (first principles, no RL/NN)
 *    State: z = [x, θ₁..θₙ, ẋ, θ̇₁..θ̇ₙ]
 *    - θ = 0 is UPRIGHT (target), θ = π is HANGING (start)
 *    - Links are massless rods with point masses at ends
 *    - Equations derived from T-V Lagrangian, mass matrix M(q)q̈ + C(q,q̇)q̇ + G(q) = Bτ
 *
 * 2. PARAMETERS (exact from cartpole8, produces solvable 8-pole)
 *    N = 8 poles
 *    g = 9.81 m/s²
 *    Cart mass Mc = 1.0 kg
 *    Pole masses: mᵢ = 0.30 × 0.82ⁱ  (decreasing distally, kg)
 *    Pole lengths: lᵢ = 0.40 × 0.90ⁱ  (decreasing distally, m)
 *    Force limit: 80.0 N
 *    Cart position limit: 2.5 m
 *    Timestep: dt = 0.01 s (for RK4)
 *
 * 3. INTEGRATION - RK4 (4th order Runge-Kutta)
 *    No hidden damping - energy drift ~2e-10 per step
 *    Every trajectory step must re-integrate exactly
 *
 * 4. THE CHALLENGE - "From hanging at rest: swing up, balance all N upright, hold"
 *    START: All poles hanging straight down (θᵢ = π for all i), zero velocity
 *    GOAL:  All poles balanced upright (|θᵢ| < 3° ≈ 0.05 rad) and HOLD indefinitely
 *    No neural networks, no RL - pure model-based control
 *
 * 5. VERIFICATION (3 checks, numpy-only in original, now in C11)
 *    1. DYNAMICS: Every log step re-integrates via RK4, max residual < 1e-10
 *    2. LIMITS: |force| ≤ 80 N, |cart position| ≤ 2.5 m
 *    3. CLAIM: Starts at all π, ends with all N links upright (< 3° error)
 *
 * 6. OUR N-POLE ENVIRONMENT FIXES (applied to BearRL)
 *    - angle_threshold = π/2 (90°) - allow full swing from hanging
 *    - reset: starts from UNSTACKED (θ ≈ π ± random) not near-upright
 *    - episode_length: curriculum from 150 to 10000 steps
 *    - reward: shaped (upright bonus + centering + velocity penalty) for learning
 *    - evaluation: switch to TRUE CartPole-v1 params at test time
 *
 * 7. CONTROLLER INSIGHTS (from cartpole8 solution)
 *    - Energy-based swing-up phase (inject energy to raise center of mass)
 *    - LQR balancing near upright (linearize, solve Riccati)
 *    - Switch between swing-up and balance based on total energy
 *    - Continuous force control [-80, 80] N (not discrete)
 *
 * 8. BEARRL INTEGRATION
 *    - Use these exact params in bear_npolecart_init()
 *    - Shaped reward during training, sparse reward for eval
 *    - Verification binary can replay any .npz trajectory
 */

#ifndef CARTPOLE8_MASTER_RULES_H
#define CARTPOLE8_MASTER_RULES_H

/* Exact parameters from clearspring3/cartpole8 - do not change */
#define CP8_N            8
#define CP8_G            9.81
#define CP8_MC           1.0
#define CP8_U_MAX        80.0
#define CP8_X_LIM        2.5
#define CP8_DT           0.01

/* Mass: m_i = 0.30 * 0.82^i */
/* Length: l_i = 0.40 * 0.90^i */
/* Force: continuous [-80, 80] N */
/* Cart: [-2.5, 2.5] m */
/* Angles: 0 = upright, PI = hanging */

#endif