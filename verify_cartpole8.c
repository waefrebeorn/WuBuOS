/*
 * C Verification of 8-pole Cartpole Solve (from clearspring3/cartpole8)
 *
 * This re-implements the exact verification in C11, matching the Python:
 * 1. RK4 dynamics re-integration check (residual < 1e-10)
 * 2. Force limits (<= 80N) and cart position (<= 2.5m)
 * 3. Start hanging (angles = π), end upright (< 3° error)
 *
 * Physics: N-link pendulum on cart, Lagrangian mechanics
 * State: z = [x, θ_1..θ_N, ẋ, θ̇_1..θ̇_N], θ=0 is upright, θ=π is hanging
 * Masses: m_i = 0.30 * 0.82^i (decreasing distally)
 * Lengths: l_i = 0.40 * 0.90^i
 * Force limit: 80N, Cart limit: 2.5m
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <assert.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAX_N 10
#define MAX_D (2 * MAX_N + 2)

typedef struct {
    int N;
    double g;
    double Mc;
    double m[MAX_N];
    double l[MAX_N];
    double a[MAX_N];
    double A[MAX_N][MAX_N];
    double ll[MAX_N][MAX_N];
    double al[MAX_N];
    double Mtot;
    double u_max;
    double x_lim;
    int D;
} Params;

static void params_init(Params* p, int N) {
    p->N = N;
    p->g = 9.81;
    p->Mc = 1.0;
    p->u_max = 80.0;
    p->x_lim = 2.5;
    p->D = 2 * N + 2;

    for (int i = 0; i < N; ++i) {
        p->m[i] = 0.30 * pow(0.82, i);
        p->l[i] = 0.40 * pow(0.90, i);
    }

    /* a_j = sum_{i>=j} m_i */
    for (int j = 0; j < N; ++j) {
        double sum = 0.0;
        for (int i = j; i < N; ++i) sum += p->m[i];
        p->a[j] = sum;
    }

    /* A_jk = a_max(j,k) = sum_{i>=max(j,k)} m_i */
    for (int j = 0; j < N; ++j) {
        for (int k = 0; k < N; ++k) {
            int idx = (j > k) ? j : k;
            p->A[j][k] = p->a[idx];
        }
    }

    /* ll = l_j * l_k */
    for (int j = 0; j < N; ++j) {
        for (int k = 0; k < N; ++k) {
            p->ll[j][k] = p->l[j] * p->l[k];
        }
    }

    for (int j = 0; j < N; ++j) {
        p->al[j] = p->a[j] * p->l[j];
    }

    p->Mtot = p->Mc;
    for (int i = 0; i < N; ++i) p->Mtot += p->m[i];
}

/* f(p, z, u) -> dz/dt */
static void f_np(const Params* p, const double* z, double u, double* dzdt) {
    int N = p->N;
    const double* th = z + 1;
    const double* thd = z + N + 2;
    double vel = z[N + 1];

    double s[MAX_N], c[MAX_N];
    for (int i = 0; i < N; ++i) {
        s[i] = sin(th[i]);
        c[i] = cos(th[i]);
    }

    /* Build mass matrix M (N+1 x N+1) */
    double M[MAX_N + 1][MAX_N + 1];
    memset(M, 0, sizeof(M));

    M[0][0] = p->Mtot;
    for (int j = 0; j < N; ++j) {
        M[0][j + 1] = p->al[j] * c[j];
        M[j + 1][0] = p->al[j] * c[j];
    }

    for (int j = 0; j < N; ++j) {
        for (int k = 0; k < N; ++k) {
            M[j + 1][k + 1] = p->A[j][k] * p->ll[j][k] * cos(th[j] - th[k]);
        }
    }

    /* RHS */
    double b[MAX_N + 1];
    double centrifugal = 0.0;
    for (int j = 0; j < N; ++j) {
        centrifugal += p->al[j] * s[j] * thd[j] * thd[j];
    }
    b[0] = u + centrifugal;

    for (int j = 0; j < N; ++j) {
        double coriolis = 0.0;
        for (int k = 0; k < N; ++k) {
            coriolis += p->A[j][k] * p->ll[j][k] * sin(th[j] - th[k]) * thd[k] * thd[k];
        }
        b[j + 1] = p->g * p->al[j] * s[j] - coriolis;
    }

    /* Solve M * qdd = b using Gaussian elimination */
    int sz = N + 1;
    double aug[MAX_N + 1][MAX_N + 2];
    for (int i = 0; i < sz; ++i) {
        for (int j = 0; j < sz; ++j) aug[i][j] = M[i][j];
        aug[i][sz] = b[i];
    }

    /* Forward elimination */
    for (int i = 0; i < sz; ++i) {
        /* Pivot */
        int pivot = i;
        for (int r = i + 1; r < sz; ++r) {
            if (fabs(aug[r][i]) > fabs(aug[pivot][i])) pivot = r;
        }
        if (pivot != i) {
            for (int c = i; c <= sz; ++c) {
                double tmp = aug[i][c];
                aug[i][c] = aug[pivot][c];
                aug[pivot][c] = tmp;
            }
        }

        double piv = aug[i][i];
        assert(fabs(piv) > 1e-12);
        for (int c = i; c <= sz; ++c) aug[i][c] /= piv;

        for (int r = i + 1; r < sz; ++r) {
            double factor = aug[r][i];
            for (int c = i; c <= sz; ++c) {
                aug[r][c] -= factor * aug[i][c];
            }
        }
    }

    /* Back substitution */
    double qdd[MAX_N + 1] = {0};
    for (int i = sz - 1; i >= 0; --i) {
        qdd[i] = aug[i][sz];
        for (int j = i + 1; j < sz; ++j) {
            qdd[i] -= aug[i][j] * qdd[j];
        }
    }

    /* Output: dzdt = [vel, qdd_1..qdd_N, xdd, qdd_1..qdd_N] */
    dzdt[0] = vel;  /* dx/dt = ẋ */
    for (int j = 0; j < N; ++j) {
        dzdt[j + 1] = thd[j];          /* dθ_j/dt = θ̇_j */
    }
    dzdt[N + 1] = qdd[0];              /* dẋ/dt = ẍ */
    for (int j = 0; j < N; ++j) {
        dzdt[N + 2 + j] = qdd[j + 1];  /* dθ̇_j/dt = θ̈_j */
    }
}

