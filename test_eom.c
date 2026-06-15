#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define M_PI 3.14159265358979323846
#define CP_G 9.81
#define CP_MC 1.0

typedef struct {
    int n, N;
    double *M, *C, *G, *rhs, *A, *b;
    double *th_sums, *thd_sums;
} EOMContext;

static EOMContext* ctx_create(int max_n) {
    EOMContext* c = (EOMContext*)calloc(1, sizeof(EOMContext));
    int N = max_n + 1;
    c->n = max_n; c->N = N;
    c->M = (double*)malloc(N * N * sizeof(double));
    c->C = (double*)malloc(N * sizeof(double));
    c->G = (double*)malloc(N * sizeof(double));
    c->rhs = (double*)malloc(N * sizeof(double));
    c->A = (double*)malloc(N * N * sizeof(double));
    c->b = (double*)malloc(N * sizeof(double));
    c->th_sums = (double*)malloc((max_n + 2) * sizeof(double));
    c->thd_sums = (double*)malloc((max_n + 2) * sizeof(double));
    return c;
}

typedef struct {
    int n;
    double* q;  double* qd;
    double* m;  double* l;  double* lc;  double* I;
} StateNA;

static StateNA* st_create(int n) {
    StateNA* s = (StateNA*)calloc(1, sizeof(StateNA));
    s->n = n;
    s->q  = (double*)calloc(n+1, sizeof(double));
    s->qd = (double*)calloc(n+1, sizeof(double));
    s->m  = (double*)malloc(n * sizeof(double));
    s->l  = (double*)malloc(n * sizeof(double));
    s->lc = (double*)malloc(n * sizeof(double));
    s->I  = (double*)malloc(n * sizeof(double));
    for (int i = 0; i < n; ++i) {
        s->m[i]  = 0.30 * pow(0.82, i+1);
        s->l[i]  = 0.40 * pow(0.90, i+1);
        s->lc[i] = s->l[i] * 0.5;
        s->I[i]  = s->m[i] * s->l[i] * s->l[i] / 3.0;
    }
    for (int i = 1; i <= n; ++i) s->q[i] = M_PI;
    return s;
}

static void st_eom(const StateNA* s, double u, double* qdd, EOMContext* c) {
    int n = s->n, N = n + 1;
    double* M = c->M; double* C = c->C; double* G = c->G;
    double* rhs = c->rhs; double* A = c->A; double* b = c->b;
    double* th_sums = c->th_sums; double* thd_sums = c->thd_sums;

    th_sums[0] = 0.0; thd_sums[0] = 0.0;
    for (int i = 1; i <= n; ++i) {
        th_sums[i] = th_sums[i-1] + s->q[i];
        thd_sums[i] = thd_sums[i-1] + s->qd[i];
    }

    M[0] = CP_MC;
    for (int i = 0; i < n; ++i) M[0] += s->m[i];

    for (int j = 1; j <= n; ++j) {
        double mij = 0.0;
        for (int i = j; i <= n; ++i) {
            double th = th_sums[j];
            if (i == j) mij += s->m[i-1] * s->lc[i-1] * cos(th);
            else mij += s->m[i-1] * s->l[j-1] * cos(th);
        }
        M[0*N + j] = mij;
        M[j*N + 0] = mij;
    }

    for (int i = 1; i <= n; ++i) {
        for (int j = i; j <= n; ++j) {
            double mij = 0.0;
            int min_ij = i;
            for (int k = min_ij; k <= n; ++k) {
                double th_i = th_sums[i], th_j = th_sums[j];
                double di = (i == k) ? s->lc[k-1] : s->l[i-1];
                double dj = (j == k) ? s->lc[k-1] : s->l[j-1];
                mij += s->m[k-1] * di * dj * cos(th_i - th_j);
            }
            if (i == j) {
                for (int k = i; k <= n; ++k) mij += s->I[k-1];
            }
            M[i*N + j] = mij;
            M[j*N + i] = mij;
        }
    }

    for (int i = 1; i <= n; ++i) {
        double ci = 0.0;
        for (int j = 1; j <= n; ++j) {
            for (int k = 1; k <= n; ++k) {
                double dMij_dqk = 0.0, dMjk_dqi = 0.0;
                int min_ij = (i < j) ? i : j;
                int min_jk = (j < k) ? j : k;
                if (k <= min_ij) {
                    double th_i = th_sums[i], th_j = th_sums[j];
                    double di = (i == k) ? s->lc[k-1] : s->l[i-1];
                    double dj = (j == k) ? s->lc[k-1] : s->l[j-1];
                    dMij_dqk = -s->m[k-1] * di * dj * sin(th_i - th_j);
                }
                if (i <= min_jk) {
                    double th_j2 = th_sums[j], th_k2 = th_sums[k];
                    double di2 = (j == i) ? s->lc[i-1] : s->l[j-1];
                    double dj2 = (k == i) ? s->lc[i-1] : s->l[k-1];
                    dMjk_dqi = -s->m[i-1] * di2 * dj2 * sin(th_j2 - th_k2);
                }
                double christoffel = dMij_dqk - 0.5 * dMjk_dqi;
                ci += christoffel * s->qd[j] * s->qd[k];
            }
        }
        C[i] = ci;
    }
    C[0] = 0.0;

    G[0] = 0.0;
    for (int i = 1; i <= n; ++i) {
        double gi = 0.0;
        for (int k = i; k <= n; ++k) {
            double th = th_sums[i];
            double l = (k == i) ? s->lc[i-1] : s->l[i-1];
            gi += s->m[k-1] * CP_G * l * cos(th);
        }
        G[i] = gi;
    }

    rhs[0] = u;
    for (int i = 1; i < N; ++i) rhs[i] = -C[i] - G[i];

    memcpy(A, M, N*N*sizeof(double));
    memcpy(b, rhs, N*sizeof(double));

    for (int i = 0; i < N; ++i) {
        int piv = i;
        for (int r = i+1; r < N; ++r)
            if (fabs(A[r*N+i]) > fabs(A[piv*N+i])) piv = r;
        if (piv != i) {
            for (int c2 = i; c2 < N; ++c2) { double t = A[i*N+c2]; A[i*N+c2] = A[piv*N+c2]; A[piv*N+c2] = t; }
            double t = b[i]; b[i] = b[piv]; b[piv] = t; }
        if (fabs(A[i*N+i]) < 1e-8) A[i*N+i] = (A[i*N+i] >= 0) ? 1e-8 : -1e-8;
        double diag = A[i*N+i];
        for (int r = i+1; r < N; ++r) {
            double f = A[r*N+i]/diag;
            for (int c2 = i; c2 < N; ++c2) A[r*N+c2] -= f*A[i*N+c2];
            b[r] -= f*b[i];
        }
    }
    for (int i = N-1; i >= 0; --i) {
        qdd[i] = b[i];
        for (int c2 = i+1; c2 < N; ++c2) qdd[i] -= A[i*N+c2]*qdd[c2];
        qdd[i] /= A[i*N+i];
    }
}

int main() {
    int n = 7;
    StateNA* st = st_create(n);
    EOMContext* ctx = ctx_create(n);
    double qdd[8];

    printf("Created state and context\n");
    fflush(stdout);

    for (int iter = 0; iter < 5; ++iter) {
        printf("Iter %d: calling st_eom...\n", iter);
        fflush(stdout);
        st_eom(st, 0.0, qdd, ctx);
        printf("Iter %d: qdd = ", iter);
        for (int i = 0; i <= n; ++i) printf("%.3f ", qdd[i]);
        printf("\n");
        fflush(stdout);
    }

    printf("Done\n");
    return 0;
}