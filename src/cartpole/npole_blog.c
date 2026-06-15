/*
 * N-Pole CartPole Physics  --  Blog-matched Lagrangian formulation
 * 
 * Reference: https://markus7800.github.io/blog/ML/cartpole.html
 * Extended to N poles using the blog's recursive formulation:
 * - p_{p2} = p_c + r * [cos(θ), sin(θ)] + (r2/2) * [cos(θ+θ2), sin(θ+θ2)]
 * - v_{p2} = v_c + r * [-sin(θ)*θ̇, cos(θ)*θ̇] + (r2/2) * [-sin(θ+θ2)*(θ̇+θ̇2), cos(θ+θ2)*(θ̇+θ̇2)]
 * 
 * Generalized coordinates: q = [x, θ_1, θ_2, ..., θ_N]
 * Each θ_i is angle from HORIZONTAL (upright = π/2)
 * Each pole i is a point mass at r_i/2 from its pivot
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define M_PI 3.14159265358979323846

/* Configuration */
#define MAX_POLES       10
#define DEFAULT_DT      0.01       /* Smaller dt for multi-pole stability */
#define GRAVITY         9.81
#define CART_MASS       1.0

/* Pole parameters (matching cartpole8: decreasing mass/length distally) */
#define POLE_MASS_BASE  0.30
#define POLE_MASS_DECAY 0.82
#define POLE_LEN_BASE   0.40
#define POLE_LEN_DECAY  0.90

/* Force and thresholds */
#define FORCE_MAG       80.0
#define X_THRESH        2.5
#define TH_THRESH       (M_PI / 2)   /* 90° from upright = π/2 */

/* State structure */
typedef struct {
    int n;                              /* number of poles */
    double x;                           /* cart position */
    double x_dot;                       /* cart velocity */
    double theta[MAX_POLES];            /* pole angles from horizontal */
    double theta_dot[MAX_POLES];        /* pole angular velocities */
    double m[MAX_POLES];                /* pole masses */
    double l[MAX_POLES];                /* pole lengths */
    double lc[MAX_POLES];               /* pole COM distances (l/2) */
} NPoleState;

/* Initialize pole parameters */
void npole_init_params(NPoleState* s, int n) {
    s->n = n;
    for (int i = 0; i < n; ++i) {
        s->m[i]  = POLE_MASS_BASE * pow(POLE_MASS_DECAY, i);
        s->l[i]  = POLE_LEN_BASE * pow(POLE_LEN_DECAY, i);
        s->lc[i] = s->l[i] * 0.5;       /* COM at midpoint */
    }
}

/* Reset to upright with small noise */
void npole_reset_upright(NPoleState* s, double noise) {
    s->x = ((double)rand() / RAND_MAX - 0.5) * noise;
    s->x_dot = ((double)rand() / RAND_MAX - 0.5) * noise;
    for (int i = 0; i < s->n; ++i) {
        s->theta[i] = M_PI/2 + ((double)rand() / RAND_MAX - 0.5) * noise;
        s->theta_dot[i] = ((double)rand() / RAND_MAX - 0.5) * noise;
    }
}

/* Reset to hanging down with small noise */
void npole_reset_down(NPoleState* s, double noise) {
    s->x = ((double)rand() / RAND_MAX - 0.5) * noise;
    s->x_dot = ((double)rand() / RAND_MAX - 0.5) * noise;
    for (int i = 0; i < s->n; ++i) {
        s->theta[i] = -M_PI/2 + ((double)rand() / RAND_MAX - 0.5) * noise;
        s->theta_dot[i] = ((double)rand() / RAND_MAX - 0.5) * noise;
    }
}

/* Compute cumulative angles and velocities for each pole segment */
void npole_compute_cumulative(const NPoleState* s, double* th_sum, double* thd_sum) {
    th_sum[0] = 0.0;
    thd_sum[0] = 0.0;
    for (int i = 0; i < s->n; ++i) {
        th_sum[i+1] = th_sum[i] + s->theta[i];
        thd_sum[i+1] = thd_sum[i] + s->theta_dot[i];
    }
}

/* Build mass matrix M and force vector F for [ẍ, θ̈_1, ..., θ̈_N]
 * M is (N+1) x (N+1)
 * Based on Lagrangian with point masses at lc_i from each pivot
 */