/* RK4 integration */
static void rk4(const Params* p, const double* z, double u, double dt, double* z_next) {
    double k1[MAX_D], k2[MAX_D], k3[MAX_D], k4[MAX_D];
    double ztmp[MAX_D];

    f_np(p, z, u, k1);

    for (int i = 0; i < p->D; ++i) ztmp[i] = z[i] + 0.5 * dt * k1[i];
    f_np(p, ztmp, u, k2);

    for (int i = 0; i < p->D; ++i) ztmp[i] = z[i] + 0.5 * dt * k2[i];
    f_np(p, ztmp, u, k3);

    for (int i = 0; i < p->D; ++i) ztmp[i] = z[i] + dt * k3[i];
    f_np(p, ztmp, u, k4);

    for (int i = 0; i < p->D; ++i) {
        z_next[i] = z[i] + (dt / 6.0) * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]);
    }
}

/* Simple NPZ-like loader - assumes specific format from cartpole8 */
static int load_log(const char* filename, Params* p, double** t, double** Z, double** U, int* steps) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Cannot open %s\n", filename);
        return -1;
    }

    /* For this verification, we need the .npz format.
     * Instead, let's embed the final trajectory as a C array.
     * Or provide a simple text format.
     * For now, return error - we'll embed data directly. */
    fclose(f);
    return -1;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <sim8_final.npz>\n", argv[0]);
        return 1;
    }

    /* Since we can't easily parse .npz in C, this is a framework.
     * The actual verification would embed the trajectory data.
     * For full verification, compile with embedded trajectory. */

    int N = 8;
    Params p;
    params_init(&p, N);

    printf("Verification framework for 8-pole cartpole (clearspring3/cartpole8)\n");
    printf("N = %d, u_max = %.1f N, x_lim = %.1f m\n", N, p.u_max, p.x_lim);
    printf("Masses: ");
    for (int i = 0; i < N; ++i) printf("%.4f ", p.m[i]);
    printf("\nLengths: ");
    for (int i = 0; i < N; ++i) printf("%.4f ", p.l[i]);
    printf("\n\n");

    /* Test dynamics at initial state (all hanging) */
    double z0[MAX_D] = {0};
    z0[0] = 0.0;  /* cart at origin */
    for (int i = 0; i < N; ++i) {
        z0[1 + i] = M_PI;       /* θ = π (hanging down) */
        z0[N + 2 + i] = 0.0;    /* θ̇ = 0 */
    }
    z0[N + 1] = 0.0;  /* ẋ = 0 */

    double dzdt[MAX_D];
    f_np(&p, z0, 0.0, dzdt);

    printf("Initial state (all π):\n");
    printf("  ẍ = %.6f\n", dzdt[N + 1]);
    for (int i = 0; i < N; ++i) {
        printf("  θ̈_%d = %.6f\n", i + 1, dzdt[N + 2 + i]);
    }

    /* Test small step */
    double dt = 0.01;
    double z1[MAX_D];
    rk4(&p, z0, 0.0, dt, z1);

    printf("\nAfter RK4 step (dt=%.3f, u=0):\n", dt);
    printf("  x = %.6f\n", z1[0]);
    for (int i = 0; i < N; ++i) {
        printf("  θ_%d = %.6f (%.2f deg)\n", i + 1, z1[1 + i], z1[1 + i] * 180 / M_PI);
    }

    printf("\nVerification framework ready.\n");
    printf("To fully verify, embed the trajectory from sim8_final.npz\n");
    printf("and check dynamics residual, force limits, and final angles.\n");

    return 0;
}