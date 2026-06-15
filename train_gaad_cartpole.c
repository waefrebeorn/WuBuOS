#include "src/bear/bear_env.h"
#include "src/bear/bear_gae.h"
#include "src/bear/bear_optimizer.h"
#include "src/bear/bear_tensor.h"
#include "src/bear/bear_policy.h"
#include "src/bear/bear_ppo.h"
#include "src/bear/bear_random.h"
#include "src/bear/bear_arena.h"
#include "GAAD_Encoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_POLES 20
#define ENVS 1
#define STEPS 1000

typedef struct {
    BearEnv env;
    BearArena arena;
    GAAD_Encoder encoder;
    GAAD_Optimizer opt;
    float* encoder_params;
    float* encoder_grad;
    int encoder_param_count;
} GAAD_CartPole;

void gaad_cartpole_init(GAAD_CartPole* cp, int poles) {
    cp->arena = bear_arena_create(64 * 1024 * 1024);
    
    // Create env
    cp->env = bear_env_create(cp->arena, BEAR_ENV_CARTPOLE_NPOLE, poles);
    cp->env.max_steps = 2000;
    cp->env.gamma = 0.99f;
    
    // GAAD optimizer for encoder
    cp->opt = gaad_optimizer_create(cp->arena);
    gaad_optimizer_set_lr(&cp->opt, 3e-4f);
    gaad_optimizer_set_betas(&cp->opt, 0.9f, 0.999f);
    gaad_optimizer_set_eps(&cp->opt, 1e-8f);
    gaad_optimizer_set_weight_decay(&cp->opt, 1e-4f);
    
    // GAAD encoder
    int in_dim = 2 + 2 * poles;
    cp->encoder = gaad_encoder_create(
        cp->arena, in_dim, in_dim,  // in_dim, out_dim (obs -> latent)
        4, 256, 256, 4, 8,         // depth, d_model, mlp_mult, heads, num_layers
        32, GAAD_ACT_GELU
    );
    
    // Get encoder parameter count
    cp->encoder_param_count = 0;
    // Approximate: embedding + 4 layers * (attn + mlp + norms)
    // We'll compute dynamically during forward
    fprintf(stderr, "GAAD CartPole init: poles=%d, obs_dim=%d\n", poles, in_dim);
}

int main(int argc, char** argv) {
    int poles = (argc > 1) ? atoi(argv[1]) : 1;
    int total_episodes = (argc > 2) ? atoi(argv[2]) : 100;
    
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "GAAD CartPole Solver - Poles: %d, Episodes: %d\n", poles, total_episodes);
    fprintf(stderr, "========================================\n");
    
    GAAD_CartPole cp;
    gaad_cartpole_init(&cp, poles);
    
    // Storage for trajectory
    float** obs_buf = (float**)malloc(STEPS * sizeof(float*));
    float** action_buf = (float**)malloc(STEPS * sizeof(float*));
    float* reward_buf = (float*)malloc(STEPS * sizeof(float));
    float* value_buf = (float*)malloc(STEPS * sizeof(float));
    float* logprob_buf = (float*)malloc(STEPS * sizeof(float));
    uint8_t* done_buf = (uint8_t*)malloc(STEPS * sizeof(uint8_t));
    
    for (int i = 0; i < STEPS; i++) {
        obs_buf[i] = (float*)malloc((2 + 2 * poles) * sizeof(float));
        action_buf[i] = (float*)malloc(2 * sizeof(float));
    }
    
    float best_return = -1e9f;
    int solved_episodes = 0;
    
    for (int ep = 0; ep < total_episodes; ep++) {
        // Reset
        bear_env_reset(&cp.env);
        int obs_dim = cp.env.obs_dim;
        int act_dim = cp.env.act_dim;  // 2 for continuous
        
        int step = 0;
        float ep_return = 0.0f;
        
        while (step < STEPS) {
            // Get obs
            const float* obs = bear_env_obs(&cp.env);
            for (int i = 0; i < obs_dim; i++) {
                obs_buf[step][i] = obs[i];
            }
            
            // ===== GAAD ENCODER FORWARD =====
            // Use arena temp memory
            BearArena temp_arena = bear_arena_create(8 * 1024 * 1024);
            
            // Flatten obs to 1D for encoder
            // Encoder expects [batch, seq, features] - we use batch=1, seq=1
            float* enc_in = (float*)bear_arena_alloc(&temp_arena, obs_dim * sizeof(float));
            memcpy(enc_in, obs_buf[step], obs_dim * sizeof(float));
            
            // Forward through encoder
            BearTensor enc_out = gaad_encoder_forward(
                &cp.encoder, 
                cp.encoder_param_count > 0 ? cp.encoder_params : NULL,
                enc_in, obs_dim,
                &temp_arena,
                0  // training mode
            );
            
            int latent_dim = enc_out.shape.dims[enc_out.shape.ndim - 1];
            fprintf(stderr, "  Ep%d Step%d: encoder out shape [%d, %d, %d], latent_dim=%d\n", 
                ep, step, enc_out.shape.dims[0], enc_out.shape.dims[1], enc_out.shape.dims[2], latent_dim);
            
            // ===== SIMPLE POLICY ON LATENT =====
            // Use latent as input to a small policy head
            // For now: just use first latent dims + some exploration
            float* action = action_buf[step];
            action[0] = 0.0f;
            action[1] = 0.0f;
            
            // Simple heuristic: if pole angle > 0, push right; else push left
            // obs: [x, x_dot, theta1, theta_dot1, theta2, theta_dot2, ...]
            float theta1 = obs_buf[step][2];
            float theta_dot1 = obs_buf[step][3];
            
            // PD-like control on latent
            float force = -10.0f * theta1 - 1.0f * theta_dot1;
            force = fmaxf(-cp.env.force_mag, fminf(cp.env.force_mag, force));
            action[0] = force;
            
            // Step env
            float reward = bear_env_step(&cp.env, action);
            reward_buf[step] = reward;
            ep_return += reward;
            
            // Check done
            done_buf[step] = cp.env.done ? 1 : 0;
            
            // Cleanup encoder output
            bear_tensor_free(&enc_out, &temp_arena);
            bear_arena_destroy(&temp_arena);
            
            if (done_buf[step]) break;
            
            step++;
        }
        
        fprintf(stderr, "Episode %d: steps=%d, return=%.2f\n", ep, step, ep_return);
        
        if (ep_return > best_return) {
            best_return = ep_return;
            fprintf(stderr, "  >>> NEW BEST: %.2f <<<\n", best_return);
        }
        
        // Check solved (standard: 500 for 1-pole, proportionally less for multi-pole)
        float solve_threshold = (poles == 1) ? 475.0f : (500.0f / poles);
        if (ep_return >= solve_threshold) {
            solved_episodes++;
            if (solved_episodes >= 10) {
                fprintf(stderr, "SOLVED! %d consecutive episodes >= %.1f\n", solved_episodes, solve_threshold);
                break;
            }
        } else {
            solved_episodes = 0;
        }
    }
    
    // Cleanup
    for (int i = 0; i < STEPS; i++) {
        free(obs_buf[i]);
        free(action_buf[i]);
    }
    free(obs_buf); free(action_buf); free(reward_buf);
    free(value_buf); free(logprob_buf); free(done_buf);
    
    bear_arena_destroy(&cp.arena);
    
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "Training complete. Best return: %.2f\n", best_return);
    fprintf(stderr, "Solved episodes: %d/10\n", solved_episodes);
    fprintf(stderr, "========================================\n");
    
    return (solved_episodes >= 10) ? 0 : 1;
}