void npole_build_system(const NPoleState* s, double force, const double* th_sum, const double* thd_sum,
                        double* M, double* F) {
    int N = s->n;
    int sz = N + 1;
    
    /* Clear */
    for (int i = 0; i < sz * sz; ++i) M[i] = 0.0;
    for (int i = 0; i < sz; ++i) F[i] = 0.0;
    
    /* M[0][0] = m_cart + sum(m_i) */
    M[0] = CART_MASS;
    for (int i = 0; i < N; ++i) M[0] += s->m[i];
    
    /* M[0][j+1] = M[j+1][0] = sum over k>=j of m_k * lc_j * cos(θ_1+...+θ_j) for j<=k
     * Wait, need to be careful: the blog's formulation for double pole:
     * p_p2 = p_c + r * [cos(θ1), sin(θ1)] + (r2/2) * [cos(θ1+θ2), sin(θ1+θ2)]
     * So pole 1 COM depends on θ1 only
     * Pole 2 COM depends on θ1+θ2
     * 
     * General: pole k COM position uses sum of θ_1 to θ_k
     * The coupling M[0][j+1] = sum_{k=j}^{N-1} m_k * lc_{j} * cos(th_sum[j])
     * where lc_j is the COM distance of pole j
     */
    for (int j = 1; j <= N; ++j) {  /* j is 1-indexed pole number */
        double mij = 0.0;
        for (int k = j; k <= N; ++k) {  /* k >= j, poles k..N affected by θ_j */
            /* Pole k's COM: at distance lc_{k-1} from its pivot, angle = th_sum[k] */
            /* But the coupling depends on how θ_j affects pole k's position */
            /* For pole k >= j, its velocity has component from θ̇_j */
            double lc = s->lc[k-1];  /* COM of pole k */
            if (k == j) {
                /* Direct: pole j's COM depends on θ_j with distance lc_j */
                mij += s->m[k-1] * lc * cos(th_sum[j]);
            } else {
                /* Indirect: pole k's pivot moves due to θ_j, distance from cart to pivot of k is sum_{i=1}^{k-1} l_i */
                /* Actually, the blog's v_p2 shows pole 2 velocity has r * [-sin(θ1)*θ̇1, cos(θ1)*θ̇1] from pole 1's pivot
                 * So the COM of pole k is at sum_{i=1}^{k} l_i * [cos(sum θ_1..i), sin(...)] but wait
                 * Blog: p_p2 = p_c + r1*[cos(θ1), sin(θ1)] + (r2/2)*[cos(θ1+θ2), sin(θ1+θ2)]
                 * So pole 1 pivot is at p_c + r1*[cos(θ1), sin(θ1)]
                 * Pole 2 COM is at p_c + r1*[cos(θ1), sin(θ1)] + (r2/2)*[cos(θ1+θ2), sin(θ1+θ2)]
                 * 
                 * For general N poles with point masses at l_i/2 from each pivot:
                 * Pole i COM = p_c + sum_{j=1}^{i-1} l_j * [cos(th_sum[j]), sin(th_sum[j])] + lc_i * [cos(th_sum[i]), sin(th_sum[i])]
                 * 
                 * Velocity involves derivatives of all previous angles
                 */
                double dj = 0.0;
                /* Sum of lengths of poles 1 to k-1 (pivot positions) */
                for (int i = 0; i < j; ++i) {
                    dj += s->l[i];
                }
                /* Wait, this is getting complex. Let me use the blog's exact formulation.
                 * 
                 * Actually, simpler approach: use recursive Lagrangian or just compute M from kinetic energy.
                 * T = 1/2 m_c ẋ² + sum_i 1/2 m_i |v_i|²
                 * v_i = ẋ * [1, 0] + sum_{j=1}^i l_j * θ̇_j * [-sin(th_sum[j]), cos(th_sum[j])] (for pivot motion)
                 *       + lc_i * θ̇_i * [-sin(th_sum[i]), cos(th_sum[i])] (for COM relative to own pivot)
                 * 
                 * This is getting complicated. Let me match the blog exactly for N=1, N=2 first.
                 */
            }
        }
    }
}

/* Simpler: Compute accelerations using the recursive formulation from the blog
 * For N=1 (single pole):
 * M = [ mc+mp,     mp*lc*cos(θ) ]
 *     [ mp*lc*cos(θ), mp*lc²     ]
 * F = [ f + mp*lc*sin(θ)*θ̇² ]
 *     [ -mp*g*lc*cos(θ) ]
 * 
 * For N=2 (double pole, blog's equations):
 * Need to derive or use existing cartpole8 implementation
 */

/* Let's use the correct formulation from the blog for N poles.
 * 
 * Position of pole i COM:
 * p_i = p_c + sum_{j=1}^{i-1} l_j * [cos(th_sum[j]), sin(th_sum[j])] + lc_i * [cos(th_sum[i]), sin(th_sum[i])]
 * 
 * Velocity:
 * v_i = v_c + sum_{j=1}^{i-1} l_j * θ̇_j * [-sin(th_sum[j]), cos(th_sum[j])] + lc_i * θ̇_i * [-sin(th_sum[i]), cos(th_sum[i])]
 * 
 * But θ̇_j contributes to ALL poles k >= j
 * So v_i = ẋ * [1,0] + sum_{j=1}^i (j < i ? l_j : lc_i) * θ̇_j * [-sin(th_sum[j]), cos(th_sum[j])]
 * 
 * Kinetic energy T = 1/2 m_c ẋ² + sum_i 1/2 m_i |v_i|²
 * 
 * Mass matrix M_{ab} = d²T / dq̇_a dq̇_b
 * q = [x, θ_1, θ_2, ..., θ_N]
 * 
 * M[0][0] = mc + sum m_i
 * M[0][j] = M[j][0] = sum_{i=j}^{N} m_i * (j < i ? l_j : lc_i) * cos(th_sum[j])
 * M[j][k] = sum_{i=max(j,k)}^{N} m_i * (j < i ? l_j : lc_i) * (k < i ? l_k : lc_i) * cos(th_sum[j] - th_sum[k])
 * 
 * Forces from potential V = sum m_i * g * (p_i)_y = sum m_i * g * [sum_{j=1}^{i-1} l_j sin(th_sum[j]) + lc_i sin(th_sum[i])]
 * And Coriolis terms from d/dt(dT/dq̇) - dT/dq
 */

