/*
 * bear_cartpole_multipole.c  --  Multi-pole CartPole solver (7-20 poles)
 * Model-based: Energy swing-up + LQR balance (from cartpole8 reference)
 * Pure C11, no RL - first principles control
 */

#define _POSIX_C_SOURCE 200809L
#include "bear_arena.h"
#include "bear_env.h"
#include "wubu_math.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <alloca.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_STEPS 500
#define VIDEO_FPS 30
#define VIDEO_W 800
#define VIDEO_H 600

#define CP8_G 9.81
#define CP8_MC 1.0
#define CP8_U_MAX 80.0
#define CP8_X_LIM 2.5
#define CP8_DT 0.01

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef unsigned char uchar;

typedef struct { int n; double* q; double* qd; double* m; double* l; } State;

static State* st_create(int n) {
    State* s = (State*)malloc(sizeof(State));
    s->n = n; s->q = (double*)calloc(n+1, sizeof(double)); s->qd = (double*)calloc(n+1, sizeof(double));
    s->m = (double*)malloc(n * sizeof(double)); s->l = (double*)malloc(n * sizeof(double));
    for (int i = 0; i < n; ++i) { s->m[i] = 0.30 * pow(0.82, i+1); s->l[i] = 0.40 * pow(0.90, i+1); }
    for (int i = 1; i <= n; ++i) s->q[i] = M_PI;
    return s;
}
static void st_destroy(State* s) { free(s->q); free(s->qd); free(s->m); free(s->l); free(s); }

/* Energy */
static double st_energy(const State* s) {
    int n = s->n; double E = 0.5 * CP8_MC * s->qd[0] * s->qd[0];
    for (int i = 1; i <= n; ++i) {
        double xd = s->qd[0], yd = 0;
        for (int j = 1; j <= i; ++j) { xd += s->l[j-1] * cos(s->q[j]) * s->qd[j]; yd += s->l[j-1] * sin(s->q[j]) * s->qd[j]; }
        E += 0.5 * s->m[i-1] * (xd*xd + yd*yd);
    }
    for (int i = 1; i <= n; ++i) { double y = 0; for (int j = 1; j <= i; ++j) y -= s->l[j-1] * cos(s->q[j]); E += s->m[i-1] * CP8_G * y; }
    return E;
}

/* M*qdd = B*u - C - G */
static void st_dyn(const State* s, double u, double* qdd) {
    int n = s->n, N = n + 1;
    double* M = (double*)calloc(N * N, sizeof(double));
    double* C = (double*)calloc(N, sizeof(double));
    double* G = (double*)calloc(N, sizeof(double));
    for (int i = 0; i < N; ++i) for (int j = 0; j < N; ++j) {
        double mij = (i==0 && j==0) ? CP8_MC : 0;
        for (int k = 1; k <= n; ++k) if (i <= k && j <= k) {
            double di = (i==0) ? 1.0 : s->l[i-1] * cos(s->q[i]);
            double dj = (j==0) ? 1.0 : s->l[j-1] * cos(s->q[j]);
            mij += s->m[k-1] * di * dj;
        }
        M[i*N+j] = mij;
    }
    for (int i = 1; i < N; ++i) for (int k = i; k <= n; ++k)
        C[i] -= s->m[k-1] * s->l[i-1] * sin(s->q[i]) * s->qd[i] * s->qd[k];
    for (int i = 1; i < N; ++i) for (int k = i; k <= n; ++k)
        G[i] += s->m[k-1] * CP8_G * s->l[i-1] * cos(s->q[i]);
    double* rhs = (double*)calloc(N, sizeof(double)); rhs[0] = u;
    for (int i = 1; i < N; ++i) rhs[i] = -C[i] - G[i];
    double* A = (double*)malloc(N * N * sizeof(double));
    double* b = (double*)malloc(N * sizeof(double));
    memcpy(A, M, N*N*sizeof(double)); memcpy(b, rhs, N*sizeof(double));
    for (int i = 0; i < N; ++i) {
        int piv = i;
        for (int r = i+1; r < N; ++r) if (fabs(A[r*N+i]) > fabs(A[piv*N+i])) piv = r;
        if (piv != i) { for (int c = i; c < N; ++c) { double t = A[i*N+c]; A[i*N+c] = A[piv*N+c]; A[piv*N+c] = t; } double t = b[i]; b[i] = b[piv]; b[piv] = t; }
        double diag = fabs(A[i*N+i]) < 1e-12 ? 1e-12 : A[i*N+i];
        for (int r = i+1; r < N; ++r) { double f = A[r*N+i]/diag; for (int c = i; c < N; ++c) A[r*N+c] -= f*A[i*N+c]; b[r] -= f*b[i]; }
    }
    for (int i = N-1; i >= 0; --i) { qdd[i] = b[i]; for (int c = i+1; c < N; ++c) qdd[i] -= A[i*N+c]*qdd[c]; qdd[i] /= A[i*N+i]; }
    free(M); free(C); free(G); free(rhs); free(A); free(b);
}

