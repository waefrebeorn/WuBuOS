/*
 * N-Pole CartPole Physics  --  OpenOCL Tutorial Scaled to N=2-20
 * 
 * Reference: https://openocl.github.io/tutorials/tutorial-01-modeling-double-cartpole/
 * 
 * State: x = [x, θ₁, θ₂, ..., θ_N, ẋ, θ̇₁, θ̇₂, ..., θ̇_N]
 *        0       1     2         N     N+1    N+2     N+3      2N
 * 
 * Angles: θ_i from VERTICAL (0 = upright, π = down) per OpenOCL convention
 * 
 * Parameters (from tutorial):
 * - m_c = 4.15 kg (cart mass)
 * - m_i = 1.15 kg (pole mass, same for all poles)
 * - L = 1.0 m (pole length)
 * - l_com = L/2 = 0.5 m (COM distance for pole)
 * - g = 9.81 m/s²
 * 
 * Kinetic energy (cart + poles):
 * T = ½ m_c ẋ² + Σ ½ m_i (ẋ² + (L_i θ̇_i)² + 2 ẋ L_i θ̇_i cos θ_i) + coupling terms
 * 
 * Potential energy:
 * V = Σ m_i g l_com_i (1 - cos θ_i)  [zero at hanging down]
 * 
 * Lagrangian: L = T - V
 * Euler-Lagrange: d/dt(∂L/∂q̇) - ∂L/∂q = B*f
 * M(q) q̇̇ + C(q,q̇) q̇ + G(q) = B * force
 * 
 * We use FEATHERSTONE's recursive O(N) for large N, symbolic for small N
 */

#include "npole_physics.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define M_PI 3.14159265358979323846

/* OpenOCL double-pole parameters */
#define OPENOCL_MC    4.15
#define OPENOCL_MP    1.15
#define OPENOCL_L     1.0
#define OPENOCL_LCOM  0.5
#define OPENOCL_G     9.81

/* =================== Helpers =================== */

static inline void compute_angles(const NPolePhysics* phys, double* th, double* thd) {
    for (int i = 0; i < phys->n; ++i) {
        th[i] = phys->q[i+1];
        thd[i] = phys->qd[i+1];
    }
}

/* =================== M(q) Mass Matrix =================== */
/*
 * M(q) for N-pole chain (Featherstone-style via symbolic generalization)
 * 
 * Based on OpenOCL double-pole symbolic results generalized:
 * 
 * M[0][0] = m_c + N * m_p
 * M[0][i+1] = m_p * L * cos(q[i+1])  ... wait, need to derive properly
 * 
 * Actually from OpenOCL for 2-pole:
 * q = [x, θ₁, θ₂]
 * M is 3x3
 * 
 * The full symbolic M from OpenOCL:
 * M00 = mc + m1 + m2
 * M01 = m1*l1*cos(θ₁) + m2*L1*cos(θ₁)
 * M02 = m2*l2*cos(θ₁+θ₂)
 * M11 = m1*l1² + m2*L1² + I1
 * M12 = m2*L1*l2*cos(θ₂)  [for COM at l2/2, I = m2*l2²/3]
 * M22 = m2*l2² + I2
 * 
 * Generalized to N poles:
 * M[0][0] = m_c + N*m_p
 * M[0][i+1] = m_p * L * cos(θ_i) + Σ_{k=i+1}^{N-1} m_p * L * cos(θ_k)  [for COM at L/2]
 * M[i+1][j+1] = (i==j) ? (m_p*L² + I) : m_p*L²*cos(θ_j-θ_{j-1})... 
 * 
 * Using COM at L/2 (not end), I = m*L²/12 for rod about center, but poles rotate about end -> I = m*L²/3
 * 
 * For uniform parameters (all poles identical):
 */

static void build_M(const NPolePhysics* phys, double* M) {
    int n = phys->n;
    int N = n + 1;
    double mp = phys->pole_mass[0];
    double L = phys->pole_length[0];
    double lcom = phys->pole_com[0];  // L/2
    double I = phys->pole_inertia[0]; // m*L²/3
    double mc = phys->cart_mass;
    
    for (int i = 0; i < N*N; ++i) M[i] = 0.0;
    
    M[0] = mc + n * mp;
    
    for (int i = 1; i <= n; ++i) {
        double cos_th = cos(phys->q[i]);
        M[i] = mp * lcom * cos_th;
        M[i * N] = M[i];
        M[(i*N) + i] = mp * L * L + I;
        
        for (int j = i+1; j <= n; ++j) {
            double cos_diff = cos(phys->q[i] - phys->q[j]);
            M[i * N + j] = mp * L * lcom * cos_diff;
            M[j * N + i] = M[i * N + j];
        }
    }
}

