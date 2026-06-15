#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define M_PI 3.14159265358979323846
#define CP_G 9.81
#define CP_MC 1.0
#define CP_U_MAX 80.0
#define CP_X_LIM 2.4
#define CP_DT 0.02
#define MAX_STEPS 500

typedef struct { int n, N; double *M, *C, *G, *rhs, *A, *b, *k1, *k2, *k3, *k4, *qt, *qdt, *th_sums, *thd_sums; } EOMContext;

static EOMContext* ctx_create(int max_n) {
    printf("ctx_create start\n"); fflush(stdout);
    EOMContext* c = (EOMContext*)calloc(1, sizeof(EOMContext));
    int N = max_n + 1;
    c->n = max_n; c->N = N;
    c->M = (double*)malloc(N * N * sizeof(double));
    c->C = (double*)malloc(N * sizeof(double));
    c->G = (double*)malloc(N * sizeof(double));
    c->rhs = (double*)malloc(N * sizeof(double));
    c->A = (double*)malloc(N * N * sizeof(double));
    c->b = (double*)malloc(N * sizeof(double));
    c->k1 = (double*)malloc(N * sizeof(double));
    c->k2 = (double*)malloc(N * sizeof(double));
    c->k3 = (double*)malloc(N * sizeof(double));
    c->k4 = (double*)malloc(N * sizeof(double));
    c->qt = (double*)malloc(N * sizeof(double));
    c->qdt = (double*)malloc(N * sizeof(double));
    c->th_sums = (double*)malloc((max_n + 2) * sizeof(double));
    c->thd_sums = (double*)malloc((max_n + 2) * sizeof(double));
    printf("ctx_create done\n"); fflush(stdout);
    return c;
}

typedef struct { int n; double *q, *qd, *m, *l, *lc, *I; } StateNA;
static StateNA* st_create(int n) {
    printf("st_create start\n"); fflush(stdout);
    StateNA* s = (StateNA*)calloc(1, sizeof(StateNA));
    s->n = n;
    s->q = (double*)calloc(n+1, sizeof(double));
    s->qd = (double*)calloc(n+1, sizeof(double));
    s->m = (double*)malloc(n * sizeof(double));
    s->l = (double*)malloc(n * sizeof(double));
    s->lc = (double*)malloc(n * sizeof(double));
    s->I = (double*)malloc(n * sizeof(double));
    for (int i = 0; i < n; ++i) {
        s->m[i] = 0.30 * pow(0.82, i+1);
        s->l[i] = 0.40 * pow(0.90, i+1);
        s->lc[i] = s->l[i] * 0.5;
        s->I[i] = s->m[i] * s->l[i] * s->l[i] / 3.0;
    }
    for (int i = 1; i <= n; ++i) s->q[i] = M_PI;
    printf("st_create done\n"); fflush(stdout);
    return s;
}

int main() {
    printf("MAIN START\n"); fflush(stdout);
    int poles = 7;
    StateNA* st = st_create(poles);
    printf("after st_create\n"); fflush(stdout);
    EOMContext* ctx = ctx_create(poles);
    printf("after ctx_create\n"); fflush(stdout);
    printf("DONE\n"); fflush(stdout);
    return 0;
}