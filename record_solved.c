/*
 * record_solved.c — Record video of already-solved CartPole using working trained model
 */

#define _POSIX_C_SOURCE 200809L
#include "src/bear/bear_arena.h"
#include "src/bear/bear_env.h"
#include "src/bear/bear_nn.h"
#include "src/bear/bear_ppo.h"
#include "src/bear/bear_opt.h"
#include "src/bear/bear_gaad.h"
#include "src/bear/wubu_math.h"
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

/* Geometric Encoder - must match training exactly */
typedef struct {
    int num_layers;
    int* layer_sizes;
    float* weights;
    float* biases;
    int input_dim;
    int output_dim;
} GeometricEncoder;

static GeometricEncoder* geo_encoder_create(BearArena* arena, int input_dim, int output_dim, int num_layers) {
    GeometricEncoder* enc = (GeometricEncoder*)bear_arena_alloc(arena, sizeof(GeometricEncoder), 1);
    if (!enc) return NULL;
    enc->num_layers = num_layers;
    enc->input_dim = input_dim;
    enc->output_dim = output_dim;
    enc->layer_sizes = (int*)bear_arena_alloc(arena, sizeof(int) * (num_layers + 1), 1);
    enc->layer_sizes[0] = input_dim;
    float phi = 1.6180339887498948482f;
    for (int i = 1; i < num_layers; ++i) {
        float scale = (i % 2 == 0) ? phi : (1.0f / phi);
        float size = enc->layer_sizes[i-1] * scale;
        enc->layer_sizes[i] = (int)(size + 0.5f);
        if (enc->layer_sizes[i] < 16) enc->layer_sizes[i] = 16;
        if (enc->layer_sizes[i] > 1024) enc->layer_sizes[i] = 1024;
    }
    enc->layer_sizes[num_layers] = output_dim;
    int total_weights = 0, total_biases = 0;
    for (int i = 0; i < num_layers; ++i) {
        total_weights += enc->layer_sizes[i] * enc->layer_sizes[i+1];
        total_biases += enc->layer_sizes[i+1];
    }
    enc->weights = (float*)bear_arena_alloc(arena, sizeof(float) * total_weights, 1);
    enc->biases = (float*)bear_arena_alloc(arena, sizeof(float) * total_biases, 1);
    int w_idx = 0, b_idx = 0;
    uint32_t seed = 0xDEADBEEF;
    for (int i = 0; i < num_layers; ++i) {
        int fan_in = enc->layer_sizes[i];
        int fan_out = enc->layer_sizes[i+1];
        float std = sqrtf(2.0f / fan_in) * (i % 2 == 0 ? 1.618f : 0.618f);
        for (int j = 0; j < fan_in * fan_out; ++j) {
            seed = seed * 1664525 + 1013904223;
            float r = (seed & 0x7FFFFFFF) / 2147483647.0f * 2.0f - 1.0f;
            enc->weights[w_idx++] = r * std;
        }
        for (int j = 0; j < fan_out; ++j) enc->biases[b_idx++] = 0.0f;
    }
    return enc;
}

static void geo_encoder_forward(const GeometricEncoder* enc, const float* input, float* output) {
    int max_buf_size = 0;
    for (int i = 1; i <= enc->num_layers; ++i) if (enc->layer_sizes[i] > max_buf_size) max_buf_size = enc->layer_sizes[i];
    float* prev = (float*)alloca(max_buf_size * sizeof(float));
    float* curr = (float*)alloca(max_buf_size * sizeof(float));
    memcpy(prev, input, enc->input_dim * sizeof(float));
    int w_idx = 0, b_idx = 0;
    for (int layer = 0; layer < enc->num_layers; ++layer) {
        int in_dim = enc->layer_sizes[layer];
        int out_dim = enc->layer_sizes[layer+1];
        for (int j = 0; j < out_dim; ++j) {
            float sum = enc->biases[b_idx++];
            for (int k = 0; k < in_dim; ++k) sum += prev[k] * enc->weights[w_idx++];
            float gelu = 0.5f * sum * (1.0f + tanhf(0.79788456f * (sum + 0.044715f * sum * sum * sum)));
            float phi_mod = (layer % 2 == 0) ? 1.618f : 0.618f;
            curr[j] = gelu * phi_mod;
        }
        memcpy(prev, curr, out_dim * sizeof(float));
    }
    memcpy(output, prev, enc->output_dim * sizeof(float));
}

