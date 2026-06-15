/*
 * bear_mujoco.h  --  MuJoCo Physics Backend for BearRL N-Pole Cartpole
 *
 * Replaces the custom Lagrangian solver with MuJoCo's RNE dynamics.
 * Generates MJCF models programmatically for N-pole cartpole with
 * independent poles (each pole hinges on the cart).
 */
#ifndef BEAR_MUJOCO_H
#define BEAR_MUJOCO_H

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize MuJoCo physics for N-pole cartpole.
 * Returns 0 on success, -1 on failure.
 * Creates MJCF XML, loads model, allocates per-env mjData.
 */
int bear_mujoco_init(int num_poles, int num_envs);

/* Step all environments using MuJoCo physics.
 * theta_in/omega_in: [num_envs][num_poles]  --  current angles and velocities
 * cart_x/vx: [num_envs]  --  current cart pos/vel
 * force: [num_envs]  --  applied cart forces
 * theta_out/omega_out: [num_envs][num_poles]  --  updated
 * cart_x_out/vx_out: [num_envs]  --  updated
 * dt: timestep
 */
void bear_mujoco_step(int num_envs, int num_poles,
                       const float* cart_x, const float* cart_vx,
                       const float* theta_in, const float* omega_in,
                       const float* force, float dt,
                       float* cart_x_out, float* cart_vx_out,
                       float* theta_out, float* omega_out);

/* Reset a specific environment to initial state */
void bear_mujoco_reset_env(int env_id, int num_poles,
                            float cart_x, float cart_vx,
                            const float* theta, const float* omega);

/* Shutdown MuJoCo backends */
void bear_mujoco_shutdown(void);

/* Check if MuJoCo is available (returns 1 if initialized) */
int bear_mujoco_available(void);

#ifdef __cplusplus
}
#endif

#endif /* BEAR_MUJOCO_H */
