/*
 * bear_nn_policy.c -- WuBuOS BearRL policy-network implementation
 * (MLP/minGRU create, forward, sample, param get/set, orthogonal init).
 * Extracted from the monolithic bear_nn.c. Self-contained; depends only on
 * the public bear_nn.h API. C11, no god headers.
 */

#include "bear_nn.h"
#include "bear_arena.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/*
 * bear_nn.c  --  PufferC/BearRL PolicyNet Implementation
 *
 * Forward + Backward pass for MLP policy and value networks.
 * Pure C11, analytical gradients via chain rule.
 */

#include "bear_nn.h"
#include "bear_arena.h"
#include "bear_simd.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ===================================================================
 * Network Creation
 * =================================================================== */

int bear_policy_create_mlp(BearPolicyNet* net, BearArena* param_arena,
                            int obs_dim, int act_dim, int act_discrete,
                            const int* hid_sizes, int num_hid) {
    if (!net || !param_arena || obs_dim <= 0 || act_dim <= 0) return -1;
    if (num_hid < 1) return -1;

    net->type = BEAR_NET_MLP;
    net->obs_dim = obs_dim;
    net->act_dim = act_dim;
    net->act_discrete = act_discrete;
    net->param_arena = param_arena;
    net->gru = NULL;
    net->num_layers = num_hid + 1;
    net->fwd_stored = 0;

    net->layers = BEAR_ARENA_ALLOC(param_arena, BearLayer, net->num_layers);
    if (!net->layers) return -1;

    int prev = obs_dim;
    int layer_idx = 0;

    for (int i = 0; i < num_hid; ++i) {
        BearLayer* l = &net->layers[layer_idx++];
        l->in_features = prev;
        l->out_features = hid_sizes[i];
        l->act = BEAR_ACT_RELU;
        l->param = BEAR_ARENA_ALLOC(param_arena, BearParam, 1);
        if (!l->param) return -1;
        if (bear_param_create(param_arena, l->param, hid_sizes[i], prev, "policy.hid") != 0) return -1;
        l->act_storage = 0;
        prev = hid_sizes[i];
    }

    /* Actor head */
    BearLayer* actor = &net->layers[layer_idx++];
    actor->in_features = prev;
    actor->out_features = act_dim;
    actor->act = BEAR_ACT_NONE;
    actor->param = BEAR_ARENA_ALLOC(param_arena, BearParam, 1);
    if (!actor->param) return -1;
    if (bear_param_create(param_arena, actor->param, act_dim, prev, "policy.actor") != 0) return -1;
    actor->act_storage = 0;

    net->hid_size = 0;
    return 0;
}

int bear_policy_create_mingru(BearPolicyNet* net, BearArena* param_arena,
                               int obs_dim, int act_dim, int act_discrete,
                               int hid_size) {
    if (!net || !param_arena || obs_dim <= 0 || act_dim <= 0 || hid_size <= 0) return -1;

    net->type = BEAR_NET_MINGU;
    net->obs_dim = obs_dim;
    net->act_dim = act_dim;
    net->act_discrete = act_discrete;
    net->hid_size = hid_size;
    net->param_arena = param_arena;
    net->fwd_stored = 0;

    net->num_layers = 2;
    net->layers = BEAR_ARENA_ALLOC(param_arena, BearLayer, net->num_layers);
    if (!net->layers) return -1;

    int layer_idx = 0;

    BearLayer* in_proj = &net->layers[layer_idx++];
    in_proj->in_features = obs_dim;
    in_proj->out_features = hid_size;
    in_proj->act = BEAR_ACT_RELU;
    in_proj->param = BEAR_ARENA_ALLOC(param_arena, BearParam, 1);
    if (!in_proj->param) return -1;
    if (bear_param_create(param_arena, in_proj->param, hid_size, obs_dim, "policy.in_proj") != 0) return -1;
    in_proj->act_storage = 0;

    BearLayer* actor = &net->layers[layer_idx++];
    actor->in_features = hid_size;
    actor->out_features = act_dim;
    actor->act = BEAR_ACT_NONE;
    actor->param = BEAR_ARENA_ALLOC(param_arena, BearParam, 1);
    if (!actor->param) return -1;
    if (bear_param_create(param_arena, actor->param, act_dim, hid_size, "policy.actor") != 0) return -1;
    actor->act_storage = 0;

    net->gru = BEAR_ARENA_ALLOC(param_arena, BearMinGRU, 1);
    if (!net->gru) return -1;
    if (bear_mingru_create(param_arena, net->gru, hid_size, hid_size) != 0) return -1;

    net->param_arena = param_arena;
    return 0;
}

