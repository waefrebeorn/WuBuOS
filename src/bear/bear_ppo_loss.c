/*
 * bear_ppo_loss.c -- BearRL PPO loss computation, gradient clipping, and
 * gradient application (extracted from the monolithic bear_ppo.c).
 *
 * Self-contained: depends only on the public bear_ppo.h API. C11, no god headers.
 */

#include "bear_ppo.h"
#include "bear_arena.h"
#include "bear_nn.h"
#include "bear_opt.h"
#include "bear_simd.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/* Clip all gradients to max_norm (returns current grad norm) */
float bear_ppo_clip_grad_norm(BearPolicyNet* policy, BearValueNet* critic, float max_norm) {
    float total_norm = 0.0f;
    if (policy && policy->layers) {
        for (int i = 0; i < policy->num_layers; ++i) {
            BearParam* p = policy->layers[i].param;
            if (p && p->grad.data) {
                int n = (int)bear_tensor_numel(&p->grad);
                float* g = (float*)p->grad.data;
                for (int j = 0; j < n; ++j) total_norm += g[j] * g[j];
            }
        }
    }
    if (critic && critic->layers) {
        for (int i = 0; i < critic->num_layers; ++i) {
            BearParam* p = critic->layers[i].param;
            if (p && p->grad.data) {
                int n = (int)bear_tensor_numel(&p->grad);
                float* g = (float*)p->grad.data;
                for (int j = 0; j < n; ++j) total_norm += g[j] * g[j];
            }
        }
    }
    total_norm = sqrtf(total_norm);
    if (total_norm > max_norm && total_norm > 0.0f) {
        float scale = max_norm / total_norm;
        if (policy && policy->layers) {
            for (int i = 0; i < policy->num_layers; ++i) {
                BearParam* p = policy->layers[i].param;
                if (p && p->grad.data) {
                    int n = (int)bear_tensor_numel(&p->grad);
                    float* g = (float*)p->grad.data;
                    for (int j = 0; j < n; ++j) g[j] *= scale;
                }
            }
        }
        if (critic && critic->layers) {
            for (int i = 0; i < critic->num_layers; ++i) {
                BearParam* p = critic->layers[i].param;
                if (p && p->grad.data) {
                    int n = (int)bear_tensor_numel(&p->grad);
                    float* g = (float*)p->grad.data;
                    for (int j = 0; j < n; ++j) g[j] *= scale;
                }
            }
        }
    }
    return total_norm;
}