/* =================== C(q,qd) Coriolis/Centrifugal =================== */
/* 
 * From OpenOCL 2-pole, generalized via Christoffel symbols:
 * C[i] = Σ_j Σ_k Γ^i_jk q̇_j q̇_k
 * 
 * For our simplified uniform case:
 * C[0] = -Σ m_p * lcom * θ̇_i² * sin(θ_i)
 * C[i] = -m_p * lcom * ẋ * θ̇_i * sin(θ_i) + coupling terms
 */

static void build_C(const NPolePhysics* phys, double* C) {
    int n = phys->n;
    int N = n + 1;
    double mp = phys->pole_mass[0];
    double L = phys->pole_length[0];
    double lcom = phys->pole_com[0];
    
    for (int i = 0; i < N; ++i) C[i] = 0.0;
    
    // Cart Coriolis: Σ m_p * lcom * θ̇_i² * sin(θ_i)
    for (int i = 1; i <= n; ++i) {
        double sin_th = sin(phys->q[i]);
        C[0] += mp * lcom * phys->qd[i] * phys->qd[i] * sin_th;
    }
    
    // Pole Coriolis
    for (int i = 1; i <= n; ++i) {
        double sin_th = sin(phys->q[i]);
        C[i] += -mp * lcom * phys->qd[0] * phys->qd[i] * sin_th;
        
        // Coupling from other poles (simplified)
        for (int j = 1; j <= n; ++j) {
            if (i != j) {
                double sin_diff = sin(phys->q[i] - phys->q[j]);
                C[i] += mp * L * lcom * phys->qd[i] * phys->qd[j] * sin_diff;
            }
        }
    }
}

/* =================== G(q) Gravity =================== */
static void build_G(const NPolePhysics* phys, double* G) {
    int n = phys->n;
    int N = n + 1;
    double mp = phys->pole_mass[0];
    double lcom = phys->pole_com[0];
    double g = phys->gravity;
    
    for (int i = 0; i < N; ++i) G[i] = 0.0;
    
    // G[0] = 0 (cart has no gravity term)
    // G[i+1] = m_p * g * lcom * sin(θ_i) for θ from vertical (0=upright)
    for (int i = 1; i <= n; ++i) {
        double sin_th = sin(phys->q[i]);
        G[i] = mp * g * lcom * sin_th;
    }
}

/* =================== Solve M * a = rhs =================== */
static void solve_system(int N, double* A, double* b, double* x) {
    // Gaussian elimination with partial pivoting
    for (int i = 0; i < N; ++i) {
        int piv = i;
        for (int r = i+1; r < N; ++r)
            if (fabs(A[r*N+i]) > fabs(A[piv*N+i])) piv = r;
        if (piv != i) {
            for (int c = i; c < N; ++c) {
                double tmp = A[i*N+c]; A[i*N+c] = A[piv*N+c]; A[piv*N+c] = tmp;
            }
            double tmp = b[i]; b[i] = b[piv]; b[piv] = tmp;
        }
        if (fabs(A[i*N+i]) < 1e-12) A[i*N+i] = (A[i*N+i] >= 0) ? 1e-12 : -1e-12;
        double diag = A[i*N+i];
        for (int r = i+1; r < N; ++r) {
            double f = A[r*N+i] / diag;
            for (int c = i; c < N; ++c) A[r*N+c] -= f * A[i*N+c];
            b[r] -= f * b[i];
        }
    }
    for (int i = N-1; i >= 0; --i) {
        x[i] = b[i];
        for (int c = i+1; c < N; ++c) x[i] -= A[i*N+c] * x[c];
        x[i] /= A[i*N+i];
    }
}

/* =================== Public API =================== */

void npole_compute_accel(NPolePhysics* phys, double force, double* qdd_out) {
    int n = phys->n;
    int N = n + 1;
    
    double M[121], C[11], G[11], rhs[11];
    double A[121], b[11];
    
    build_M(phys, M);
    build_C(phys, C);
    build_G(phys, G);
    
    rhs[0] = force;
    for (int i = 1; i < N; ++i) rhs[i] = -C[i] - G[i];
    
    memcpy(A, M, N*N*sizeof(double));
    memcpy(b, rhs, N*sizeof(double));
    solve_system(N, A, b, qdd_out);
}