/* ===================================================================
 * Forward Pass  --  stores activations for backward
 * =================================================================== */

void bear_policy_forward(const BearPolicyNet* net,
                          const BearTensor* obs,
                          const BearTensor* h_in,
                          BearTensor* actions,
                          BearTensor* logprobs,
                          BearTensor* values,
                          BearTensor* h_out,
                          BearArena* temp_arena) {
    int batch = (int)obs->shape[0];
    int act_dim = net->act_dim;

    BearTensor x = *obs;
    BearTensor layer_out;

    if (net->type == BEAR_NET_MLP) {
        /* Hidden layers */
        for (int i = 0; i < net->num_layers - 1; ++i) {
            BearLayer* l = (BearLayer*)&net->layers[i];
            bear_tensor_create(temp_arena, &layer_out, (int64_t[]){batch, l->out_features}, 2, BEAR_DTYPE_F32, "layer_out");
            bear_mlp_layer(&x, &l->param->weight, NULL, &layer_out, l->act, temp_arena);
            /* Store post-activation */
            l->a_post = layer_out;
            l->act_storage = 1;
            x = layer_out;
        }

        /* Actor head: last layer */
        BearLayer* actor = (BearLayer*)&net->layers[net->num_layers - 1];
        bear_tensor_create(temp_arena, &layer_out, (int64_t[]){batch, act_dim}, 2, BEAR_DTYPE_F32, "actor_logits");
        bear_mlp_layer(&x, &actor->param->weight, &actor->param->bias, &layer_out, BEAR_ACT_NONE, temp_arena);
        /* Store pre-activation (logits) for backward */
        actor->z_pre = layer_out;
        actor->act_storage = 1;

        if (net->act_discrete) {
            float* logits = (float*)layer_out.data;
            float* probs = (float*)actions->data;
            for (int b = 0; b < batch; ++b) {
                float max_logit = -INFINITY;
                for (int a = 0; a < act_dim; ++a)
                    if (logits[b * act_dim + a] > max_logit) max_logit = logits[b * act_dim + a];
                float sum_exp = 0.0f;
                for (int a = 0; a < act_dim; ++a)
                    sum_exp += expf(logits[b * act_dim + a] - max_logit);
                for (int a = 0; a < act_dim; ++a)
                    probs[b * act_dim + a] = expf(logits[b * act_dim + a] - max_logit) / sum_exp;
                /* Store probabilities in actions; bear_policy_sample will do stochastic sampling */
                /* logprobs will be computed in bear_policy_sample after sampling */
            }
        } else {
            /* Continuous actions: Gaussian policy with fixed std */
            float* mu = (float*)layer_out.data;
            float* act = (float*)actions->data;
            float* lp = (float*)logprobs->data;
            float ls = net->logstd ? 0.0f : net->logstd_fixed;
            float var = expf(2.0f * ls);
            for (int b = 0; b < batch; ++b) {
                for (int a = 0; a < act_dim; ++a) {
                    /* Box-Muller: sample from N(0,1) */
                    float u1 = (float)(rand() + 1) / (float)((unsigned)RAND_MAX + 2);
                    float u2 = (float)(rand() + 1) / (float)((unsigned)RAND_MAX + 2);
                    float eps = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * 3.14159265f * u2);
                    float std = expf(ls);
                    act[b * act_dim + a] = mu[b * act_dim + a] + std * eps;
                    /* log π(a|s) = -0.5 * ((a-μ)²/σ² + log(2πσ²)) */
                    float diff = act[b * act_dim + a] - mu[b * act_dim + a];
                    lp[b] += -0.5f * (diff * diff / var + logf(2.0f * 3.14159265f * var));
                }
            }
        }

        if (values) memset(values->data, 0, batch * sizeof(float));

    } else if (net->type == BEAR_NET_MINGU) {
        int hid = net->hid_size;

        BearTensor x_proj;
        bear_tensor_create(temp_arena, &x_proj, (int64_t[]){batch, hid}, 2, BEAR_DTYPE_F32, "x_proj");
        bear_mlp_layer(obs, &net->layers[0].param->weight, NULL, &x_proj, BEAR_ACT_RELU, temp_arena);
        net->layers[0].a_post = x_proj;
        net->layers[0].act_storage = 1;

        BearTensor h_next;
        bear_tensor_create(temp_arena, &h_next, (int64_t[]){batch, hid}, 2, BEAR_DTYPE_F32, "h_next");
        bear_mingru_step(net->gru, &x_proj, h_in, &h_next, temp_arena);

        if (h_out) *h_out = h_next;

        BearTensor actor_out;
        bear_tensor_create(temp_arena, &actor_out, (int64_t[]){batch, act_dim}, 2, BEAR_DTYPE_F32, "actor_logits");
        bear_mlp_layer(&h_next, &net->layers[1].param->weight, &net->layers[1].param->bias, &actor_out, BEAR_ACT_NONE, temp_arena);
        net->layers[1].z_pre = actor_out;
        net->layers[1].act_storage = 1;

        if (net->act_discrete) {
            float* logits = (float*)actor_out.data;
            float* probs = (float*)actions->data;
            for (int b = 0; b < batch; ++b) {
                float max_logit = -INFINITY;
                for (int a = 0; a < act_dim; ++a)
                    if (logits[b * act_dim + a] > max_logit) max_logit = logits[b * act_dim + a];
                float sum_exp = 0.0f;
                for (int a = 0; a < act_dim; ++a)
                    sum_exp += expf(logits[b * act_dim + a] - max_logit);
                for (int a = 0; a < act_dim; ++a)
                    probs[b * act_dim + a] = expf(logits[b * act_dim + a] - max_logit) / sum_exp;
                /* Store probabilities in actions; bear_policy_sample will do stochastic sampling */
                /* logprobs will be computed in bear_policy_sample after sampling */
            }
        } else {
            /* Continuous actions: Gaussian policy with fixed std */
            float* mu = (float*)actor_out.data;
            float* act = (float*)actions->data;
            float* lp = (float*)logprobs->data;
            float ls = net->logstd ? 0.0f : net->logstd_fixed;
            float var = expf(2.0f * ls);
            for (int b = 0; b < batch; ++b) {
                for (int a = 0; a < act_dim; ++a) {
                    float u1 = (float)(rand() + 1) / (float)((unsigned)RAND_MAX + 2);
                    float u2 = (float)(rand() + 1) / (float)((unsigned)RAND_MAX + 2);
                    float eps = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * 3.14159265f * u2);
                    float std = expf(ls);
                    act[b * act_dim + a] = mu[b * act_dim + a] + std * eps;
                    float diff = act[b * act_dim + a] - mu[b * act_dim + a];
                    lp[b] += -0.5f * (diff * diff / var + logf(2.0f * 3.14159265f * var));
                }
            }
        }

        if (values) memset(values->data, 0, batch * sizeof(float));
    }

    ((BearPolicyNet*)net)->fwd_stored = 1;
}

