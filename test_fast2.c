#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define M_PI 3.14159265358979323846
#define CP_G 9.81
#define CP_MC 1.0
#define CP_U_MAX 80.0
#define CP_X_LIM 2.4
#define CP_DT 0.02
#define MAX_STEPS 500

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
static void ctx_destroy(EOMContext* c) {
    free(c->M); free(c->C); free(c->G); free(c->rhs);
    free(c->A); free(c->b); free(c->k1); free(c->k2);
    free(c->k3); free(c->k4); free(c->qt); free(c->qdt);
    free(c->th_sums); free(c->thd_sums); free(c);
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
static void st_destroy(StateNA* s) { free(s->q); free(s->qd); free(s->m); free(s->l); free(s->lc); free(s->I); free(s); }

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

static double st_energy(const StateNA* s) {
    int n = s->n;
    double T = 0.5 * CP_MC * s->qd[0] * s->qd[0], V = 0.0;
    for (int i = 1; i <= n; ++i) {
        double x_com = s->q[0], y_com = 0.0;
        for (int j = 1; j <= i; ++j) {
            double th = 0; for(int k=1;k<=j;++k) th+=s->q[k];
            if (j<i) { x_com+=s->l[j-1]*sin(th); y_com-=s->l[j-1]*cos(th); }
            else { x_com+=s->lc[j-1]*sin(th); y_com-=s->lc[j-1]*cos(th); }
        }
        double vx = s->qd[0], vy = 0.0, thd_sum = 0.0;
        for (int j = 1; j <= i; ++j) {
            double th = 0; for(int k=1;k<=j;++k){ th+=s->q[k]; thd_sum+=s->qd[k]; }
            double factor = (j<i)?s->l[j-1]:s->lc[j-1];
            vx += factor * cos(th) * thd_sum;
            vy += factor * sin(th) * thd_sum;
        }
        T += 0.5 * s->m[i-1]*(vx*vx+vy*vy) + 0.5 * s->I[i-1]*thd_sum*thd_sum;
        V += s->m[i-1] * CP_G * y_com;
    }
    return T + V;
}

typedef struct { int n; double* K; } LQR;
static LQR* lqr_create(int n) { LQR* l=(LQR*)malloc(sizeof(LQR)); l->n=n; l->K=(double*)calloc(2*n+2,sizeof(double)); l->K[0]=10; for(int i=1;i<=n;++i)l->K[i]=150+i*10; l->K[n+1]=15; for(int i=1;i<=n;++i)l->K[n+1+i]=5+i*2; return l; }
static void lqr_destroy(LQR* l){ free(l->K); free(l); }
static double lqr_ctrl(const LQR* l, const StateNA* s) { double u=0; u-=l->K[0]*s->q[0]; for(int i=1;i<=l->n;++i)u-=l->K[i]*s->q[i]; u-=l->K[l->n+1]*s->qd[0]; for(int i=1;i<=l->n;++i)u-=l->K[l->n+1+i]*s->qd[i]; if(u>CP_U_MAX)u=CP_U_MAX; if(u<-CP_U_MAX)u=-CP_U_MAX; return u; }
static double swing_ctrl(const StateNA* s, double Et) { double E=st_energy(s), dE=E-Et, th1=s->q[1], thd1=s->qd[1], k=80; double u=-k*dE*cos(th1)*thd1+0.01*sin(th1); if(u>CP_U_MAX)u=CP_U_MAX; if(u<-CP_U_MAX)u=-CP_U_MAX; return u; }
static int near_upright(const StateNA* s, double thresh) { for(int i=1;i<=s->n;++i)if(fabs(s->q[i])>thresh)return 0; return fabs(s->q[0])<CP_X_LIM*0.5; }

int main() {
    int poles = 7;
    printf("=== %d-Pole Fast Test ===\n", poles);
    StateNA* st = st_create(poles); LQR* lqr = lqr_create(poles);
    double Et = 0.0; EOMContext* ctx = ctx_create(poles);
    int hold = 0, solved = 0;
    for (int step = 0; step < MAX_STEPS; ++step) {
        int use_lqr = near_upright(st, 0.08);
        double u = use_lqr ? lqr_ctrl(lqr, st) : swing_ctrl(st, Et);
        st_rk4(st, u, ctx);
        int up = 1; for(int i=1;i<=poles;++i)if(fabs(st->q[i])>0.05)up=0;
        if(up && fabs(st->q[0])<CP_X_LIM)hold++; else hold=0;
        if (step % 50 == 0) printf("%d: E=%.1f x=%.3f th1=%.3f u=%.1f %s hold=%d\n",step,st_energy(st),st->q[0],st->q[1],u,use_lqr?"LQR":"Swing",hold);
        if (hold>200) { printf("SOLVED at %d\n",step); solved=1; break; }
        if (fabs(st->q[0])>CP_X_LIM) { printf("FAILED: cart out of bounds at %d\n",step); break; }
    }
    st_destroy(st); lqr_destroy(lqr); ctx_destroy(ctx);
    printf("Result: %s\n", solved?"SOLVED":"FAILED");
    return 0;
}