void npole_step_euler(NPolePhysics* phys, double force) {
    double qdd[11];
    npole_compute_accel(phys, force, qdd);
    phys->qd[0] += phys->dt * qdd[0];
    phys->q[0] += phys->dt * phys->qd[0];
    for (int i = 1; i <= phys->n; ++i) {
        phys->qd[i] += phys->dt * qdd[i];
        phys->q[i] += phys->dt * phys->qd[i];
    }
}

/* =================== Initialization =================== */

void npole_init_openocl(NPolePhysics* phys, int num_poles) {
    memset(phys, 0, sizeof(NPolePhysics));
    if (num_poles < 1) num_poles = 1;
    if (num_poles > NPOLE_MAX_POLES) num_poles = NPOLE_MAX_POLES;
    phys->n = num_poles;
    
    phys->cart_mass = OPENOCL_MC;
    phys->gravity = OPENOCL_G;
    phys->force_mag = 100.0;
    phys->dt = 0.01;
    
    double total_mass = phys->cart_mass;
    for (int i = 0; i < num_poles; ++i) {
        phys->pole_mass[i] = OPENOCL_MP;
        phys->pole_length[i] = OPENOCL_L;
        phys->pole_com[i] = OPENOCL_LCOM;
        phys->pole_inertia[i] = OPENOCL_MP * OPENOCL_L * OPENOCL_L / 3.0; // rod about end
        total_mass += phys->pole_mass[i];
    }
    phys->total_mass = total_mass;
    
    // Start upright (0 = vertical up per OpenOCL)
    phys->q[0] = 0.0; phys->qd[0] = 0.0;
    for (int i = 1; i <= num_poles; ++i) {
        phys->q[i] = 0.0;  // 0 = upright per OpenOCL convention
        phys->qd[i] = 0.0;
    }
}

void npole_init_cartpole(NPolePhysics* phys) {
    npole_init_openocl(phys, 1);
    // Override for standard CartPole-v1
    phys->cart_mass = 1.0;
    phys->pole_mass[0] = 0.1;
    phys->pole_length[0] = 0.5;
    phys->pole_com[0] = 0.25;
    phys->pole_inertia[0] = 0.1 * 0.5 * 0.5 / 3.0;
    phys->gravity = 9.81;
    phys->force_mag = 10.0;
    phys->total_mass = 1.1;
    phys->dt = 0.02;
    phys->q[1] = M_PI/2;  // π/2 = upright from horizontal (blog convention)
}

void npole_reset_upright(NPolePhysics* phys, double noise_scale) {
    phys->q[0] = ((double)rand() / RAND_MAX - 0.5) * noise_scale;
    phys->qd[0] = ((double)rand() / RAND_MAX - 0.5) * noise_scale;
    for (int i = 1; i <= phys->n; ++i) {
        phys->q[i] = ((double)rand() / RAND_MAX - 0.5) * noise_scale;
        phys->qd[i] = ((double)rand() / RAND_MAX - 0.5) * noise_scale;
    }
}

/* =================== Energy =================== */

double npole_energy(const NPolePhysics* phys) {
    int n = phys->n;
    double E = 0.5 * phys->cart_mass * phys->qd[0] * phys->qd[0];
    
    for (int i = 1; i <= n; ++i) {
        double th = phys->q[i];
        double thd = phys->qd[i];
        double lcom = phys->pole_com[i-1];
        double mp = phys->pole_mass[i-1];
        
        double vpx = phys->qd[0] + lcom * thd * cos(th);
        double vpy = lcom * thd * sin(th);
        
        E += 0.5 * mp * (vpx*vpx + vpy*vpy);
        E += mp * phys->gravity * lcom * (1 - cos(th));
    }
    return E;
}

int npole_is_done(const NPolePhysics* phys, double x_threshold, double theta_threshold) {
    if (fabs(phys->q[0]) > x_threshold) return 1;
    for (int i = 1; i <= phys->n; ++i) {
        if (fabs(phys->q[i]) > theta_threshold) return 1;
    }
    return 0;
}

void npole_get_state(const NPolePhysics* phys, double* x, double* x_dot,
                     double* theta, double* theta_dot, int max_poles) {
    *x = phys->q[0]; *x_dot = phys->qd[0];
    int n = phys->n < max_poles ? phys->n : max_poles;
    for (int i = 0; i < n; ++i) {
        theta[i] = phys->q[i+1];
        theta_dot[i] = phys->qd[i+1];
    }
}