BearPPOLoss bear_ppo_loss(const BearPolicyNet* policy, const BearValueNet* critic,
                          const BearTensor* obs, const BearTensor* actions,
                          const BearTensor* old_logprobs, const BearTensor* advantages,
                          const BearTensor* returns, const BearTensor* old_values,
                          const BearPPOConfig* cfg,
                          BearArena* temp_arena) {
    BearPPOLoss loss = {0};
    int batch = (int)obs->shape[0];
    int act_dim = policy->act_dim;

    /* Forward pass: policy for mean+logprobs, value net for values */
    BearTensor new_logprobs, new_values, h_out;
    BearTensor new_actions;  /* for discrete: sampled actions; for continuous: mean */
    int64_t act_shape[2] = { batch, act_dim };
    int64_t scalar_shape[1] = { batch };

    bear_tensor_create(temp_arena, &new_actions, act_shape, 2, BEAR_DTYPE_F32, "new_act");
    bear_tensor_create(temp_arena, &new_logprobs, scalar_shape, 1, BEAR_DTYPE_F32, "new_lp");
    bear_tensor_create(temp_arena, &new_values, scalar_shape, 1, BEAR_DTYPE_F32, "new_val");
    bear_tensor_create(temp_arena, &h_out, (int64_t[]){batch, 1}, 2, BEAR_DTYPE_F32, "h_out");

    if (policy->act_discrete) {
        /* Discrete: forward pass samples actions and computes logprobs */
        bear_policy_forward(policy, obs, NULL, &new_actions, &new_logprobs, NULL, &h_out, temp_arena);
    } else {
        /* Continuous: forward pass to get mean (mu), then compute logprob of STORED actions.
         * We need a forward pass that doesn't sample  --  just returns the mean.
         * For now, reuse forward but then overwrite logprobs with evaluate. */
        bear_policy_forward(policy, obs, NULL, &new_actions, &new_logprobs, NULL, &h_out, temp_arena);
        /* new_actions now contains sampled actions; we need the mean (mu).
         * The actor head output (before sampling) is stored in z_pre.
         * Compute logprob of stored actions under N(mu, sigma^2). */
        float* mu = (float*)policy->layers[policy->num_layers - 1].z_pre.data;
        float* stored_act = (float*)actions->data;
        float* new_lp_p = (float*)new_logprobs.data;
        float ls = policy->logstd ? 0.0f : policy->logstd_fixed;
        float var = expf(2.0f * ls);
        float log_norm = -0.5f * logf(2.0f * 3.14159265f * var);
        for (int i = 0; i < batch; ++i) {
            float lp = 0.0f;
            for (int a = 0; a < act_dim; ++a) {
                float diff = stored_act[i * act_dim + a] - mu[i * act_dim + a];
                lp += -0.5f * diff * diff / var + log_norm;
            }
            new_lp_p[i] = lp;
        }
    }

    /* Value network forward: obs -> values */
    bear_value_forward(critic, obs, &new_values, temp_arena);

    float* new_lp_p = (float*)new_logprobs.data;
    float* old_lp_p = (float*)old_logprobs->data;
    float* adv_p = (float*)advantages->data;
    float* ret_p = (float*)returns->data;
    float* old_v_p = (float*)old_values->data;
    float* new_v_p = (float*)new_values.data;

    /* Policy loss: clipped surrogate */
                              float policy_loss = 0.0f;
                              float clip_frac = 0.0f;
                              float approx_kl = 0.0f;

                              for (int i = 0; i < batch; ++i) {
                                  float diff = new_lp_p[i] - old_lp_p[i];
                                  /* Clamp diff to prevent exp() overflow */
                                  if (diff > 20.0f) diff = 20.0f;
                                  if (diff < -20.0f) diff = -20.0f;
                                  float ratio = expf(diff);
                                  float clipped = fmaxf(fminf(ratio, 1.0f + cfg->clip_coef), 1.0f - cfg->clip_coef);
                                  float surr1 = ratio * adv_p[i];
                                  float surr2 = clipped * adv_p[i];
                                  float sample_loss = -fminf(surr1, surr2);
                                  /* Guard against inf/NaN */
                                  if (isnan(sample_loss) || isinf(sample_loss)) sample_loss = 0.0f;
                                  policy_loss += sample_loss;
                                  if (ratio > 1.0f + cfg->clip_coef || ratio < 1.0f - cfg->clip_coef)
                                      clip_frac += 1.0f;
                                  approx_kl += (ratio - 1.0f) - logf(ratio + 1e-8f);
                              }
                              policy_loss /= batch;
                              clip_frac /= batch;
                              approx_kl /= batch;

                              /* Value loss: clipped MSE */
                              float value_loss = 0.0f;
                              for (int i = 0; i < batch; ++i) {
                                  float v_pred = new_v_p[i];
                                  float v_clipped = fmaxf(fminf(v_pred, old_v_p[i] + cfg->clip_coef_vf),
                                                           old_v_p[i] - cfg->clip_coef_vf);
                                  float loss1 = (v_pred - ret_p[i]) * (v_pred - ret_p[i]);
                                  float loss2 = (v_clipped - ret_p[i]) * (v_clipped - ret_p[i]);
                                  value_loss += fmaxf(loss1, loss2);
                              }
                              value_loss = 0.5f * value_loss / batch;

                              /* Entropy bonus */
                              float entropy = 0.0f;
                              if (policy->act_discrete) {
                                  float* probs = (float*)actions->data;
                                  for (int i = 0; i < batch; ++i) {
                                      for (int a = 0; a < act_dim; ++a) {
                                          if (probs[i * act_dim + a] > 1e-8f)
                                              entropy -= probs[i * act_dim + a] * logf(probs[i * act_dim + a] + 1e-8f);
                                      }
                                  }
                                  entropy /= batch;
                              }

                              loss.policy_loss = policy_loss;
                              loss.value_loss = value_loss;
                              loss.entropy_loss = entropy;
                              loss.total_loss = policy_loss + cfg->vf_coef * value_loss - cfg->ent_coef * entropy;
                              loss.approx_kl = approx_kl;
                              loss.clip_frac = clip_frac;

                              return loss;
                          }

void bear_ppo_update(BearPolicyNet* policy, BearValueNet* critic,
                      const BearPPOLoss* loss, BearOptimizer* opt) {
    /* Stub  --  actual gradient computation happens in bear_ppo_apply_gradients
     * which is called from bear_trainer_iter after all minibatches are processed. */
    (void)policy; (void)critic; (void)loss; (void)opt;
}

/* ===================================================================
 * Apply accumulated gradients via Adam optimizer
 * =================================================================== */

void bear_ppo_apply_gradients(BearPolicyNet* policy, BearValueNet* critic,
                               BearOptimizer* opt_policy, BearOptimizer* opt_critic) {
    if (!opt_policy || !opt_critic) return;

    float lr = opt_policy->lr;
    float beta1 = opt_policy->beta1;
    float beta2 = opt_policy->beta2;
    float eps = opt_policy->eps;
    float wd = opt_policy->weight_decay;
    int step = opt_policy->step;

    /* Apply Adam to policy network params */
    if (policy && policy->layers) {
        for (int i = 0; i < policy->num_layers; ++i) {
            BearParam* p = policy->layers[i].param;
            if (!p || !p->grad.data || !p->weight.data) continue;
            int n = (int)bear_tensor_numel(&p->weight);
            float* w = (float*)p->weight.data;
            float* g = (float*)p->grad.data;
            float* m = (float*)p->mom.data;
            float* v = (float*)p->var.data;
            if (!m || !v) continue;
            bear_adam_step_param(w, g, m, v, n, lr, beta1, beta2, eps, wd, step);
        }
    }

    /* Apply Adam to value network params */
    if (critic && critic->layers) {
        for (int i = 0; i < critic->num_layers; ++i) {
            BearParam* p = critic->layers[i].param;
            if (!p || !p->grad.data || !p->weight.data) continue;
            int n = (int)bear_tensor_numel(&p->weight);
            float* w = (float*)p->weight.data;
            float* g = (float*)p->grad.data;
            float* m = (float*)p->mom.data;
            float* v = (float*)p->var.data;
            if (!m || !v) continue;
            bear_adam_step_param(w, g, m, v, n, opt_critic->lr, opt_critic->beta1,
                                  opt_critic->beta2, opt_critic->eps,
                                  opt_critic->weight_decay, opt_critic->step);
        }
    }
}