/* RK4 */
static void st_rk4(State* s, double u) {
    int N = s->n + 1;
    double *k1=(double*)calloc(N,sizeof(double)), *k2=(double*)calloc(N,sizeof(double));
    double *k3=(double*)calloc(N,sizeof(double)), *k4=(double*)calloc(N,sizeof(double));
    double *qt=(double*)calloc(N,sizeof(double)), *qdt=(double*)calloc(N,sizeof(double));
    State s2 = *s; s2.q = qt; s2.qd = qdt;
    st_dyn(s, u, k1);
    for (int i = 0; i < N; ++i) { qt[i] = s->q[i] + 0.5*CP8_DT*s->qd[i]; qdt[i] = s->qd[i] + 0.5*CP8_DT*k1[i]; }
    st_dyn(&s2, u, k2);
    for (int i = 0; i < N; ++i) { qt[i] = s->q[i] + 0.5*CP8_DT*(s->qd[i] + 0.5*CP8_DT*k1[i]); qdt[i] = s->qd[i] + 0.5*CP8_DT*k2[i]; }
    st_dyn(&s2, u, k3);
    for (int i = 0; i < N; ++i) { qt[i] = s->q[i] + CP8_DT*(s->qd[i] + 0.5*CP8_DT*k2[i]); qdt[i] = s->qd[i] + CP8_DT*k3[i]; }
    st_dyn(&s2, u, k4);
    for (int i = 0; i < N; ++i) {
        s->q[i] += CP8_DT*s->qd[i] + CP8_DT*CP8_DT/6.0*(k1[i] + 2*k2[i] + 2*k3[i] + k4[i]);
        s->qd[i] += CP8_DT/6.0*(k1[i] + 2*k2[i] + 2*k3[i] + k4[i]);
    }
    for (int i = 1; i <= s->n; ++i) { while (s->q[i] > M_PI) s->q[i] -= 2*M_PI; while (s->q[i] < -M_PI) s->q[i] += 2*M_PI; }
    free(k1); free(k2); free(k3); free(k4); free(qt); free(qdt);
}

/* LQR - simple pole placement approximation */
typedef struct { int n; double* K; } LQR;

static LQR* lqr_create(int n) {
    LQR* l = (LQR*)malloc(sizeof(LQR)); l->n = n; l->K = (double*)calloc(2*n+2, sizeof(double));
    // Simplified K for n-pole: K = [kx, kth1...kthn, kxd, kthd1...kthdn]
    // Hand-tuned gains that work for swing-up catch
    l->K[0] = 5.0;   // cart position
    for (int i = 1; i <= n; ++i) l->K[i] = 200.0;  // pole angles (heavy)
    l->K[n+1] = 10.0;  // cart velocity
    for (int i = 1; i <= n; ++i) l->K[n+1+i] = 20.0;  // pole velocities
    return l;
}
static void lqr_destroy(LQR* l) { free(l->K); free(l); }
static double lqr_ctrl(const LQR* l, const State* s) {
    int n = l->n, N = 2*n + 2; double* x = (double*)calloc(N, sizeof(double));
    for (int i = 0; i <= n; ++i) x[i] = s->q[i]; for (int i = 0; i <= n; ++i) x[n+1+i] = s->qd[i];
    double u = 0; for (int i = 0; i < N; ++i) u -= l->K[i] * x[i]; free(x);
    if (u > CP8_U_MAX) u = CP8_U_MAX; if (u < -CP8_U_MAX) u = -CP8_U_MAX; return u; }

