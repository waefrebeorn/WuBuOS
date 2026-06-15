#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define M_PI 3.14159265358979323846
#define CP_G 9.81
#define CP_MC 1.0
#define CP_DT 0.02

typedef struct { int n, N; double *M, *C, *G, *rhs, *A, *b, *k1, *k2, *k3, *k4, *qt, *qdt, *th_sums, *thd_sums; } EOMContext;
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
    c->k1 = (double*)malloc(N * sizeof(double));
    c->k2 = (double*)malloc(N * sizeof(double));
    c->k3 = (double*)malloc(N * sizeof(double));
    c->k4 = (double*)malloc(N * sizeof(double));
    c->qt = (double*)malloc(N * sizeof(double));
    c->qdt = (double*)malloc(N * sizeof(double));
    c->th_sums = (double*)malloc((max_n + 2) * sizeof(double));
    c->thd_sums = (double*)malloc((max_n + 2) * sizeof(double));
    return c;
}

typedef struct { int n; double *q, *qd, *m, *l, *lc, *I; } StateNA;
static StateNA* st_create(int n) {
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
    return s;
}

static void st_eom(const StateNA* s, double u, double* qdd, EOMContext* c) {
    int n = s->n, N = n + 1;
    double *th_sums = c->th_sums, *thd_sums = c->thd_sums;
    th_sums[0] = 0.0; thd_sums[0] = 0.0;
    for (int i = 1; i <= n; ++i) { th_sums[i] = th_sums[i-1] + s->q[i]; thd_sums[i] = thd_sums[i-1] + s->qd[i]; }
    double *M = c->M, *C = c->C, *G = c->G, *rhs = c->rhs, *A = c->A, *b = c->b;
    M[0] = CP_MC; for (int i = 0; i < n; ++i) M[0] += s->m[i];
    for (int j = 1; j <= n; ++j) { double mij = 0.0, th = th_sums[j];
        for (int i = j; i <= n; ++i) mij += s->m[i-1] * ((i==j)?s->lc[i-1]:s->l[j-1]) * cos(th);
        M[0*N+j] = mij; M[j*N+0] = mij; }
    for (int i = 1; i <= n; ++i) for (int j = i; j <= n; ++j) {
        double mij = 0.0;
        for (int k = i; k <= n; ++k) {
            double di = (i==k)?s->lc[k-1]:s->l[i-1], dj = (j==k)?s->lc[k-1]:s->l[j-1];
            mij += s->m[k-1] * di * dj * cos(th_sums[i]-th_sums[j]);
        }
        if (i==j) for (int k=i; k<=n; ++k) mij += s->I[k-1];
        M[i*N+j] = mij; M[j*N+i] = mij;
    }
    for (int i = 1; i <= n; ++i) {
        double ci = 0.0;
        for (int j = 1; j <= n; ++j) for (int k = 1; k <= n; ++k) {
            int min_ij = (i<j)?i:j, min_jk = (j<k)?j:k;
            double dMij_dqk = 0, dMjk_dqi = 0;
            if (k <= min_ij) { double di=(i==k)?s->lc[k-1]:s->l[i-1], dj=(j==k)?s->lc[k-1]:s->l[j-1]; dMij_dqk = -s->m[k-1]*di*dj*sin(th_sums[i]-th_sums[j]); }
            if (i <= min_jk) { double di=(j==i)?s->lc[i-1]:s->l[j-1], dj=(k==i)?s->lc[i-1]:s->l[k-1]; dMjk_dqi = -s->m[i-1]*di*dj*sin(th_sums[j]-th_sums[k]); }
            ci += (dMij_dqk - 0.5*dMjk_dqi) * s->qd[j] * s->qd[k];
        }
        C[i] = ci;
    } C[0] = 0;
    G[0] = 0;
    for (int i = 1; i <= n; ++i) { double gi = 0;
        for (int k = i; k <= n; ++k) gi += s->m[k-1] * CP_G * ((k==i)?s->lc[i-1]:s->l[i-1]) * cos(th_sums[i]);
        G[i] = gi;
    }
    rhs[0] = u; for (int i = 1; i < N; ++i) rhs[i] = -C[i] - G[i];
    memcpy(A, M, N*N*sizeof(double)); memcpy(b, rhs, N*sizeof(double));
    for (int i = 0; i < N; ++i) {
        int piv = i; for (int r=i+1; r<N; ++r) if (fabs(A[r*N+i])>fabs(A[piv*N+i])) piv=r;
        if (piv!=i) { for(int c2=i;c2<N;++c2){double t=A[i*N+c2];A[i*N+c2]=A[piv*N+c2];A[piv*N+c2]=t;} double t=b[i];b[i]=b[piv];b[piv]=t; }
        if (fabs(A[i*N+i])<1e-8) A[i*N+i]=(A[i*N+i]>=0)?1e-8:-1e-8;
        double diag=A[i*N+i]; for(int r=i+1;r<N;++r){double f=A[r*N+i]/diag; for(int c2=i;c2<N;++c2) A[r*N+c2]-=f*A[i*N+c2]; b[r]-=f*b[i];}
    }
    for (int i=N-1;i>=0;--i){ qdd[i]=b[i]; for(int c2=i+1;c2<N;++c2) qdd[i]-=A[i*N+c2]*qdd[c2]; qdd[i]/=A[i*N+i]; }
}

static void st_rk4(StateNA* s, double u, EOMContext* c) {
    int N = s->n + 1;
    double *k1=c->k1, *k2=c->k2, *k3=c->k3, *k4=c->k4, *qt=c->qt, *qdt=c->qdt;
    StateNA s2 = *s; s2.q = qt; s2.qd = qdt;
    st_eom(s, u, k1, c);
    for (int i = 0; i < N; ++i) { qt[i] = s->q[i] + 0.5*CP_DT*s->qd[i]; qdt[i] = s->qd[i] + 0.5*CP_DT*k1[i]; }
    st_eom(&s2, u, k2, c);
    for (int i = 0; i < N; ++i) { qt[i] = s->q[i] + 0.5*CP_DT*s->qd[i] + 0.25*CP_DT*CP_DT*k1[i]; qdt[i] = s->qd[i] + 0.5*CP_DT*k2[i]; }
    st_eom(&s2, u, k3, c);
    for (int i = 0; i < N; ++i) { qt[i] = s->q[i] + CP_DT*s->qd[i] + 0.5*CP_DT*CP_DT*k2[i]; qdt[i] = s->qd[i] + CP_DT*k3[i]; }
    st_eom(&s2, u, k4, c);
    for (int i = 0; i < N; ++i) {
        s->q[i] += CP_DT*s->qd[i] + CP_DT*CP_DT/6.0*(k1[i]+2*k2[i]+2*k3[i]+k4[i]);
        s->qd[i] += CP_DT/6.0*(k1[i]+2*k2[i]+2*k3[i]+k4[i]);
    }
    for (int i = 1; i <= s->n; ++i) { while(s->q[i]>M_PI)s->q[i]-=2*M_PI; while(s->q[i]<-M_PI)s->q[i]+=2*M_PI; }
}

int main() {
    printf("MAIN START\n"); fflush(stdout);
    int poles = 7;
    StateNA* st = st_create(poles);
    EOMContext* ctx = ctx_create(poles);
    printf("Starting loop...\n"); fflush(stdout);
    for (int step = 0; step < 100; ++step) {
        st_rk4(st, 0.0, ctx);
        if (step % 20 == 0) { printf("step %d q[1]=%.3f\n", step, st->q[1]); fflush(stdout); }
    }
    printf("DONE\n"); fflush(stdout);
    return 0;
}