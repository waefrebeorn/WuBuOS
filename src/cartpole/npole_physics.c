/*
 * N-Pole CartPole Physics  --  Blog-Exact Lagrangian Chain with Semi-Implicit Euler
 * 
 * https://markus7800.github.io/blog/ML/cartpole.html
 * 
 * "The double pendulum... Due to the chaotic behaviour... did not succeed"
 * 
 * But the PHYSICS is the chain Lagrangian. We must implement it correctly.
 * 
 * Key from blog:
 * - Semi-implicit Euler (NOT RK4) for stability
 * - θ from HORIZONTAL, π/2 = upright
 * - q = [x, θ₁, θ₂, ..., θ_N] with θ_i = RELATIVE angle (absolute = sum)
 * - Point mass at l/2 from pivot for each pole
 */

#include "npole_physics.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define M_PI 3.14159265358979323846

static inline void compute_cumulative(const NPolePhysics* phys, double* th_sum, double* thd_sum) {
    th_sum[0] = 0.0; thd_sum[0] = 0.0;
    for (int i = 1; i <= phys->n; ++i) {
        th_sum[i] = th_sum[i-1] + phys->q[i];
        thd_sum[i] = thd_sum[i-1] + phys->qd[i];
    }
}

/* -------------------------- Initialization -------------------------- */

void npole_init_cartpole(NPolePhysics* phys) {
    memset(phys, 0, sizeof(NPolePhysics));
    phys->n = 1;
    phys->cart_mass = 1.0;
    phys->pole_mass[0] = 0.1;
    phys->pole_length[0] = 0.5;
    phys->pole_com[0] = 0.25;
    phys->pole_inertia[0] = phys->pole_mass[0] * phys->pole_length[0] * phys->pole_length[0] / 3.0;
    phys->total_mass = phys->cart_mass + phys->pole_mass[0];
    phys->gravity = 9.81;
    phys->force_mag = 10.0;
    phys->dt = 0.02;
    phys->use_chain = 1;  /* Single pole: chain = independent */
    
    phys->q[0] = 0.0; phys->qd[0] = 0.0;
    phys->q[1] = M_PI / 2; phys->qd[1] = 0.0;
}

void npole_init_cartpole8(NPolePhysics* phys, int num_poles) {
    memset(phys, 0, sizeof(NPolePhysics));
    
    if (num_poles < 1) num_poles = 1;
    if (num_poles > NPOLE_MAX_POLES) num_poles = NPOLE_MAX_POLES;
    phys->n = num_poles;
    
    phys->cart_mass = 1.0;
    phys->gravity = 9.81;
    phys->force_mag = 80.0;
    phys->dt = 0.005;  /* Semi-implicit Euler needs smaller dt for chain stability */
    phys->use_chain = 1;  /* True chain physics by default */
    
    double total_mass = phys->cart_mass;
    for (int i = 0; i < num_poles; ++i) {
        phys->pole_mass[i] = 0.30 * pow(0.82, i + 1);
        phys->pole_length[i] = 0.40 * pow(0.90, i + 1);
        phys->pole_com[i] = phys->pole_length[i] * 0.5;
        phys->pole_inertia[i] = phys->pole_mass[i] * phys->pole_length[i] * phys->pole_length[i] / 3.0;
        total_mass += phys->pole_mass[i];
    }
    phys->total_mass = total_mass;
    
    /* Chain upright: pole1 at π/2 from horizontal, poles 2..N at 0 relative (aligned) */
    phys->q[0] = 0.0; phys->qd[0] = 0.0;
    for (int i = 1; i <= num_poles; ++i) {
        phys->q[i] = (i == 1) ? M_PI / 2 : 0.0;
        phys->qd[i] = 0.0;
    }
}

/* -------------------------- Reset (Chain Training Noise) -------------------------- */
/*
 * Blog training for chain: pole1 absolute from horizontal ∈ [π/2 ± 1.5, π/2 ± 0.5]
 * Poles 2..N relative near 0 (aligned)
 */
void npole_reset_upright(NPolePhysics* phys, double noise_scale) {
    phys->q[0] = ((double)rand() / RAND_MAX - 0.5) * noise_scale;
    phys->qd[0] = ((double)rand() / RAND_MAX - 0.5) * noise_scale;
    if (phys->use_chain && phys->n > 1) {
        phys->q[1] = M_PI / 2 + ((double)rand() / RAND_MAX - 0.5) * noise_scale;
        phys->qd[1] = ((double)rand() / RAND_MAX - 0.5) * noise_scale;
        for (int i = 2; i <= phys->n; ++i) {
            phys->q[i] = ((double)rand() / RAND_MAX - 0.5) * noise_scale;
            phys->qd[i] = ((double)rand() / RAND_MAX - 0.5) * noise_scale;
        }
    } else {
        for (int i = 1; i <= phys->n; ++i) {
            phys->q[i] = M_PI / 2 + ((double)rand() / RAND_MAX - 0.5) * noise_scale;
            phys->qd[i] = ((double)rand() / RAND_MAX - 0.5) * noise_scale;
        }
    }
}

