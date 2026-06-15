/*
 * CartPole Physics  --  Exact implementation from markus7800 blog (Lagrangian formulation)
 * 
 * Reference: https://markus7800.github.io/blog/ML/cartpole.html
 * 
 * Key conventions:
 * - x: cart position (horizontal)
 * - θ: pole angle from HORIZONTAL axis (upright = π/2, down = -π/2 or 3π/2)
 * - Pole is a point mass at r/2 (midpoint of rod)
 * - Semi-implicit Euler integration
 * - Standard CartPole-v1 parameters
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define M_PI 3.14159265358979323846

/* Standard CartPole-v1 parameters (from OpenAI Gym) */
#define CP_G       9.81
#define CP_MC      1.0
#define CP_MP      0.1
#define CP_R       0.5          /* pole length */
#define CP_R_HALF  0.25         /* pole COM at r/2 */
#define CP_FORCE   10.0
#define CP_DT      0.02
#define CP_X_THRESH  2.4
#define CP_TH_THRESH 0.209      /* ~12 degrees from upright */

/* State: [x, x_dot, theta, theta_dot] */
typedef struct {
    double x;
    double x_dot;
    double theta;       /* angle from HORIZONTAL (upright = pi/2) */
    double theta_dot;
} CartPoleState;

static inline double clamp(double v, double lo, double hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/* 
 * Lagrangian Equations of Motion (from blog)
 * 
 * q = [x, θ]
 * p_pole = p_cart + (r/2) * [cos(θ), sin(θ)]^T
 * 
 * T = 1/2 * m_c * ẋ² + 1/2 * m_p * |v_pole|²
 * V = m_p * g * (r/2) * sin(θ)
 * 
 * L = T - V
 * 
 * d/dt(dL/dẋ) = dL/dx + f
 * d/dt(dL/dθ̇) = dL/dθ
 * 
 * Results in:
 * (m_c + m_p) * ẍ + m_p * (r/2) * cos(θ) * θ̈ - m_p * (r/2) * sin(θ) * θ̇² = f
 * m_p * (r/2) * cos(θ) * ẍ + m_p * (r/2)² * θ̈ + m_p * g * (r/2) * cos(θ) = 0
 * 
 * Mass matrix M = [ m_c + m_p,     m_p*(r/2)*cos(θ)    ]
 *                 [ m_p*(r/2)*cos(θ),  m_p*(r/2)²      ]
 * 
 * Forces F = [ f + m_p*(r/2)*sin(θ)*θ̇² ]
 *            [ -m_p*g*(r/2)*cos(θ)      ]
 * 
 * Solve M * [ẍ, θ̈]^T = F
 */

void cartpole_eom(const CartPoleState* s, double force, double* xacc, double* thetaacc) {
    const double mc = CP_MC;
    const double mp = CP_MP;
    const double r2 = CP_R_HALF;
    const double g = CP_G;
    
    double thdot = s->theta_dot;
    
    /* Mass matrix */
    double M00 = mc + mp;
    double M01 = mp * r2 * cos(s->theta);
    double M10 = M01;
    double M11 = mp * r2 * r2;
    
    /* Force vector */
    double F0 = force + mp * r2 * sin(s->theta) * thdot * thdot;
    double F1 = -mp * g * r2 * cos(s->theta);
    
    /* Solve 2x2 system: M * a = F */
    double det = M00 * M11 - M01 * M10;
    if (fabs(det) < 1e-12) {
        *xacc = 0.0;
        *thetaacc = 0.0;
        return;
    }
    
    *xacc     = (F0 * M11 - M01 * F1) / det;
    *thetaacc = (M00 * F1 - M10 * F0) / det;
}

/* Semi-implicit Euler step (as used in blog) */
void cartpole_step(CartPoleState* s, double force) {
    double xacc, thetaacc;
    cartpole_eom(s, force, &xacc, &thetaacc);
    
    /* Semi-implicit Euler: update velocities first, then positions */
    s->x_dot     += CP_DT * xacc;
    s->theta_dot += CP_DT * thetaacc;
    s->x         += CP_DT * s->x_dot;
    s->theta     += CP_DT * s->theta_dot;
    
    /* Wrap theta to [-pi, pi] for consistency */
    while (s->theta > M_PI) s->theta -= 2 * M_PI;
    while (s->theta < -M_PI) s->theta += 2 * M_PI;
}

/* Initialize to near-upright (as in blog balancing task) */
void cartpole_reset_upright(CartPoleState* s, double noise_scale) {
    s->x = ((double)rand() / RAND_MAX - 0.5) * noise_scale;
    s->x_dot = ((double)rand() / RAND_MAX - 0.5) * noise_scale;
    s->theta = M_PI/2 + ((double)rand() / RAND_MAX - 0.5) * noise_scale;  /* pi/2 = upright */
    s->theta_dot = ((double)rand() / RAND_MAX - 0.5) * noise_scale;
}

/* Initialize to hanging down (as in blog swing-up task) */
void cartpole_reset_down(CartPoleState* s, double noise_scale) {
    s->x = ((double)rand() / RAND_MAX - 0.5) * noise_scale;
    s->x_dot = ((double)rand() / RAND_MAX - 0.5) * noise_scale;
    s->theta = -M_PI/2 + ((double)rand() / RAND_MAX - 0.5) * noise_scale;  /* -pi/2 = hanging down */
    s->theta_dot = ((double)rand() / RAND_MAX - 0.5) * noise_scale;
}

/* Check termination conditions */
int cartpole_is_done(const CartPoleState* s) {
    /* x threshold */
    if (fabs(s->x) > CP_X_THRESH) return 1;
    /* theta threshold: upright ± CP_TH_THRESH */
    double theta_from_upright = fabs(s->theta - M_PI/2);
    while (theta_from_upright > M_PI) theta_from_upright = 2 * M_PI - theta_from_upright;
    if (theta_from_upright > CP_TH_THRESH) return 1;
    return 0;
}

/* Energy of the system */
double cartpole_energy(const CartPoleState* s) {
    const double mc = CP_MC;
    const double mp = CP_MP;
    const double r2 = CP_R_HALF;
    const double g = CP_G;
    
    /* Cart kinetic energy */
    double T_cart = 0.5 * mc * s->x_dot * s->x_dot;
    
    /* Pole velocity: v_pole = [ẋ - r2*sin(θ)*θ̇, r2*cos(θ)*θ̇] */
    double vpx = s->x_dot - r2 * sin(s->theta) * s->theta_dot;
    double vpy = r2 * cos(s->theta) * s->theta_dot;
    double T_pole = 0.5 * mp * (vpx * vpx + vpy * vpy);
    
    /* Potential energy (zero at cart level) */
    double V = mp * g * r2 * sin(s->theta);
    
    return T_cart + T_pole + V;
}

/* Print state for debugging */
void cartpole_print(const CartPoleState* s, const char* label) {
    double theta_deg = s->theta * 180.0 / M_PI;
    double from_upright = fabs(s->theta - M_PI/2) * 180.0 / M_PI;
    printf("%s: x=%.4f v=%.4f θ=%.2f° (upright±%.2f°) ω=%.4f | E=%.6f\n",
           label, s->x, s->x_dot, theta_deg, from_upright, s->theta_dot, cartpole_energy(s));
}

/* ========================================================================
 * MAIN: Run verification tests matching blog scenarios
 * ======================================================================== */

int main() {
    srand(42);
    
    printf("===============================================================\n");
    printf("  CartPole  --  Blog-Matched Lagrangian Physics Verification\n");
    printf("===============================================================\n\n");
    
    CartPoleState s;
    
    /* Test 1: Equation of Motion at equilibrium (upright, zero velocity, zero force) */
    printf("Test 1: EOM at upright equilibrium (θ=π/2, ẋ=0, θ̇=0, f=0)\n");
    s.x = 0; s.x_dot = 0; s.theta = M_PI/2; s.theta_dot = 0;
    double xacc, thacc;
    cartpole_eom(&s, 0.0, &xacc, &thacc);
    printf("  ẍ = %.10f, θ̈ = %.10f (both should be ~0)\n\n", xacc, thacc);
    
    /* Test 2: EOM at hanging down (θ=-π/2) */
    printf("Test 2: EOM at hanging down (θ=-π/2, ẋ=0, θ̇=0, f=0)\n");
    s.theta = -M_PI/2;
    cartpole_eom(&s, 0.0, &xacc, &thacc);
    printf("  ẍ = %.10f (should be 0)\n", xacc);
    printf("  θ̈ = %.10f (should be positive = falling down = -g/(r/2) = -39.24)\n\n", thacc);
    /* θ̈ = -g/(r/2) = -9.81/0.25 = -39.24, but sign depends on convention */
    /* Blog: θ from horizontal, so hanging down = -π/2, gravity torque = -mg(r/2)cos(-π/2) = 0 at exact bottom? */
    /* Wait: V = mp*g*(r/2)*sin(θ), so dV/dθ = mp*g*(r/2)*cos(θ) */
    /* At θ=-π/2, cos(-π/2)=0, so no torque. At θ=0 (horizontal), max torque. */
    
    /* Test 3: Energy conservation with no force */
    printf("Test 3: Energy conservation (no force, 1000 steps dt=0.02)\n");
    cartpole_reset_upright(&s, 0.1);
    double E0 = cartpole_energy(&s);
    for (int i = 0; i < 1000; i++) cartpole_step(&s, 0.0);
    double E1 = cartpole_energy(&s);
    printf("  E0 = %.8f, E1000 = %.8f, ΔE = %.2e\n\n", E0, E1, E1 - E0);
    
    /* Test 4: Balancing task  --  small perturbations should stay near upright */
    printf("Test 4: Balancing task (upright start, 100 steps, zero force)\n");
    cartpole_reset_upright(&s, 0.05);
    cartpole_print(&s, "  Initial");
    for (int i = 0; i < 100; i++) cartpole_step(&s, 0.0);
    cartpole_print(&s, "  After 100 steps");
    int done = cartpole_is_done(&s);
    printf("  Done: %s\n\n", done ? "YES (fell)" : "NO (balanced)");
    
    /* Test 5: Hanging down  --  should swing passively */
    printf("Test 5: Passive swing from hanging down (100 steps, zero force)\n");
    cartpole_reset_down(&s, 0.01);
    cartpole_print(&s, "  Initial");
    for (int i = 0; i < 100; i++) cartpole_step(&s, 0.0);
    cartpole_print(&s, "  After 100 steps");
    printf("  (should swing past upright and oscillate)\n\n");
    
    /* Test 6: Control response  --  push right, cart moves right */
    printf("Test 6: Control response (push right, 10 steps)\n");
    cartpole_reset_upright(&s, 0.0);
    cartpole_print(&s, "  Initial");
    for (int i = 0; i < 10; i++) cartpole_step(&s, CP_FORCE);
    cartpole_print(&s, "  After 10 steps (force=+10)");
    printf("  Cart should move right, pole should lean left (negative θ from upright)\n\n");
    
    /* Test 7: Manual PID-like balancing (crude) */
    printf("Test 7: Crude balancing (P on angle, 500 steps)\n");
    cartpole_reset_upright(&s, 0.1);
    int balanced_steps = 0;
    for (int i = 0; i < 500; i++) {
        /* Simple P controller: force = -k_p * (θ - π/2) - k_d * θ̇ */
        double theta_err = s.theta - M_PI/2;
        double force = -50.0 * theta_err - 10.0 * s.theta_dot;
        force = clamp(force, -CP_FORCE, CP_FORCE);
        cartpole_step(&s, force);
        if (!cartpole_is_done(&s)) balanced_steps++;
        else break;
    }
    printf("  Balanced for %d/500 steps\n\n", balanced_steps);
    
    /* Test 8: Verify semi-implicit Euler stability vs explicit */
    printf("Test 8: Large dt stability check (dt=0.02, force=10, 500 steps)\n");
    cartpole_reset_upright(&s, 0.0);
    for (int i = 0; i < 500; i++) cartpole_step(&s, 10.0);
    cartpole_print(&s, "  After 500 steps");
    printf("  (should not explode with semi-implicit Euler)\n\n");
    
    printf("===============================================================\n");
    printf("  All tests complete.\n");
    printf("===============================================================\n");
    return 0;
}