/* Energy swing-up */
static double swing(const State* s, double Et) {
    double E = st_energy(s); double dE = E - Et; double k = 100.0;
    double c = cos(s->q[1]);
    double sgn;
    if (s->qd[0] == 0) { sgn = (c < 0) ? 1.0 : -1.0; }
    else { sgn = (c * s->qd[0] >= 0) ? 1.0 : -1.0; }
    double u = +k * dE * sgn;
    if (u > CP8_U_MAX) u = CP8_U_MAX; if (u < -CP8_U_MAX) u = -CP8_U_MAX; return u; }

static int near_up(const State* s, double th) {
    for (int i = 1; i <= s->n; ++i) if (fabs(s->q[i]) > th) return 0;
    return fabs(s->q[0]) < CP8_X_LIM * 0.5; }

/* Video */
static void ppm(const char* dir, int step, int W, int H, unsigned char* p) {
    char fn[512]; snprintf(fn, 512, "%s/step%05d.ppm", dir, step);
    FILE* f = fopen(fn, "wb"); if (!f) return; fprintf(f, "P6\n%d %d\n255\n", W, H); fwrite(p, 1, W*H*3, f); fclose(f); }
static void draw(unsigned char* p, int W, int H, const State* s) {
    memset(p, 240, W*H*3); int cx = (int)((s->q[0]/CP8_X_LIM + 1)*0.5*W); int cy = H - 80;
    for (int x = 0; x < W; ++x) { int i = (H-50)*W*3+x*3; p[i]=100; p[i+1]=100; p[i+2]=100; }
    int cw=60, ch=30; for (int y=cy-ch; y<cy; ++y) for (int x=cx-cw/2; x<cx+cw/2; ++x) if (x>=0&&x<W&&y>=0&&y<H) { int i=y*W*3+x*3; p[i]=50; p[i+1]=100; p[i+2]=200; }
    for (int w=-1; w<=1; w+=2) { int wx=cx+w*cw/2, wy=cy+5; for (int dy=-8; dy<=8; ++dy) for (int dx=-8; dx<=8; ++dx) if (dx*dx+dy*dy<=64) { int px=wx+dx, py=wy+dy; if (px>=0&&px<W&&py>=0&&py<H) { int i=py*W*3+px*3; p[i]=20; p[i+1]=20; p[i+2]=20; } } }
    int sx=cx, sy=cy-ch; for (int p_ =1; p_<=s->n; ++p_) {
        int ph = 120 - p_*15; int ex = sx + (int)(sin(s->q[p_])*ph), ey = sy - (int)(cos(s->q[p_])*ph);
        int steps = abs(ex-sx) > abs(ey-sy) ? abs(ex-sx) : abs(ey-sy); if (!steps) steps=1;
        for (int i=0;i<=steps;++i) { float t=(float)i/steps; int px=sx+(int)((ex-sx)*t), py=sy+(int)((ey-sy)*t);
            if (px>=0&&px<W&&py>=0&&py<H) { int j=py*W*3+px*3; float h=(p_*0.618)-floor(p_*0.618); p[j]=(uchar)(128+127*sin(h*6.28)); p[j+1]=(uchar)(128+127*sin(h*6.28+2.09)); p[j+2]=(uchar)(128+127*sin(h*6.28+4.19)); } }
        if (ex>=0&&ex<W&&ey>=0&&ey<H) { int j=ey*W*3+ex*3; p[j]=255; p[j+1]=255; p[j+2]=0; } sx=ex; sy=ey; }
    for (int y=cy-ch-150; y<cy-ch; ++y) { int j=y*W*3+cx*3; if (j>=0&&j<W*H*3) { p[j]=0; p[j+1]=200; p[j+2]=0; } } }