void bear_policy_sample(BearPolicyNet* net, BearTensor* actions, BearTensor* logprobs,
                         uint64_t* rng_state) {
    (void)rng_state;
    int batch = (int)actions->shape[0];
    int act_dim = net->act_dim;

    if (net->act_discrete) {
        float* probs = (float*)actions->data;
        for (int b = 0; b < batch; ++b) {
            /* actions already contains probabilities from bear_policy_forward */
            float max_val = -INFINITY;
            int sampled = 0;
            for (int a = 0; a < act_dim; ++a) {
                float gumbel = -logf(-logf((float)rand() / RAND_MAX + 1e-8f));
                float score = logf(probs[b * act_dim + a] + 1e-8f) + gumbel;
                if (score > max_val) { max_val = score; sampled = a; }
            }

            /* Compute logprob of sampled action BEFORE converting to one-hot */
            float sampled_logprob = logf(probs[b * act_dim + sampled] + 1e-8f);

            /* Convert to one-hot */
            for (int a = 0; a < act_dim; ++a) probs[b * act_dim + a] = 0;
            probs[b * act_dim + sampled] = 1.0f;
            ((float*)logprobs->data)[b] = sampled_logprob;
        }
    } else {
        /* Continuous: forward pass already sampled from Gaussian and computed logprobs.
         * This function is a no-op for continuous actions. */
    }
}

