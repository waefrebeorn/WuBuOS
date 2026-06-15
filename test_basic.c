#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define M_PI 3.14159265358979323846
int main() {
    printf("START\n"); fflush(stdout);
    int n = 7;
    double* q = calloc(n+1, sizeof(double));
    printf("alloc q\n"); fflush(stdout);
    double* qd = calloc(n+1, sizeof(double));
    printf("alloc qd\n"); fflush(stdout);
    double* m = malloc(n * sizeof(double));
    printf("alloc m\n"); fflush(stdout);
    for (int i = 1; i <= n; ++i) q[i] = M_PI;
    printf("init q\n"); fflush(stdout);
    for (int step = 0; step < 100; ++step) {
        if (step % 20 == 0) { printf("step %d q[1]=%.3f\n", step, q[1]); fflush(stdout); }
    }
    printf("DONE\n"); fflush(stdout);
    return 0;
}