void npole_reset_down(NPolePhysics* phys, double noise_scale) {
    phys->q[0] = ((double)rand() / RAND_MAX - 0.5) * noise_scale;
    phys->qd[0] = ((double)rand() / RAND_MAX - 0.5) * noise_scale;
    for (int i = 1; i <= phys->n; ++i) {
        phys->q[i] = -M_PI / 2 + ((double)rand() / RAND_MAX - 0.5) * noise_scale;
        phys->qd[i] = ((double)rand() / RAND_MAX - 0.5) * noise_scale;
    }
}

/* -------------------------- Single-Pole (Blog Exact) -------------------------- */

static void npole_compute_accel_single(NPolePhysics* phys, double force, double* qdd) {
    double mc = phys->cart_mass, mp = phys->pole_mass[0], lc = phys->pole_com[0], g = phys->gravity;
    double th = phys->q[1], thd = phys->qd[1], s = sin(th), c = cos(th);
    
    double M00 = mc + mp, M01 = mp * lc * c, M11 = mp * lc * lc;
    double F0 = force + mp * lc * thd * thd * s;
    double F1 = -mp * g * lc * c;
    double det = M00 * M11 - M01 * M01;
    qdd[0] = (F0 * M11 - M01 * F1) / det;
    qdd[1] = (M00 * F1 - M01 * F0) / det;
}

/* -------------------------- Multi-Pole Chain (Blog Exact Lagrangian) -------------------------- */
/*
 * q = [x, θ₁, θ₂, ..., θ_N] where θ_i = relative angle
 * θ_sum_i = Σ_{j=1}^i θ_j = absolute angle from horizontal
 * θd_sum_i = Σ_{j=1}^i θ̇_j = absolute angular velocity
 * 
 * M[0][0] = mc + Σ m_i
 * M[0][j] = Σ_{k=j}^{n-1} m_k * (k==j ? lc_k : l_j) * cos(θ_sum_j)  [j>=1]
 * M[i][j] = Σ_{k=max(i,j)}^{n-1} m_k * d_i * d_j * cos(θ_sum_i - θ_sum_j) + δ_ij Σ I_k
 * 
 * F[0] = force + Σ m_i * lc_i * θd_sum_i² * sin(θ_sum_i)
 * F[i] = -Σ_{k=i}^{n-1} m_k * g * (k==i ? lc_i : l_i) * cos(θ_sum_i) + Coriolis
 * 
 * Coriolis: C_i = Σ_j Σ_k (dMij/dqk - 0.5 dMjk/dqi) qd_j qd_k
 */