void bear_policy_deterministic(BearPolicyNet* net, BearTensor* actions) {
    int batch = (int)actions->shape[0];
    int act_dim = net->act_dim;
    float* probs = (float*)actions->data;

    if (net->act_discrete) {
        for (int b = 0; b < batch; ++b) {
            float max_p = -INFINITY;
            int argmax = 0;
            for (int a = 0; a < act_dim; ++a) {
                if (probs[b * act_dim + a] > max_p) {
                    max_p = probs[b * act_dim + a];
                    argmax = a;
                }
            }
            for (int a = 0; a < act_dim; ++a) probs[b * act_dim + a] = 0;
            probs[b * act_dim + argmax] = 1.0f;
        }
    }
}

int bear_policy_get_params(const BearPolicyNet* net, float* out, int max_params) {
    int idx = 0;
    for (int i = 0; i < net->num_layers; ++i) {
        BearLayer* l = &net->layers[i];
        int n = l->param->weight.shape[0] * l->param->weight.shape[1];
        if (idx + n > max_params) return -1;
        float* w = (float*)l->param->weight.data;
        memcpy(out + idx, w, n * sizeof(float));
        idx += n;
    }
    if (net->gru) {
        BearParam* params[] = { &net->gru->Wz, &net->gru->Uz, &net->gru->bz,
                                &net->gru->Wr, &net->gru->Ur, &net->gru->br,
                                &net->gru->Wn, &net->gru->Un, &net->gru->bn };
        for (int i = 0; i < 9; ++i) {
            int n = params[i]->weight.shape[0] * params[i]->weight.shape[1];
            if (idx + n > max_params) return -1;
            float* w = (float*)params[i]->weight.data;
            memcpy(out + idx, w, n * sizeof(float));
            idx += n;
        }
    }
    return idx;
}

int bear_policy_set_params(BearPolicyNet* net, const float* in, int num_params) {
    int idx = 0;
    for (int i = 0; i < net->num_layers; ++i) {
        BearLayer* l = &net->layers[i];
        int n = l->param->weight.shape[0] * l->param->weight.shape[1];
        if (idx + n > num_params) return -1;
        float* w = (float*)l->param->weight.data;
        memcpy(w, in + idx, n * sizeof(float));
        idx += n;
    }
    if (net->gru) {
        BearParam* params[] = { &net->gru->Wz, &net->gru->Uz, &net->gru->bz,
                                &net->gru->Wr, &net->gru->Ur, &net->gru->br,
                                &net->gru->Wn, &net->gru->Un, &net->gru->bn };
        for (int i = 0; i < 9; ++i) {
            int n = params[i]->weight.shape[0] * params[i]->weight.shape[1];
            if (idx + n > num_params) return -1;
            float* w = (float*)params[i]->weight.data;
            memcpy(w, in + idx, n * sizeof(float));
            idx += n;
        }
    }
    return 0;
}

void bear_orthogonal_init_params(BearPolicyNet* net, float gain) {
    for (int i = 0; i < net->num_layers; ++i) {
        BearLayer* l = &net->layers[i];
        bear_orthogonal_init(&l->param->weight, gain);
        if (l->param->bias.data) bear_tensor_fill(&l->param->bias, 0.0f);
    }
    if (net->gru) {
        BearParam* params[] = { &net->gru->Wz, &net->gru->Uz, &net->gru->bz,
                                &net->gru->Wr, &net->gru->Ur, &net->gru->br,
                                &net->gru->Wn, &net->gru->Un, &net->gru->bn };
        for (int i = 0; i < 9; ++i) {
            bear_orthogonal_init(&params[i]->weight, gain);
            if (params[i]->bias.data) bear_tensor_fill(&params[i]->bias, 0.0f);
        }
    }
}
