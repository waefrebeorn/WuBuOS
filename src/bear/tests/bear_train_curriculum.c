/*
 * bear_train_curriculum.c — Curriculum training 1→7 poles
 * Progressive learning: each pole count uses previous policy as init
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
#include "bear_train_chained.c"  /* include implementation directly */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <alloca.h>

#define MAX_POLES 20
#define MAX_ENVS 32

int main(int argc, char** argv) {
    int nenv = 16; int from_poles = 1, to_poles = 7;
    int iters_per_pole = 80; int seed = (int)time(NULL);
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i],"--envs")) nenv = atoi(argv[++i]);
        else if (!strcmp(argv[i],"--from")) from_poles = atoi(argv[++i]);
        else if (!strcmp(argv[i],"--to")) to_poles = atoi(argv[++i]);
        else if (!strcmp(argv[i],"--iters")) iters_per_pole = atoi(argv[++i]);
        else if (!strcmp(argv[i],"--seed")) seed = atoi(argv[++i]);
        else if (!strcmp(argv[i],"--help")) { printf("Usage: %s [--envs N] [--from N] [--to N] [--iters N] [--seed N]\n",argv[0]);return 0;}
    }
    srand(seed);
    
    /* Keep policy/value networks across curriculum stages */
    BearArena g_arena, r_arena, s_arena;
    BearPolicyNet pl = {0}; BearValueNet cr = {0};
    BearGAADOptimizer* gaad = NULL; BearPPOConfig cfg = {0};
    GeoEnc *pe = NULL, *ve = NULL;
    int policy_init_done = 0;
    
    for (int np = from_poles; np <= to_poles; ++np) {
        printf("\n════════════════════════════════════════\n");
        printf(" CURRICULUM: %d POLES (iters=%d)\n", np, iters_per_pole);
        printf("════════════════════════════════════════\n");
        
        int od = 2 + 4*np;
        
        /* Init arenas once */
        if (!policy_init_done) {
            if (bear_arena_create(&g_arena, 256*1024*1024) ||
                bear_arena_create(&r_arena, 64*1024*1024) ||
                bear_arena_create(&s_arena, 16*1024*1024)) {
                fprintf(stderr, "Arena fail\n"); return 1;
            }
            if (chained_env_init(nenv, np) != 0) return 1;
            policy_init_done = 1;
        } else {
            /* Re-init env with new pole count */
            chained_env_close();
            if (chained_env_init(nenv, np) != 0) return 1;
        }
        
        /* Create encoders fresh each curriculum stage (different obs dim) */
        if (pe) { /* skip - arena will reclaim */ }
        pe = geo_create(&g_arena, od, 128, 4);
        ve = geo_create(&g_arena, od, 64, 3);
        if (!pe || !ve) { fprintf(stderr, "Encoder fail\n"); return 1; }
        
        if (!pl.layers) {
            int ph[] = {128,128};
            if (bear_policy_create_mlp(&pl, &g_arena, 128, 1, 0, ph, 2)) { fprintf(stderr,"Policy fail\n"); return 1; }
            bear_orthogonal_init_params(&pl, 1.0f); pl.logstd = NULL; pl.logstd_fixed = 0.0f;
        }
        
        if (!cr.layers) {
            int vh[] = {64,64};
            if (bear_value_create(&cr, &g_arena, 64, vh, 2)) { fprintf(stderr,"Value fail\n"); return 1; }
            bear_value_orthogonal_init(&cr, 1.0f);
        }
        
        if (!gaad) {
            BearGAADConfig gc = bear_gaad_default_config();
            gc.base_lr = 1e-4f;
            gc.use_log_g_scaling = 1; gc.use_anisotropic = 1; gc.use_resonant = 1; gc.use_poincare = 1;
            int pc = 0;
            for (int i = 0; i < pl.num_layers; ++i) if (pl.layers[i].param && pl.layers[i].param->weight.data)
                pc += pl.layers[i].param->weight.shape[0] * pl.layers[i].param->weight.shape[1];
            for (int i = 0; i < cr.num_layers; ++i) if (cr.layers[i].param && cr.layers[i].param->weight.data)
                pc += cr.layers[i].param->weight.shape[0] * cr.layers[i].param->weight.shape[1];
            gaad = bear_gaad_create(&g_arena, &gc, pc);
            if (!gaad) { fprintf(stderr, "GAAD fail\n"); return 1; }
        }
        
        if (cfg.epochs_per_iter == 0) {
            cfg = bear_ppo_default_config();
            cfg.lr = 1e-4f; cfg.epochs_per_iter = 4; cfg.minibatch_size = 64; cfg.ent_coef = 0.01f;
        }
        
        BearTrajectory trj;
        if (bear_traj_init(&trj, &g_arena, 1024, nenv, 1, od, 1, 0)) { fprintf(stderr, "Traj fail\n"); return 1; }
        
        /* Training loop for this pole count */
        for (int it = 0; it < iters_per_pole; ++it) {
            uint64_t rng[2] = {0xDEADBEEFDEADBEEFull^(uint64_t)seed, 0xCAFEBABECAFEBABEull^(uint64_t)time(NULL)^it};
            train_iter_curriculum(&pl, &cr, &trj, &cfg, gaad, pe, ve, np, nenv, &g_arena, &r_arena, &s_arena, rng, it);
        }
        
        /* Record and save video for this pole count */
        printf("Recording %d-pole video...\n", np);
        char vdir[512]; snprintf(vdir, 512, "/tmp/curr_%dpole_%ld", np, time(NULL));
        mkdir(vdir, 0755);
        unsigned char* fb = malloc(VIDEO_W*VIDEO_H*3);
        record_episode_curriculum(fb, vdir, np, &pl, pe);
        encode_video(vdir, np);
        free(fb);
        printf("✓ %d-pole complete\n", np);
    }
    
    printf("\n════════════════════════════════════════\n");
    printf(" CURRICULUM %d→%d COMPLETE\n", from_poles, to_poles);
    printf("════════════════════════════════════════\n");
    return 0;
}
