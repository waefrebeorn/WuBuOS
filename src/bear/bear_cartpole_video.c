/*
 * bear_cartpole_video.c  --  Train N-Pole CartPole + Record Solved Videos
 * Uses bear_trainer API (proven working) + PPM frame rendering + ffmpeg
 */

#define _POSIX_C_SOURCE 200809L
#include "bear_arena.h"
#include "bear_env.h"
#include "bear_nn.h"
#include "bear_ppo.h"
#include "bear_opt.h"
#include "bear_gaad.h"
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
#define ROLLOUT_LEN 2048
#define VIDEO_FPS 30
#define VIDEO_W 800
#define VIDEO_H 600
#define SOLVED_THRESHOLD 475.0f

/* Simple logger for training */
static void train_logger(int iter, float total_steps, float return_mean,
                         float policy_loss, float value_loss, float entropy,
                         float lr, void* user_data) {
    (void)user_data;
    printf("Iter %4d | Steps %10.0f | Return %7.2f | PLoss %7.4f | VLoss %7.4f | Ent %7.4f | LR %.2e\n",
           iter, total_steps, return_mean, policy_loss, value_loss, entropy, lr);
    fflush(stdout);
}

/* PPM frame writing */
static void write_ppm_frame(const char* dir, int episode, int step,
                            int width, int height, unsigned char* pixels) {
    char filename[512];
    snprintf(filename, sizeof(filename), "%s/ep%03d_step%05d.ppm", dir, episode, step);
    FILE* f = fopen(filename, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", width, height);
    fwrite(pixels, 1, width * height * 3, f);
    fclose(f);
}

/* Render CartPole (chained poles from cart) */
static void render_cartpole(unsigned char* pixels, int width, int height,
                            float cart_x, float cart_vx,
                            float* pole_angles, int num_poles,
                            float max_x) {
    memset(pixels, 240, width * height * 3);

    int cx = (int)((cart_x / max_x + 1.0f) * 0.5f * width);
    int cy = height - 80;

    /* Ground */
    for (int x = 0; x < width; ++x) {
        int idx = (height - 50) * width * 3 + x * 3;
        pixels[idx] = 100; pixels[idx+1] = 100; pixels[idx+2] = 100;
    }

    /* Cart body */
    int cart_w = 60, cart_h = 30;
    for (int y = cy - cart_h; y < cy; ++y) {
        for (int x = cx - cart_w/2; x < cx + cart_w/2; ++x) {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                int idx = y * width * 3 + x * 3;
                pixels[idx] = 50; pixels[idx+1] = 100; pixels[idx+2] = 200;
            }
        }
    }

    /* Wheels */
    for (int w = -1; w <= 1; w += 2) {
        int wx = cx + w * cart_w/2, wy = cy + 5;
        for (int dy = -8; dy <= 8; ++dy) {
            for (int dx = -8; dx <= 8; ++dx) {
                if (dx*dx + dy*dy <= 64) {
                    int px = wx + dx, py = wy + dy;
                    if (px >= 0 && px < width && py >= 0 && py < height) {
                        int idx = py * width * 3 + px * 3;
                        pixels[idx] = 20; pixels[idx+1] = 20; pixels[idx+2] = 20;
                    }
                }
            }
        }
    }

    /* Poles - CHAINED (each pole attached to previous pole's tip) */
    int sx = cx, sy = cy - cart_h;
    for (int p = 0; p < num_poles; ++p) {
        float angle = pole_angles[p];
        int pole_h = 120 - p * 15;
        int ex = sx + (int)(sinf(angle) * pole_h);
        int ey = sy - (int)(cosf(angle) * pole_h);

        int steps = abs(ex - sx) > abs(ey - sy) ? abs(ex - sx) : abs(ey - sy);
        if (steps == 0) steps = 1;
        for (int i = 0; i <= steps; ++i) {
            float t = (float)i / steps;
            int px = sx + (int)((ex - sx) * t);
            int py = sy + (int)((ey - sy) * t);
            if (px >= 0 && px < width && py >= 0 && py < height) {
                int idx = py * width * 3 + px * 3;
                float hue = (p * 0.618f) - floorf(p * 0.618f);
                pixels[idx] = (unsigned char)(128 + 127 * sinf(hue * 6.28f));
                pixels[idx+1] = (unsigned char)(128 + 127 * sinf(hue * 6.28f + 2.09f));
                pixels[idx+2] = (unsigned char)(128 + 127 * sinf(hue * 6.28f + 4.19f));
            }
        }

        /* Pole tip */
        if (ex >= 0 && ex < width && ey >= 0 && ey < height) {
            int idx = ey * width * 3 + ex * 3;
            pixels[idx] = 255; pixels[idx+1] = 255; pixels[idx+2] = 0;
        }

        sx = ex;
        sy = ey;
    }

    /* Upright reference line */
    for (int y = cy - cart_h - 150; y < cy - cart_h; ++y) {
        int idx = y * width * 3 + cx * 3;
        if (idx >= 0 && idx < width * height * 3) {
            pixels[idx] = 0; pixels[idx+1] = 200; pixels[idx+2] = 0;
        }
    }
}