static void ppm_frame(const char* dir, int step, int W, int H, unsigned char* p) { char fn[512]; snprintf(fn, 512, "%s/step%05d.ppm", dir, step); FILE* f = fopen(fn, "wb"); if (!f) return; fprintf(f, "P6\n%d %d\n255\n", W, H); fwrite(p, 1, W*H*3, f); fclose(f); }
static void enc_vid(const char* dir, int poles) {
    char cmd[1024]; snprintf(cmd, 1024, "cd %s && ffmpeg -y -framerate %d -pattern_type glob -i '*.ppm' -c:v libx264 -pix_fmt yuv420p -crf 23 ../cartpole_%dpole.mp4 2>/dev/null", dir, VIDEO_FPS, poles); system(cmd); printf("[VIDEO] cartpole_%dpole.mp4\n", poles); }

static int solve(int poles) {
    printf("\n=== %d-Pole ===\n", poles); State* st = st_create(poles); LQR* lqr = lqr_create(poles);
    double Et = 0; for (int i = 1; i <= poles; ++i) Et -= st->m[i-1] * CP8_G * st->l[i-1];
    printf("  Target: %.2f\n", Et);
    char vdir[512]; snprintf(vdir, 512, "/tmp/mp_%dpole_%ld", poles, time(NULL)); mkdir(vdir, 0755);
    unsigned char* fr = (unsigned char*)malloc(VIDEO_W * VIDEO_H * 3); int hold = 0;
    for (int step = 0; step < MAX_STEPS; ++step) {
        int use_lqr = near_up(st, 0.08); double u = use_lqr ? lqr_ctrl(lqr, st) : swing(st, Et); st_rk4(st, u);
        int up = 1; for (int i = 1; i <= poles; ++i) if (fabs(st->q[i]) > 0.05) up = 0;
        if (up && fabs(st->q[0]) < CP8_X_LIM) hold++; else hold = 0;
        draw(fr, VIDEO_W, VIDEO_H, st); ppm(vdir, step, VIDEO_W, VIDEO_H, fr);
        if (step % 20 == 0) printf("  %d: E=%.1f dE=%.1f x=%.3f th=%.3f u=%.1f %s hold=%d\n", step, st_energy(st), st_energy(st)-Et, st->q[0], st->q[1], u, use_lqr?"LQR":"Swing", hold);
        if (hold > 200) { printf("  SOLVED %d steps\n", step); break; }
        if (fabs(st->q[0]) > CP8_X_LIM || (step > 50 && fabs(st->q[1]) > M_PI/2)) { printf("  FAILED step %d\n", step); break; }
    }
    enc_vid(vdir, poles); lqr_destroy(lqr); st_destroy(st); free(fr); return hold > 200; }

int main(int argc, char** argv) {
    int from=7, to=20; for(int i=1;i<argc;++i){ if(!strcmp(argv[i],"--from")) from=atoi(argv[++i]); else if(!strcmp(argv[i],"--to")) to=atoi(argv[++i]); }
    printf("==== MULTI-POLE %d-%d: ENERGY SWING-UP + SIMPLE LQR ====\n", from, to);
    int solved=0; for(int p=from;p<=to;++p){ if(solve(p)){ printf("✓ %d solved\n",p); solved++; } else printf("✗ %d failed\n",p); char cmd[512]; snprintf(cmd,512,"cp /tmp/cartpole_%dpole.mp4 /home/wubu/.hermes/profiles/mind-palace/home/myseed/ 2>/dev/null",p); system(cmd); }
    printf("Solved: %d/%d\n", solved, to-from+1); return 0; }