/* PPM frame writing */
static void write_ppm_frame(const char* dir, int episode, int step, int width, int height, unsigned char* pixels) {
    char filename[512];
    snprintf(filename, sizeof(filename), "%s/ep%03d_step%05d.ppm", dir, episode, step);
    FILE* f = fopen(filename, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", width, height);
    fwrite(pixels, 1, width * height * 3, f);
    fclose(f);
}

/* Render CartPole */
static void render_cartpole(unsigned char* pixels, int width, int height,
                            float cart_x, float cart_vx,
                            float* pole_angles, int num_poles,
                            float max_x) {
    memset(pixels, 240, width * height * 3);
    int cx = (int)((cart_x / max_x + 1.0f) * 0.5f * width);
    int cy = height - 80;
    
    // Ground
    for (int x = 0; x < width; ++x) {
        int idx = (height - 50) * width * 3 + x * 3;
        pixels[idx] = 100; pixels[idx+1] = 100; pixels[idx+2] = 100;
    }
    
    // Cart
    int cart_w = 60, cart_h = 30;
    for (int y = cy - cart_h; y < cy; ++y) {
        for (int x = cx - cart_w/2; x < cx + cart_w/2; ++x) {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                int idx = y * width * 3 + x * 3;
                pixels[idx] = 50; pixels[idx+1] = 100; pixels[idx+2] = 200;
            }
        }
    }
    
    // Wheels
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
    
    // Poles - CHAINED (each pole attached to previous pole's tip)
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

        // Ball tip
        if (ex >= 0 && ex < width && ey >= 0 && ey < height) {
            int idx = ey * width * 3 + ex * 3;
            pixels[idx] = 255; pixels[idx+1] = 255; pixels[idx+2] = 0;
        }

        // Next pole starts from this pole's tip
        sx = ex;
        sy = ey;
    }
    
    // Upright reference
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