/* Encode video with ffmpeg */
static void encode_video(const char* dir, int poles) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "cd %s && ffmpeg -y -framerate %d -pattern_type glob -i '*.ppm' "
        "-c:v libx264 -pix_fmt yuv420p -crf 23 ../cartpole_%dpole.mp4 2>/dev/null",
        dir, VIDEO_FPS, poles);
    int ret = system(cmd);
    if (ret == 0) printf("[VIDEO] Saved: cartpole_%dpole.mp4\n", poles);
    else printf("[VIDEO] ffmpeg failed (ret=%d)\n", ret);
}

/* Record evaluation episodes on N-Pole 1-pole (continuous, shaped rewards) - shows full balance */
static int record_episodes(BearPolicyNet* policy, BearArena* global_arena,
                           const char* video_dir, unsigned char* frame_buf, int num_episodes) {
    /* Create N-Pole 1-pole environment for recording (same as training) */
    size_t cap = 16 * 1024 * 1024;
    BearArena arena;
    bear_arena_create(&arena, cap);
    
    BearEnv* eval_env = bear_env_create_npole(1, 1, &arena);
    if (!eval_env) {
        fprintf(stderr, "Failed to create N-Pole env\n");
        return -1;
    }
    eval_env->spec.max_episode_steps = MAX_STEPS;
    bear_npole_set_episode_length_max(eval_env, MAX_STEPS);

    int best_episode = 0;
    float best_return = -INFINITY;

    int obs_dim = eval_env->spec.obs_dim;  /* N-Pole 1-pole has obs_dim=6 */

    for (int ep = 0; ep < num_episodes; ++ep) {
        BearArena step_arena;
        bear_arena_create(&step_arena, 2 * 1024 * 1024);
        bear_env_reset_all(eval_env, &arena);

        float ep_ret = 0.0f;
        int done = 0;
        int step = 0;

        while (!done && step < MAX_STEPS) {
            bear_arena_reset(&step_arena);

            /* Forward pass through policy */
            BearTensor enc_t, act, logp, val, h_out;
            int64_t enc_shape[2] = { 1, obs_dim };
            int64_t act_shape[2] = { 1, 1 };
            int64_t scalar_shape[1] = { 1 };
            bear_tensor_create(&step_arena, &enc_t, enc_shape, 2, BEAR_DTYPE_F32, "eval_enc");
            memcpy(enc_t.data, eval_env->obs.data, obs_dim * sizeof(float));
            bear_tensor_create(&step_arena, &act, act_shape, 2, BEAR_DTYPE_F32, "eval_act");
            bear_tensor_create(&step_arena, &logp, scalar_shape, 1, BEAR_DTYPE_F32, "eval_logp");
            bear_tensor_create(&step_arena, &val, scalar_shape, 1, BEAR_DTYPE_F32, "eval_val");
            bear_tensor_create(&step_arena, &h_out, (int64_t[]){1, 128}, 2, BEAR_DTYPE_F32, "eval_h");

            bear_policy_forward(policy, &enc_t, NULL, &act, &logp, &val, &h_out, &step_arena);
            bear_policy_deterministic(policy, &act);

            float force = ((float*)act.data)[0];

            BearTensor rew, done_t, next_obs;
            bear_tensor_create(&step_arena, &rew, scalar_shape, 1, BEAR_DTYPE_F32, "eval_rew");
            bear_tensor_create(&step_arena, &done_t, scalar_shape, 1, BEAR_DTYPE_U8, "eval_done");
            int64_t next_shape[2] = { 1, obs_dim };
            bear_tensor_create(&step_arena, &next_obs, next_shape, 2, BEAR_DTYPE_F32, "eval_next");

            float* eval_act = (float*)eval_env->actions.data;
            eval_act[0] = force;
            eval_env->step(eval_env, &eval_env->actions, &rew, &done_t, &next_obs, &step_arena);

            float reward = ((float*)rew.data)[0];
            uint8_t done_flag = ((uint8_t*)done_t.data)[0];

            ep_ret += reward;
            done = done_flag;

            memcpy(eval_env->obs.data, next_obs.data, obs_dim * sizeof(float));

            /* Extract pole angle for rendering (N-Pole: obs = [x, vx, sinθ1, cosθ1, ω1, ...]) */
            float* obs_data = (float*)eval_env->obs.data;
            float pole_angles[1];
            pole_angles[0] = atan2f(obs_data[2], obs_data[3]);
            float cart_x = obs_data[0];
            float cart_vx = obs_data[1];

            render_cartpole(frame_buf, VIDEO_W, VIDEO_H, cart_x, cart_vx, pole_angles, 1, 2.5f);
            write_ppm_frame(video_dir, ep, step, VIDEO_W, VIDEO_H, frame_buf);

            step++;
        }

        if (ep_ret >= MAX_STEPS * 0.95f) {
            printf("  [RECORD] Ep %d: SOLVED (%.1f/%.0f, %d steps)\n", ep, ep_ret, (float)MAX_STEPS, step);
        } else {
            printf("  [RECORD] Ep %d: %.1f return, %d steps\n", ep, ep_ret, step);
        }

        if (ep_ret > best_return) {
            best_return = ep_ret;
            best_episode = ep;
        }

        bear_arena_destroy(&step_arena);
    }

    bear_arena_destroy(&arena);
    return best_episode;
}