static inline int idx(int i, int j, int sz) { return i * sz + j; }

void npole_eom(const NPoleState* s, double force, double* qdd, double dt) {
    int N = s->n;
    int sz = N + 1;
    
    double th_sum[MAX_POLES + 1];
    double thd_sum[MAX_POLES + 1];
    npole_compute_cumulative(s, th_sum, thd_sum);
    
    /* Allocate on stack for small N */
    double M[121];  /* 11x11 max */
    double F[11];
    double A[121];
    double b[11];
    
    /* Clear */
    for (int i = 0; i < sz * sz; ++i) M[i] = 0.0;
    for (int i = 0; i < sz; ++i) F[i] = 0.0;
    
    /* ---- M[0][0] ---- */
    M[0] = CART_MASS;
    for (int i = 0; i < N; ++i) M[0] += s->m[i];
    
    /* ---- M[0][j] and M[j][0] ---- */
    for (int j = 1; j <= N; ++j) {  /* θ_j */
        double mij = 0.0;
        for (int i = j; i <= N; ++i) {  /* poles i >= j affected by θ_j */
            double d = (j < i) ? s->l[j-1] : s->lc[i-1];
            mij += s->m[i-1] * d * cos(th_sum[j]);
        }
        M[idx(0, j, sz)] = mij;
        M[idx(j, 0, sz)] = mij;
    }
    
    /* ---- M[j][k] for j,k >= 1 ---- */
    for (int j = 1; j <= N; ++j) {
        for (int k = j; k <= N; ++k) {
            double mij = 0.0;
            for (int i = (j > k ? j : k); i <= N; ++i) {  /* i >= max(j,k) */
                double dj = (j < i) ? s->l[j-1] : s->lc[i-1];
                double dk = (k < i) ? s->l[k-1] : s->lc[i-1];
                mij += s->m[i-1] * dj * dk * cos(th_sum[j] - th_sum[k]);
            }
            M[idx(j, k, sz)] = mij;
            M[idx(k, j, sz)] = mij;
        }
    }
    
    /* ---- Force vector F ---- */
    /* F[0] = external force + Coriolis (centrifugal) terms */
    F[0] = force;
    for (int i = 1; i <= N; ++i) {  /* pole i contributes to cart Coriolis */
        double sum = 0.0;
        for (int j = 1; j <= i; ++j) {
            /* Derivative of M[0][j] w.r.t. θ_k * θ̇_j * θ̇_k terms
             * Actually compute from kinetic energy derivatives
             */
        }
        /* Simpler: use Christoffel symbols or direct Coriolis formula
         * C_i = sum_{j,k} Γ^i_{jk} θ̇_j θ̇_k
         * For cart equation: sum_{j} sum_{k} m_k * d_j * θ̇_j * θ̇_k * sin(th_sum[j] - th_sum[k]) * cos(th_sum[j])
         * Let me derive properly...
         */
    }
    
    /* This is getting very complex. For now, let me use the existing
     * test_eom.c implementation which is already working, and just
     * fix the coordinate convention to match the blog (θ from horizontal).
     * 
     * The existing test_eom.c uses angles from VERTICAL (upright=π).
     * The blog uses angles from HORIZONTAL (upright=π/2).
     * So I can just adapt the working implementation.
     */
    
    (void)dt; (void)force;
    for (int i = 0; i < sz; ++i) qdd[i] = 0.0;
}

/* Placeholder - will be replaced by validated implementation */
void npole_step(NPoleState* s, double force, double dt) {
    double qdd[11];
    npole_eom(s, force, qdd, dt);
    
    /* Semi-implicit Euler */
    s->x_dot += dt * qdd[0];
    s->x += dt * s->x_dot;
    for (int i = 0; i < s->n; ++i) {
        s->theta_dot[i] += dt * qdd[i+1];
        s->theta[i] += dt * s->theta_dot[i];
        while (s->theta[i] > M_PI) s->theta[i] -= 2 * M_PI;
        while (s->theta[i] < -M_PI) s->theta[i] += 2 * M_PI;
    }
}

int main() {
    printf("N-pole stub - needs full Lagrangian implementation\n");
    return 0;
}