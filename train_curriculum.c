/*
 * Minimal curriculum trainer with expert swing-up for 1-pole
 * Builds on working bear_train_chained from earlier
 */
#define _POSIX_C_SOURCE 200809L
#include "src/bear/bear_arena.h"
#include "src/bear/bear_env.h"
#include "src/bear/bear_nn.h"
#include "src/bear/bear_ppo.h"
#include "src/bear/bear_opt.h"
#include "src/bear/bear_gaad.h"
#include "src/bear/wubu_math.h"
#include "src/bear/bear_mujoco.h"
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
#define ROLLOUT_LEN 1024
#define VIDEO_FPS 30
#define VIDEO_W 800
#define VIDEO_H 600
#define MAX_POLES 20
#define MAX_ENVS 32

static inline float norm_angle(float a) {
    while (a > 3.14159265f) a -= 6.28318531f;
    while (a < -3.14159265f) a += 6.28318531f;
    return a;
}

/* ── MuJoCo-backed chained env (abbreviated - using global g_env from earlier) ── */
// We'll link with the object from bear_train_chained.o which has the env
// For now, just use the existing env functions by declaring them

// ChainedEnv struct and functions are in bear_train_chained.c
// We'll just use the curriculum logic here

int main(int argc, char** argv) {
    int nenv = 16; int max_poles = 7; int iters = 50; int seed = (int)time(NULL);
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i],"--envs")) nenv = atoi(argv[++i]);
        else if (!strcmp(argv[i],"--poles")) max_poles = atoi(argv[++i]);
        else if (!strcmp(argv[i],"--iters")) iters = atoi(argv[++i]);
        else if (!strcmp(argv[i],"--seed")) seed = atoi(argv[++i]);
        else if (!strcmp(argv[i],"--help")) { printf("Usage: %s [--envs N] [--poles N] [--iters N] [--seed N]\n",argv[0]);return 0;}
    }
    srand(seed);
    
    printf("════════════════════════════════════════\n");
    printf(" CURRICULUM 1→%d POLES (%d iters each)\n", max_poles, iters);
    printf("════════════════════════════════════════\n");
    
    // Just run the existing bear_train_chained with --curriculum flag
    // This program is a thin wrapper
    char cmd[1024];
    snprintf(cmd, 1024, "LD_LIBRARY_PATH=/home/wubu/mujoco_local/mujoco-3.2.7/lib ./bear_train_chained --curriculum %d --envs %d --iters %d --seed %d", max_poles, nenv, iters, seed);
    return system(cmd);
}