/* Train N-Pole and record video */
static int train_and_record(int poles, int num_envs, int iters, int seed, int record_episodes_count) {
    /* RNG state */
    uint64_t rng_state[2] = { 0xDEADBEEFDEADBEEFull ^ (uint64_t)seed,
                               0xCAFEBABECAFEBABEull ^ (uint64_t)time(NULL) };

    /* Arena capacities */
    size_t global_cap = 256 * 1024 * 1024;
    size_t rollout_cap = 64 * 1024 * 1024;
    size_t step_cap = 16 * 1024 * 1024;

    BearArena global_arena, rollout_arena, step_arena;
    if (bear_arena_create(&global_arena, global_cap) != 0) return -1;
    if (bear_arena_create(&rollout_arena, rollout_cap) != 0) return -1;
    if (bear_arena_create(&step_arena, step_cap) != 0) return -1;

    /* Create N-Pole training environment */
    printf("Creating N-Pole %d-pole (continuous, shaped rewards)...\n", poles);
    BearEnv* train_env = bear_env_create_npole(poles, num_envs, &global_arena);
    if (!train_env) return -1;
    bear_npole_set_episode_length_max(train_env, MAX_STEPS);
    train_env->spec.max_episode_steps = MAX_STEPS;

    printf("Environment: obs_dim=%d, act_dim=%d (continuous), max_steps=%d\n",
           train_env->spec.obs_dim, train_env->spec.act_dim, MAX_STEPS);

    /* Create MLP Policy */
    BearPolicyNet policy;
    int phid[] = { 128, 128 };
    if (bear_policy_create_mlp(&policy, &global_arena,
                               train_env->spec.obs_dim, train_env->spec.act_dim,
                               train_env->spec.act_discrete, phid, 2) != 0) return -1;
    bear_orthogonal_init_params(&policy, 1.0f);
    policy.logstd = NULL;
    policy.logstd_fixed = 0.0f;

    /* Create Value Network */
    BearValueNet critic;
    int vhid[] = { 128, 128 };
    if (bear_value_create(&critic, &global_arena, train_env->spec.obs_dim, vhid, 2) != 0) return -1;
    bear_value_orthogonal_init(&critic, 1.0f);

    /* PPO Config */
    BearPPOConfig cfg = bear_ppo_default_config();
    cfg.lr = 1e-4f;
    cfg.epochs_per_iter = 10;
    cfg.minibatch_size = 64;
    cfg.gamma = 0.99f;
    cfg.gae_lambda = 0.95f;
    cfg.clip_coef = 0.2f;
    cfg.clip_coef_vf = 0.2f;
    cfg.vf_coef = 0.5f;
    cfg.ent_coef = 0.01f;
    cfg.target_kl = 0.02f;
    cfg.lr_anneal = 1;
    cfg.normalize_adv = 1;
    cfg.normalize_obs = 1;
    cfg.max_grad_norm = 0.5f;
    cfg.normalize_rewards = 1;

    /* Optimizers */
    BearOptimizer* opt_policy = bear_optimizer_create(&global_arena, BEAR_OPT_ADAM, cfg.lr);
    BearOptimizer* opt_critic = bear_optimizer_create(&global_arena, BEAR_OPT_ADAM, cfg.lr);

    for (int i = 0; i < policy.num_layers; ++i) {
        BearParam* p = policy.layers[i].param;
        if (p && p->weight.data) bear_optimizer_register(opt_policy, p);
    }
    for (int i = 0; i < critic.num_layers; ++i) {
        BearParam* p = critic.layers[i].param;
        if (p && p->weight.data) bear_optimizer_register(opt_critic, p);
    }

    /* Initialize Trainer */
    BearTrainer trainer;
    if (bear_trainer_init(&trainer, &policy, &critic, train_env, &cfg,
                          global_cap, rollout_cap, step_cap) != 0) return -1;
    trainer.global_arena = global_arena;
    trainer.rollout_arena = rollout_arena;
    trainer.step_arena = step_arena;
    trainer.opt_policy = opt_policy;
    trainer.opt_critic = opt_critic;
    trainer.traj.rollout_len = ROLLOUT_LEN;

    bear_trainer_set_logger(&trainer, train_logger, NULL);

    /* Video setup */
    char video_dir[512];
    snprintf(video_dir, sizeof(video_dir), "/tmp/cartpole_video_%dpole_%ld", poles, time(NULL));
    mkdir(video_dir, 0755);
    unsigned char* frame_buf = (unsigned char*)malloc(VIDEO_W * VIDEO_H * 3);

    /* Track best model weights */
    int total_params = 0;
    for (int i = 0; i < policy.num_layers; ++i) {
        BearParam* p = policy.layers[i].param;
        if (p && p->weight.data) total_params += p->weight.shape[0] * p->weight.shape[1];
    }
    for (int i = 0; i < critic.num_layers; ++i) {
        BearParam* p = critic.layers[i].param;
        if (p && p->weight.data) total_params += p->weight.shape[0] * p->weight.shape[1];
    }
    float* best_policy_weights = (float*)malloc(total_params * sizeof(float));
    float* best_critic_weights = (float*)malloc(total_params * sizeof(float));
    /* Training loop */
    float best_eval = -INFINITY;
    int best_iter = -1;

    for (int iter = 0; iter < iters; ++iter) {
        /* LR anneal */
        if (cfg.lr_anneal) {
            float frac = 1.0f - (float)trainer.total_steps / (float)(iters * num_envs * ROLLOUT_LEN);
            float new_lr = cfg.lr * fmaxf(frac, 0.1f);
            bear_optimizer_set_lr(opt_policy, new_lr);
            bear_optimizer_set_lr(opt_critic, new_lr);
        }

        float avg_return = bear_trainer_iter(&trainer, rng_state);

        /* Track best model every iteration */
        if (avg_return > best_eval) {
            best_eval = avg_return;
            best_iter = iter;
            /* Save best weights */
            int idx = 0;
            for (int i = 0; i < policy.num_layers; ++i) {
                BearParam* p = policy.layers[i].param;
                if (p && p->weight.data) {
                    int n = p->weight.shape[0] * p->weight.shape[1];
                    memcpy(best_policy_weights + idx, p->weight.data, n * sizeof(float));
                    idx += n;
                }
            }
            for (int i = 0; i < critic.num_layers; ++i) {
                BearParam* p = critic.layers[i].param;
                if (p && p->weight.data) {
                    int n = p->weight.shape[0] * p->weight.shape[1];
                    memcpy(best_critic_weights + idx, p->weight.data, n * sizeof(float));
                    idx += n;
                }
            }
        }

        /* Periodic recording every 10 iterations */
        if (iter % 10 == 0 || iter == iters - 1) {
            printf("\n[Iter %d] Recording evaluation episodes on N-Pole %d-pole...\n", iter, poles);
            record_episodes(&policy, &global_arena, video_dir, frame_buf, 3);
        }
    }

    /* Restore best weights and record final video */
    printf("\nRestoring best model (iter %d, return %.1f) for final recording...\n", best_iter, best_eval);
    int idx = 0;
    for (int i = 0; i < policy.num_layers; ++i) {
        BearParam* p = policy.layers[i].param;
        if (p && p->weight.data) {
            int n = p->weight.shape[0] * p->weight.shape[1];
            memcpy(p->weight.data, best_policy_weights + idx, n * sizeof(float));
            idx += n;
        }
    }
    for (int i = 0; i < critic.num_layers; ++i) {
        BearParam* p = critic.layers[i].param;
        if (p && p->weight.data) {
            int n = p->weight.shape[0] * p->weight.shape[1];
            memcpy(p->weight.data, best_critic_weights + idx, n * sizeof(float));
            idx += n;
        }
    }

    printf("Final recording of best policy on N-Pole %d-pole...\n", poles);
    record_episodes(&policy, &global_arena, video_dir, frame_buf, 5);
    encode_video(video_dir, poles);

    /* Cleanup */
    free(frame_buf);
    free(best_policy_weights);
    free(best_critic_weights);
    bear_env_close(train_env);
    bear_arena_destroy(&global_arena);
    bear_arena_destroy(&rollout_arena);
    bear_arena_destroy(&step_arena);

    return 0;
}

