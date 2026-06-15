#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define M_PI 3.14159265358979323846

int main() {
    int n = 7;
    int N = n + 1;
    double* q = calloc(N, sizeof(double));
    double* qd = calloc(N, sizeof(double));
    double* m = malloc(n * sizeof(double));
    double* l = malloc(n * sizeof(double));
    double* lc = malloc(n * sizeof(double));
    double* I = malloc(n * sizeof(double));

    for (int i = 0; i < n; ++i) {
        m[i] = 0.30 * pow(0.82, i+1);
        l[i] = 0.40 * pow(0.90, i+1);
        lc[i] = l[i] * 0.5;
        I[i] = m[i] * l[i] * l[i] / 3.0;
    }
    for (int i = 1; i <= n; ++i) q[i] = M_PI;

    double CP_G = 9.81, CP_MC = 1.0;

    /* Build Mass matrix M */
    double* M = calloc(N * N, sizeof(double));
    M[0] = CP_MC;
    for (int i = 0; i < n; ++i) M[0] += m[i];

    for (int j = 1; j <= n; ++j) {
        double mij = 0.0;
        for (int i = j; i <= n; ++i) {
            double th_sum = 0.0;
            for (int k = 1; k <= j; ++k) th_sum += q[k];
            if (i == j) {
                mij += m[i-1] * lc[i-1] * cos(th_sum);
            } else {
                mij += m[i-1] * l[j-1] * cos(th_sum);
            }
        }
        M[0*N + j] = mij;
        M[j*N + 0] = mij;
    }

    for (int i = 1; i <= n; ++i) {
        for (int j = 1; j <= n; ++j) {
            double mij = 0.0;
            int min_ij = (i < j) ? i : j;
            for (int k = min_ij; k <= n; ++k) {
                double th_sum_i = 0.0, th_sum_j = 0.0;
                for (int p = 1; p <= i; ++p) th_sum_i += q[p];
                for (int p = 1; p <= j; ++p) th_sum_j += q[p];
                double di = (i == k) ? lc[k-1] : l[i-1];
                double dj = (j == k) ? lc[k-1] : l[j-1];
                mij += m[k-1] * di * dj * cos(th_sum_i - th_sum_j);
            }
            if (i == j) {
                for (int k = i; k <= n; ++k) mij += I[k-1];
            }
            M[i*N + j] = mij;
        }
    }

    printf("M built\n");
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            printf("%8.3f ", M[i*N+j]);
        }
        printf("\n");
    }

    /* Check determinant - is M invertible? */
    double* A = malloc(N * N * sizeof(double));
    memcpy(A, M, N*N*sizeof(double));
    double det = 1.0;
    for (int i = 0; i < N; ++i) {
        int piv = i;
        for (int r = i+1; r < N; ++r)
            if (fabs(A[r*N+i]) > fabs(A[piv*N+i])) piv = r;
        if (piv != i) {
            for (int c = i; c < N; ++c) { double t = A[i*N+c]; A[i*N+c] = A[piv*N+c]; A[piv*N+c] = t; }
            det = -det;
        }
        det *= A[i*N+i];
        if (fabs(A[i*N+i]) < 1e-12) { printf("Zero pivot at %d\n", i); break; }
        for (int r = i+1; r < N; ++r) {
            double f = A[r*N+i]/A[i*N+i];
            for (int c = i; c < N; ++c) A[r*N+c] -= f*A[i*N+c];
        }
    }
    printf("det(M) = %.3e\n", det);

    free(M); free(A); free(q); free(qd); free(m); free(l); free(lc); free(I);
    return 0;
}