static void npole_compute_accel_chain(NPolePhysics* phys, double force, double* qdd) {
    int n = phys->n, N = n + 1;
    double th_sum[11], thd_sum[11];
    compute_cumulative(phys, th_sum, thd_sum);
    
    double M[121], F[11];
    memset(M, 0, N * N * sizeof(double));
    memset(F, 0, N * sizeof(double));
    
    M[0] = phys->cart_mass;
    for (int i = 0; i < n; ++i) M[0] += phys->pole_mass[i];
    
    for (int j = 1; j <= n; ++j) {
        double mij = 0.0;
        for (int k = j; k <= n; ++k) {
            double th = th_sum[j];
            if (k == j) mij += phys->pole_mass[k-1] * phys->pole_com[k-1] * cos(th);
            else mij += phys->pole_mass[k-1] * phys->pole_length[j-1] * cos(th);
        }
        M[j * N] = M[j] = mij;
    }
    
    for (int i = 1; i <= n; ++i) {
        for (int j = i; j <= n; ++j) {
            double mij = 0.0;
            int min_ij = i;
            for (int k = min_ij; k <= n; ++k) {
                double th_i = th_sum[i], th_j = th_sum[j];
                double di = (i == k) ? phys->pole_com[k-1] : phys->pole_length[i-1];
                double dj = (j == k) ? phys->pole_com[k-1] : phys->pole_length[j-1];
                mij += phys->pole_mass[k-1] * di * dj * cos(th_i - th_j);
            }
            if (i == j) for (int k = i; k <= n; ++k) mij += phys->pole_inertia[k-1];
            M[i * N + j] = M[j * N + i] = mij;
        }
    }
    
    F[0] = force;
    for (int i = 1; i <= n; ++i) {
        double gi = 0.0;
        for (int k = i; k <= n; ++k) {
            double l = (k == i) ? phys->pole_com[i-1] : phys->pole_length[i-1];
            gi += phys->pole_mass[k-1] * phys->gravity * l * cos(th_sum[i]);
        }
        double ci = 0.0;
        for (int j = 1; j <= n; ++j) {
            for (int k = 1; k <= n; ++k) {
                double dMij_dqk = 0.0, dMjk_dqi = 0.0;
                int min_ij = (i < j) ? i : j;
                int min_jk = (j < k) ? j : k;
                if (k <= min_ij) {
                    double di = (i == k) ? phys->pole_com[k-1] : phys->pole_length[i-1];
                    double dj = (j == k) ? phys->pole_com[k-1] : phys->pole_length[j-1];
                    dMij_dqk = -phys->pole_mass[k-1] * di * dj * sin(th_sum[i] - th_sum[j]);
                }
                if (i <= min_jk) {
                    double di2 = (j == i) ? phys->pole_com[i-1] : phys->pole_length[j-1];
                    double dj2 = (k == i) ? phys->pole_com[i-1] : phys->pole_length[k-1];
                    dMjk_dqi = -phys->pole_mass[i-1] * di2 * dj2 * sin(th_sum[j] - th_sum[k]);
                }
                ci += (dMij_dqk - 0.5 * dMjk_dqi) * phys->qd[j] * phys->qd[k];
            }
        }
        F[i] = -ci - gi;
    }
    
    for (int i = 1; i <= n; ++i) {
        double mi = phys->pole_mass[i-1], lci = phys->pole_com[i-1];
        double sin_thi = sin(th_sum[i]), thd_i = thd_sum[i];
        F[0] += mi * lci * thd_i * thd_i * sin_thi;
    }
    
    double A[121], b[11];
    memcpy(A, M, N * N * sizeof(double));
    memcpy(b, F, N * sizeof(double));
    
    for (int i = 0; i < N; ++i) {
        int piv = i;
        for (int r = i + 1; r < N; ++r)
            if (fabs(A[r * N + i]) > fabs(A[piv * N + i])) piv = r;
        if (piv != i) {
            for (int c = i; c < N; ++c) {
                double t = A[i * N + c]; A[i * N + c] = A[piv * N + c]; A[piv * N + c] = t;
            }
            double t = b[i]; b[i] = b[piv]; b[piv] = t;
        }
        if (fabs(A[i * N + i]) < 1e-12) A[i * N + i] = A[i * N + i] >= 0 ? 1e-12 : -1e-12;
        double diag = A[i * N + i];
        for (int r = i + 1; r < N; ++r) {
            double f = A[r * N + i] / diag;
            for (int c = i; c < N; ++c) A[r * N + c] -= f * A[i * N + c];
            b[r] -= f * b[i];
        }
    }
    for (int i = N - 1; i >= 0; --i) {
        qdd[i] = b[i];
        for (int c = i + 1; c < N; ++c) qdd[i] -= A[i * N + c] * qdd[c];
        qdd[i] /= A[i * N + i];
    }
}

/* -------------------------- Semi-Implicit Euler (Blog Method) -------------------------- */

static void npole_step_semi_implicit(NPolePhysics* phys, double force) {
    double qdd[11];
    if (phys->n == 1) npole_compute_accel_single(phys, force, qdd);
    else npole_compute_accel_chain(phys, force, qdd);
    
    /* Semi-implicit: velocities first, then positions */
    phys->qd[0] += phys->dt * qdd[0];
    phys->q[0] += phys->dt * phys->qd[0];
    for (int i = 1; i <= phys->n; ++i) {
        phys->qd[i] += phys->dt * qdd[i];
        phys->q[i] += phys->dt * phys->qd[i];
    }
}

/* -------------------------- RK4 (for single-pole only) -------------------------- */

