/*
 * N-Pole CartPole Physics  --  Production module
 * 
 * Based on Lagrangian mechanics with recursive O(N) algorithm
 * 
 * State convention:
 * - q[0] = cart position x
 * - q[i] for i=1..N = pole angle from HORIZONTAL (upright = π/2)
 * - qd[0] = cart velocity ẋ
 * - qd[i] = pole angular velocity θ̇_i
 * 
 * Pole i (0-indexed in arrays):
 * - mass m[i], length l[i], COM at l[i]/2
 * - Moment of inertia I[i] = m[i] * l[i]² / 3.0 (rod about end)
 * 
 * CartPole-v1 standard parameters (N=1):
 * m_cart = 1.0, m_pole = 0.1, l_pole = 0.5, g = 9.81
 * force = ±10, dt = 0.02
 * 
 * N-pole (cartpole8) parameters:
 * m_cart = 1.0
 * m_i = 0.30 * 0.82^i (decreasing distally)
 * l_i = 0.40 * 0.90^i
 * force = ±80, dt = 0.01
 */

#ifndef NPOLE_PHYSICS_H
#define NPOLE_PHYSICS_H

#include <stddef.h>

#define NPOLE_MAX_POLES 10
#define NPOLE_MAX_STATE (1 + NPOLE_MAX_POLES)  /* cart + N poles */

typedef struct {
    int n;                              /* number of poles */
    
    /* Physical parameters */
    double cart_mass;
    double total_mass;                   /* cart + all poles */
    double pole_mass[NPOLE_MAX_POLES];
    double pole_length[NPOLE_MAX_POLES];
    double pole_com[NPOLE_MAX_POLES];   /* l/2 */
    double pole_inertia[NPOLE_MAX_POLES]; /* m*l²/3 */
    double gravity;
    double force_mag;
    double dt;
    int use_chain;                       /* 1 = blog chain, 0 = independent (stable) */
    
    /* State */
    double q[NPOLE_MAX_STATE];          /* positions: [x, θ₁, θ₂, ...] */
    double qd[NPOLE_MAX_STATE];         /* velocities: [ẋ, θ̇₁, θ̇₂, ...] */
    
    /* Working buffers (allocated once) */
    double th_sums[NPOLE_MAX_POLES + 1];
    double thd_sums[NPOLE_MAX_POLES + 1];
} NPolePhysics;

/* Initialize with standard CartPole-v1 parameters (N=1) */
void npole_init_cartpole(NPolePhysics* phys);

/* Initialize with OpenOCL double-pole params scaled to N poles (2-20) */
void npole_init_openocl(NPolePhysics* phys, int num_poles);

/* Initialize with custom parameters */
void npole_init_custom(NPolePhysics* phys, int num_poles,
                       double cart_mass, const double* pole_mass,
                       const double* pole_length, double gravity,
                       double force_mag, double dt);

/* Reset to upright equilibrium with noise */
void npole_reset_upright(NPolePhysics* phys, double noise_scale);

/* Reset to hanging down with noise */
void npole_reset_down(NPolePhysics* phys, double noise_scale);

/* Compute equations of motion: M(q) * qdd + C(q,qd) + G(q) = B * force
 * Returns accelerations in qdd_out (size n+1)
 */
void npole_compute_accel(NPolePhysics* phys, double force, double* qdd_out);

/* Integration step: semi-implicit Euler */
void npole_step_euler(NPolePhysics* phys, double force);

/* Integration step: 4th-order Runge-Kutta (for N>1 stability) */
void npole_step_rk4(NPolePhysics* phys, double force);

/* Total mechanical energy (kinetic + potential) */
double npole_energy(const NPolePhysics* phys);

/* Check termination conditions */
int npole_is_done(const NPolePhysics* phys, double x_threshold, double theta_threshold);

/* Copy state out (for observation encoding) */
void npole_get_state(const NPolePhysics* phys, double* x, double* x_dot,
                     double* theta, double* theta_dot, int max_poles);

#endif