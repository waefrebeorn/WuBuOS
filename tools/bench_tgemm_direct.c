#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#ifdef _OPENMP
#include <omp.h>
#endif

static double nowsec(void){ struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts); return ts.tv_sec+ts.tv_nsec*1e-9; }

/* 4-row tiled kernel: C += A*B, row-major */
static void tgemm_4row(int64_t *C, const int64_t *A, const int64_t *B, int M, int N, int K) {
    int i = 0;
    for (; i + 3 < M; i += 4) {
        int64_t c0=0, c1=0, c2=0, c3=0;
        for (int j = 0; j < N; j++) {
            int64_t s0=0, s1=0, s2=0, s3=0;
            for (int k = 0; k < K; k++) {
                s0 += A[(i+0)*K + k] * B[k*N + j];
                s1 += A[(i+1)*K + k] * B[k*N + j];
                s2 += A[(i+2)*K + k] * B[k*N + j];
                s3 += A[(i+3)*K + k] * B[k*N + j];
            }
            c0 += s0; c1 += s1; c2 += s2; c3 += s3;
        }
        C[i*N + 0] = c0; C[i*N + 1] = c1; C[i*N + 2] = c2; C[i*N + 3] = c3;
    }
    for (; i < M; i++) {
        for (int j = 0; j < N; j++) {
            int64_t s = 0;
            for (int k = 0; k < K; k++) s += A[i*K + k] * B[k*N + j];
            C[i*N + j] = s;
        }
    }
}

int main(void){
    const int M=64, N=64, K=64;
    int64_t *A=malloc(M*K*8), *B=malloc(K*N*8), *C=calloc(M*N,8), *Ref=calloc(M*N,8);
    for(int i=0;i<M*K;i++)A[i]=(rand()%17)-8;
    for(int i=0;i<K*N;i++)B[i]=(rand()%13)-6;

    memcpy(Ref, A, M*K*8);
    tgemm_4row(Ref, A, B, M, N, K);

    int nt=1;
#ifdef _OPENMP
    const char* ntc=getenv("OMP_NUM_THREADS");
    if(ntc) nt=atoi(ntc);
#endif

    double t0=nowsec();
    for(int r=0;r<20;r++){ memset(C,0,M*N*8); tgemm_4row(C,A,B,M,N,K); }
    double t=(nowsec()-t0)/20;

    int bad=0;
    for(int i=0;i<M*N && bad<5;i++) if(C[i]!=Ref[i]) bad++;

    printf("sz=%dx%dx%d nt=%d t=%.3fms bad=%d %s\n", M,N,K,nt,t*1000,bad,bad?"FAIL":"PASS");
    free(A); free(B); free(C); free(Ref);
    return bad;
}