static void npole_step_rk4_single(NPolePhysics* phys, double force) {
    int N = 2;
    double dt = phys->dt;
    double q0[2] = {phys->q[0], phys->q[1]};
    double qd0[2] = {phys->qd[0], phys->qd[1]};
    double k1_qdd[2], k2_qdd[2], k3_qdd[2], k4_qdd[2];
    
    npole_compute_accel_single(phys, force, k1_qdd);
    phys->qd[0] = qd0[0] + 0.5*dt*k1_qdd[0]; phys->q[0] = q0[0] + 0.5*dt*qd0[0];
    phys->qd[1] = qd0[1] + 0.5*dt*k1_qdd[1]; phys->q[1] = q0[1] + 0.5*dt*qd0[1];
    npole_compute_accel_single(phys, force, k2_qdd);
    phys->qd[0] = qd0[0] + 0.5*dt*k2_qdd[0]; phys->q[0] = q0[0] + 0.5*dt*(qd0[0]+0.5*dt*k1_qdd[0]);
    phys->qd[1] = qd0[1] + 0.5*dt*k2_qdd[1]; phys->q[1] = q0[1] + 0.5*dt*(qd0[1]+0.5*dt*k1_qdd[1]);
    npole_compute_accel_single(phys, force, k3_qdd);
    phys->qd[0] = qd0[0] + dt*k3_qdd[0]; phys->q[0] = q0[0] + dt*(qd0[0]+0.5*dt*k2_qdd[0]);
    phys->qd[1] = qd0[1] + dt*k3_qdd[1]; phys->q[1] = q0[1] + dt*(qd0[1]+0.5*dt*k2_qdd[1]);
    npole_compute_accel_single(phys, force, k4_qdd);
    phys->qd[0] = qd0[0] + dt/6*(k1_qdd[0]+2*k2_qdd[0]+2*k3_qdd[0]+k4_qdd[0]);
    phys->q[0] = q0[0] + dt/6*(qd0[0]+2*(qd0[0]+0.5*dt*k1_qdd[0])+2*(qd0[0]+0.5*dt*k2_qdd[0])+(qd0[0]+dt*k3_qdd[0]));
    phys->qd[1] = qd0[1] + dt/6*(k1_qdd[1]+2*k2_qdd[1]+2*k3_qdd[1]+k4_qdd[1]);
    phys->q[1] = q0[1] + dt/6*(qd0[1]+2*(qd0[1]+0.5*dt*k1_qdd[1])+2*(qd0[1]+0.5*dt*k2_qdd[1])+(qd0[1]+dt*k3_qdd[1]));
}

/* -------------------------- Public Dispatch -------------------------- */

void npole_compute_accel(NPolePhysics* phys, double force, double* qdd_out) {
    if (phys->n == 1) npole_compute_accel_single(phys, force, qdd_out);
    else npole_compute_accel_chain(phys, force, qdd_out);
}

void npole_step_euler(NPolePhysics* phys, double force) {  /* Alias: semi-implicit */
    npole_step_semi_implicit(phys, force);
}

void npole_step_rk4(NPolePhysics* phys, double force) {
    if (phys->n == 1) npole_step_rk4_single(phys, force);
    else npole_step_semi_implicit(phys, force);  /* Chain uses semi-implicit per blog */
}

/* -------------------------- Energy & Termination -------------------------- */

double npole_energy(const NPolePhysics* phys) {
    int n = phys->n; double E = 0.0;
    double th_sum[11], thd_sum[11];
    compute_cumulative(phys, th_sum, thd_sum);
    
    E += 0.5 * phys->cart_mass * phys->qd[0] * phys->qd[0];
    for (int i = 1; i <= n; ++i) {
        double xi = phys->q[0], yi = 0.0, vxi = phys->qd[0], vyi = 0.0;
        for (int j = 1; j <= i; ++j) {
            double th = th_sum[j], lj = (j < i) ? phys->pole_length[j-1] : phys->pole_com[j-1];
            xi += lj * cos(th); yi += lj * sin(th);
            double thd = thd_sum[j];
            vxi += lj * (-sin(th)) * thd; vyi += lj * cos(th) * thd;
        }
        double mi = phys->pole_mass[i-1];
        E += 0.5 * mi * (vxi*vxi + vyi*vyi) + mi * phys->gravity * yi;
    }
    return E;
}

int npole_is_done(const NPolePhysics* phys, double x_threshold, double theta_threshold) {
    if (fabs(phys->q[0]) > x_threshold) return 1;
    for (int i = 1; i <= phys->n; ++i) {
        double from_upright = fabs(phys->q[i] - M_PI/2);
        while (from_upright > M_PI) from_upright = 2 * M_PI - from_upright;
        if (from_upright > theta_threshold) return 1;
    }
    return 0;
}

void npole_get_state(const NPolePhysics* phys, double* x, double* x_dot,
                     double* theta, double* theta_dot, int max_poles) {
    *x = phys->q[0]; *x_dot = phys->qd[0];
    int n = phys->n < max_poles ? phys->n : max_poles;
    for (int i = 0; i < n; ++i) { theta[i] = phys->q[i+1]; theta_dot[i] = phys->qd[i+1]; }
}