int main(int argc, char** argv) {
    int num_poles = 1;
    int seed = 42;
    int episodes = 5;
    const char* policy_path = NULL;
    const char* value_path = NULL;
    int deterministic = 1;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--poles") == 0) num_poles = atoi(argv[++i]);
        else if (strcmp(argv[i], "--seed") == 0) seed = atoi(argv[++i]);
        else if (strcmp(argv[i], "--episodes") == 0) episodes = atoi(argv[++i]);
        else if (strcmp(argv[i], "--policy") == 0) policy_path = argv[++i];
        else if (strcmp(argv[i], "--value") == 0) value_path = argv[++i];
        else if (strcmp(argv[i], "--stochastic") == 0) deterministic = 0;
    }

    BearArena global_arena, eval_arena;
    bear_arena_create(&global_arena, 64 * 1024 * 1024);
    bear_arena_create(&eval_arena, 8 * 1024 * 1024);

    // Create eval env
    BearEnv* env = bear_env_create_npole(num_poles, 1, &eval_arena);
    if (!env) { fprintf(stderr, "Env create failed\n"); return 1; }
    env->spec.max_episode_steps = MAX_STEPS;
    bear_env_reset_all(env, &eval_arena);

    int obs_dim = env->spec.obs_dim;

    // Create encoder + policy
    GeometricEncoder* encoder = geo_encoder_create(&global_arena, obs_dim, 128, 4);
    int phid[] = { 128, 128 };
    BearPolicyNet policy;
    bear_policy_create_mlp(&policy, &global_arena, 128, 1, 0, phid, 2);
    bear_orthogonal_init_params(&policy, 1.0f);
    policy.logstd = NULL; policy.logstd_fixed = 0.0f;

    /* Load trained policy if provided */
    if (policy_path) {
        if (bear_policy_load(&policy, policy_path) == 0) {
            printf("[LOAD] Policy loaded from %s\n", policy_path);
        } else {
            fprintf(stderr, "[LOAD] Failed to load policy from %s, using random weights\n", policy_path);
        }
    }

    /* Load trained value if provided */
    BearValueNet critic = {0};
    int vhid[] = { 64, 64 };
    if (value_path) {
        bear_value_create(&critic, &global_arena, 64, vhid, 2);
        if (bear_value_load(&critic, value_path) == 0) {
            printf("[LOAD] Value loaded from %s\n", value_path);
        }
    }

    // Video setup
    char video_dir[512];
    snprintf(video_dir, sizeof(video_dir), "/tmp/cartpole_demo_%dpole_%ld", num_poles, time(NULL));
    mkdir(video_dir, 0755);
    unsigned char* frame_buf = (unsigned char*)malloc(VIDEO_W * VIDEO_H * 3);
    
    printf("Recording %d episodes for %d-pole...\n", episodes, num_poles);
    
    for (int ep = 0; ep < episodes; ++ep) {
        BearArena step_arena;
        bear_arena_create(&step_arena, 2 * 1024 * 1024);
        bear_env_reset_all(env, &eval_arena);
        
        float ep_ret = 0;
        int done = 0, step = 0;
        float* obs = (float*)malloc(obs_dim * sizeof(float));
        float* enc_obs = (float*)malloc(128 * sizeof(float));
        
        while (!done && step < MAX_STEPS) {
            bear_arena_reset(&step_arena);
            memcpy(obs, env->obs.data, obs_dim * sizeof(float));
            geo_encoder_forward(encoder, obs, enc_obs);
            
            BearTensor enc_t, act, logp, val, h_out;
            int64_t enc_shape[2] = { 1, 128 };
            int64_t act_shape[2] = { 1, 1 };
            int64_t scalar_shape[1] = { 1 };
            bear_tensor_create(&step_arena, &enc_t, enc_shape, 2, BEAR_DTYPE_F32, "enc");
            memcpy(enc_t.data, enc_obs, 128 * sizeof(float));
            bear_tensor_create(&step_arena, &act, act_shape, 2, BEAR_DTYPE_F32, "act");
            bear_tensor_create(&step_arena, &logp, scalar_shape, 1, BEAR_DTYPE_F32, "logp");
            bear_tensor_create(&step_arena, &val, scalar_shape, 1, BEAR_DTYPE_F32, "val");
            bear_tensor_create(&step_arena, &h_out, (int64_t[]){1, 128}, 2, BEAR_DTYPE_F32, "h");
            
            bear_policy_forward(&policy, &enc_t, NULL, &act, &logp, &val, &h_out, &step_arena);
            
            float force = ((float*)act.data)[0];
            BearTensor rew, done_t, next_obs;
            bear_tensor_create(&step_arena, &rew, scalar_shape, 1, BEAR_DTYPE_F32, "rew");
            bear_tensor_create(&step_arena, &done_t, scalar_shape, 1, BEAR_DTYPE_U8, "done");
            int64_t next_shape[2] = { 1, obs_dim };
            bear_tensor_create(&step_arena, &next_obs, next_shape, 2, BEAR_DTYPE_F32, "next");
            
            float* eval_act = (float*)env->actions.data;
            eval_act[0] = force;
            env->step(env, &env->actions, &rew, &done_t, &next_obs, &step_arena);
            
            ep_ret += ((float*)rew.data)[0];
            done = ((uint8_t*)done_t.data)[0];
            memcpy(env->obs.data, next_obs.data, obs_dim * sizeof(float));
            
            // Render
            float pole_angles[20];
            for (int p = 0; p < num_poles; ++p) {
                float sin_t = obs[2 + p * 4];
                float cos_t = obs[2 + p * 4 + 1];
                pole_angles[p] = atan2f(sin_t, cos_t);
            }
            float cart_x = obs[0];
            render_cartpole(frame_buf, VIDEO_W, VIDEO_H, cart_x, obs[1], pole_angles, num_poles, 2.5f);
            write_ppm_frame(video_dir, ep, step, VIDEO_W, VIDEO_H, frame_buf);
            
            step++;
        }
        
        printf("Episode %d: %.1f return, %d steps\n", ep, ep_ret, step);
        bear_arena_destroy(&step_arena);
        free(obs);
        free(enc_obs);
    }
    
    encode_video(video_dir, num_poles);
    
    bear_env_close(env);
    bear_arena_destroy(&global_arena);
    bear_arena_destroy(&eval_arena);
    free(frame_buf);
    return 0;
}
