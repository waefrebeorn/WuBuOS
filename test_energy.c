#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#define M_PI 3.14159265358979323846

int main() {
    int n = 7;
    double* q = calloc(n+1, sizeof(double));
    double* qd = calloc(n+1, sizeof(double));
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

    printf("Init done\n");
    fflush(stdout);

    /* Test energy computation */
    double CP_G = 9.81, CP_MC = 1.0;
    double T = 0.5 * CP_MC * qd[0] * qd[0];
    double V = 0.0;

    printf("Loop start\n");
    fflush(stdout);

    for (int i = 1; i <= n; ++i) {
        double x_com = q[0];
        double y_com = 0.0;
        for (int j = 1; j <= i; ++j) {
            double th_sum = 0.0;
            for (int k = 1; k <= j; ++k) th_sum += q[k];
            if (j < i) {
                x_com += l[j-1] * sin(th_sum);
                y_com -= l[j-1] * cos(th_sum);
            } else {
                x_com += lc[j-1] * sin(th_sum);
                y_com -= lc[j-1] * cos(th_sum);
            }
        }
        double vx = qd[0];
        double vy = 0.0;
        double thd_sum = 0.0;
        for (int j = 1; j <= i; ++j) {
            double th_sum = 0.0;
            for (int k = 1; k <= j; ++k) { th_sum += q[k]; thd_sum += qd[k]; }
            double factor = (j < i) ? l[j-1] : lc[j-1];
            vx += factor * cos(th_sum) * thd_sum;
            vy += factor * sin(th_sum) * thd_sum;
        }
        T += 0.5 * m[i-1] * (vx*vx + vy*vy) + 0.5 * I[i-1] * thd_sum * thd_sum;
        V += m[i-1] * CP_G * y_com;
        printf("i=%d T=%.3f V=%.3f\n", i, T, V);
        fflush(stdout);
    }
    printf("Energy: %.3f\n", T+V);
    return 0;
}