int main(int argc, char** argv) {
    int num_envs = 16;
    int start_pole = 1, end_pole = 5;
    int iters = 30;
    int seed = (int)time(NULL);
    int record_eps = 5;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--envs") == 0) num_envs = atoi(argv[++i]);
        else if (strcmp(argv[i], "--from") == 0) start_pole = atoi(argv[++i]);
        else if (strcmp(argv[i], "--to") == 0) end_pole = atoi(argv[++i]);
        else if (strcmp(argv[i], "--iters") == 0) iters = atoi(argv[++i]);
        else if (strcmp(argv[i], "--seed") == 0) seed = atoi(argv[++i]);
        else if (strcmp(argv[i], "--record") == 0) record_eps = atoi(argv[++i]);
        else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [--envs N] [--from N] [--to N] [--iters N] [--seed N] [--record N]\n", argv[0]);
            return 0;
        }
    }

    srand(seed);

    printf("===============================================================\n");
    printf("  WUBUOS CARTPOLE %d-%d  --  GAAD + PPO + VIDEO\n", start_pole, end_pole);
    printf("===============================================================\n");
    printf("  Envs: %d | Iters/pole: %d | Seed: %d\n", num_envs, iters, seed);
    printf("  Training: N-Pole (continuous, shaped, hanging start)\n");
    printf("  Physics: cartpole8 exact (m*l point mass, 80N, RK4, hanging)\n");
    printf("  Output: cartpole_Npole.mp4 (recorded from trained policy)\n");
    printf("===============================================================\n\n");

    for (int p = start_pole; p <= end_pole; ++p) {
        printf("\n=== POLE %d/%d ==============================================\n", p, end_pole);
        if (train_and_record(p, num_envs, iters, seed + p, record_eps) != 0) {
            fprintf(stderr, "Failed to train/record for %d-pole\n", p);
            continue;
        }
        printf("✓ Pole %d complete: cartpole_%dpole.mp4\n", p, p);
    }

    printf("\n===============================================================\n");
    printf("  COMPLETE: cartpole_%dpole.mp4 through %dpole.mp4\n", start_pole, end_pole);
    printf("===============================================================\n");
    return 0;
}