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

/* ===================================================================
 * Value Network
 * =================================================================== */

int bear_value_create(BearValueNet* vnet, BearArena* param_arena,
                       int obs_dim, const int* hid_sizes, int num_hid) {
    if (!vnet || !param_arena) return -1;

    vnet->param_arena = param_arena;
    vnet->num_layers = num_hid + 1;
    vnet->fwd_stored = 0;
    vnet->layers = BEAR_ARENA_ALLOC(param_arena, BearLayer, vnet->num_layers);
    if (!vnet->layers) return -1;

    int prev = obs_dim;
    for (int i = 0; i < num_hid; ++i) {
        BearLayer* l = &vnet->layers[i];
        l->in_features = prev;
        l->out_features = hid_sizes[i];
        l->act = BEAR_ACT_RELU;
        l->param = BEAR_ARENA_ALLOC(param_arena, BearParam, 1);
        if (!l->param) return -1;
        if (bear_param_create(param_arena, l->param, hid_sizes[i], prev, "value.hid") != 0) return -1;
        l->act_storage = 0;
        prev = hid_sizes[i];
    }

    BearLayer* out = &vnet->layers[vnet->num_layers - 1];
    out->in_features = prev;
    out->out_features = 1;
    out->act = BEAR_ACT_NONE;
    out->param = BEAR_ARENA_ALLOC(param_arena, BearParam, 1);
    if (!out->param) return -1;
    if (bear_param_create(param_arena, out->param, 1, prev, "value.out") != 0) return -1;
    out->act_storage = 0;

    return 0;
}

void bear_value_forward(const BearValueNet* vnet,
                         const BearTensor* obs,
                         BearTensor* values,
                         BearArena* temp_arena) {
    BearTensor x = *obs;
    BearTensor layer_out;
    int batch = (int)obs->shape[0];

    for (int i = 0; i < vnet->num_layers; ++i) {
        BearLayer* l = (BearLayer*)&vnet->layers[i];
        bear_tensor_create(temp_arena, &layer_out, (int64_t[]){batch, l->out_features}, 2, BEAR_DTYPE_F32, "value_layer");
        bear_mlp_layer(&x, &l->param->weight, &l->param->bias, &layer_out, l->act, temp_arena);
        /* Store activations for backward */
        if (i < vnet->num_layers - 1) {
            l->a_post = layer_out;
        } else {
            l->z_pre = layer_out;
        }
        l->act_storage = 1;
        x = layer_out;
    }

    float* v = (float*)x.data;
    for (int b = 0; b < batch; ++b) ((float*)values->data)[b] = v[b];

    ((BearValueNet*)vnet)->fwd_stored = 1;
}

void bear_value_orthogonal_init(BearValueNet* vnet, float gain) {
    for (int i = 0; i < vnet->num_layers; ++i) {
        BearLayer* l = &vnet->layers[i];
        bear_orthogonal_init(&l->param->weight, gain);
        if (l->param->bias.data) bear_tensor_fill(&l->param->bias, 0.0f);
    }
}

/* ===================================================================
 * Backward Pass  --  Analytical Gradients
 * =================================================================== */

/*
 * Policy backward for discrete (categorical) actions.
 *
 * The PPO clipped surrogate loss for sample i:
 *   L_i = -min(r_i * A_i, clip(r_i, 1-eps, 1+eps) * A_i)
 * where r_i = exp(new_lp_i - old_lp_i) = π_new(a_i|s_i) / π_old(a_i|s_i)
 *
 * For the chosen action a (one-hot in actions tensor):
 *   dL/dlogit_a = -[clipped ? 0 : r_i * A_i] * (1 - π_a) / mb_size
 *   dL/dlogit_j = -[clipped ? 0 : r_i * A_i] * (-π_j) / mb_size  (j != a)
 * Simplified: dL/dlogit = -signal * (one_hot_a - π) / mb_size
 * where signal = r_i * A_i if not clipped, else 0
 *
 * Then backprop through linear layers:
 *   dL/dW_out = dL/dlogit^T @ h_prev
 *   dL/db_out = sum(dL/dlogit, axis=0)
 *   dL/dh_prev = dL/dlogit @ W_out
 *   dL/dh_prev *= (h_prev > 0)  [ReLU derivative]
 *   ... repeat for each hidden layer
 */

int bear_policy_backward(BearPolicyNet* net,
                          const BearTensor* obs,
                          const BearTensor* actions,
                          const BearTensor* old_logprobs,
                          const BearTensor* advantages,
                          float clip_coef,
                          float policy_grad_scale,
                          BearArena* temp_arena) {
    if (!net || !net->fwd_stored) return -1;

    if (net->act_discrete) {
        return bear_policy_backward_discrete(net, obs, actions, old_logprobs, advantages,
                                              clip_coef, policy_grad_scale, temp_arena);
    } else {
        return bear_policy_backward_continuous(net, obs, actions, old_logprobs, advantages,
                                                clip_coef, policy_grad_scale, temp_arena);
    }
}

/*
 * Discrete (categorical) backward  --  original implementation
 */
int bear_policy_backward_discrete(BearPolicyNet* net,
                                    const BearTensor* obs,
                                    const BearTensor* actions,
                                    const BearTensor* old_logprobs,
                                    const BearTensor* advantages,
                                    float clip_coef,
                                    float policy_grad_scale,
                                    BearArena* temp_arena) {

    int mb = (int)obs->shape[0];
    int act_dim = net->act_dim;
    int last = net->num_layers - 1;

    /* -- Step 1: compute dL/dlogit (gradient at output) -- */
    float* dlogit = (float*)BEAR_ARENA_ALLOC(temp_arena, float, mb * act_dim);
    if (!dlogit) return -1;

    float* old_lp = (float*)old_logprobs->data;
    float* adv = (float*)advantages->data;
    float* chosen = (float*)actions->data;  /* one-hot */

    /* Recompute logits from stored activation */
    BearLayer* actor = &net->layers[last];
    float* logits = (float*)actor->z_pre.data;

    for (int i = 0; i < mb; ++i) {
        /* Compute π_new from logits (softmax) */
        float max_logit = -INFINITY;
        for (int a = 0; a < act_dim; ++a)
            if (logits[i * act_dim + a] > max_logit) max_logit = logits[i * act_dim + a];
        float sum_exp = 0.0f;
        for (int a = 0; a < act_dim; ++a)
            sum_exp += expf(logits[i * act_dim + a] - max_logit);

        float pi_new[64];  /* max act_dim */
        for (int a = 0; a < act_dim; ++a)
            pi_new[a] = expf(logits[i * act_dim + a] - max_logit) / sum_exp;

        /* Find chosen action from one-hot */
        int chosen_a = 0;
        for (int a = 0; a < act_dim; ++a)
            if (chosen[i * act_dim + a] > 0.5f) { chosen_a = a; break; }

        /* Ratio r = π_new / π_old = exp(log π_new - old_lp) */
        float log_pi_new_chosen = logf(pi_new[chosen_a] + 1e-8f);
        float ratio = expf(log_pi_new_chosen - old_lp[i]);
        float clipped = fmaxf(fminf(ratio, 1.0f + clip_coef), 1.0f - clip_coef);

        /* PPO clipped surrogate gradient:
         * If clipped: gradient is 0 (don't update)
         * If not clipped: dL/dlogit = -ratio * A * (one_hot - π) / mb
         *
         * We use the "negative loss" convention: positive = good.
         * So signal = ratio * adv[i] (positive adv = encourage action)
         */
        int is_clipped = (ratio > 1.0f + clip_coef || ratio < 1.0f - clip_coef);
        float signal = is_clipped ? 0.0f : ratio * adv[i] * policy_grad_scale / (float)mb;

        for (int a = 0; a < act_dim; ++a) {
            float indicator = (a == chosen_a) ? 1.0f : 0.0f;
            dlogit[i * act_dim + a] = signal * (indicator - pi_new[a]);
        }
    }

    /* -- Step 2: backprop through layers -- */
    /* dlogit is [mb, act_dim]. We need to backprop through each layer. */

    /* Temp buffer for gradient w.r.t. layer input */
    float* dx = (float*)BEAR_ARENA_ALLOC(temp_arena, float, mb * actor->in_features);
    if (!dx) return -1;

    /* Backprop through output (actor) layer:
     * z = h_prev @ W^T + b
     * dL/dW = dL/dz^T @ h_prev   [act_dim, in_f]
     * dL/db = sum(dL/dz, axis=0)  [act_dim]
     * dL/dh_prev = dL/dz @ W      [mb, in_f]
     */
    float* w_out = (float*)actor->param->weight.data;
    float* b_out = (float*)actor->param->bias.data;
    float* grad_w_out = (float*)actor->param->grad.data;
    float* grad_b_out = (float*)actor->param->grad.data;  /* bias grad stored separately */

    /* h_prev = stored a_post of previous layer (or obs for first layer) */
    float* h_prev;
    int prev_feat;
    if (last > 0) {
        BearLayer* prev_layer = &net->layers[last - 1];
        h_prev = (float*)prev_layer->a_post.data;
        prev_feat = prev_layer->out_features;
    } else {
        h_prev = (float*)obs->data;
        prev_feat = net->obs_dim;
    }

    /* dL/dW_out[d, k] = sum_i dlogit[i,d] * h_prev[i,k] */
    for (int d = 0; d < act_dim; ++d) {
        for (int k = 0; k < prev_feat; ++k) {
            float g = 0.0f;
            for (int i = 0; i < mb; ++i)
                g += dlogit[i * act_dim + d] * h_prev[i * prev_feat + k];
            grad_w_out[d * prev_feat + k] += g;
        }
    }

    /* dL/dh_prev[i, k] = sum_d dlogit[i,d] * W[d, k] */
    for (int i = 0; i < mb; ++i) {
        for (int k = 0; k < prev_feat; ++k) {
            float g = 0.0f;
            for (int d = 0; d < act_dim; ++d)
                g += dlogit[i * act_dim + d] * w_out[d * prev_feat + k];
            dx[i * prev_feat + k] = g;
        }
    }

    /* -- Step 3: backprop through hidden layers -- */
    for (int li = last - 1; li >= 0; --li) {
        BearLayer* l = &net->layers[li];
        int in_f = l->in_features;
        int out_f = l->out_features;
        float* w = (float*)l->param->weight.data;
        float* grad_w = (float*)l->param->grad.data;

        /* ReLU backward: dx *= (z_pre > 0) */
        if (l->act == BEAR_ACT_RELU && l->act_storage) {
            float* z_pre = (float*)l->a_post.data;  /* for ReLU, a_post = relu(z), but we need z_pre */
            /* Actually a_post IS the post-activation. We need pre-activation.
             * For ReLU: if a_post > 0 then z_pre > 0, so derivative = 1.
             * This is correct because ReLU'(x) = 1 if x > 0, else 0.
             * And a_post = max(0, z_pre), so a_post > 0 iff z_pre > 0. */
            for (int i = 0; i < mb * out_f; ++i) {
                if (((float*)l->a_post.data)[i] <= 0.0f)
                    dx[i] = 0.0f;
            }
        }

        /* Input to this layer */
        float* layer_input;
        int in_feat;
        if (li > 0) {
            BearLayer* prev_l = &net->layers[li - 1];
            layer_input = (float*)prev_l->a_post.data;
            in_feat = prev_l->out_features;
        } else {
            layer_input = (float*)obs->data;
            in_feat = net->obs_dim;
        }

        /* dL/dW[of, k] = sum_i dx[i, of] * layer_input[i, k] */
        for (int of = 0; of < out_f; ++of) {
            for (int k = 0; k < in_feat; ++k) {
                float g = 0.0f;
                for (int i = 0; i < mb; ++i)
                    g += dx[i * out_f + of] * layer_input[i * in_feat + k];
                grad_w[of * in_feat + k] += g;
            }
        }

        /* dL/dx_prev[i, k] = sum_of dx[i, of] * W[of, k] */
        if (li > 0) {
            float* new_dx = (float*)BEAR_ARENA_ALLOC(temp_arena, float, mb * in_feat);
            if (!new_dx) return -1;
            for (int i = 0; i < mb; ++i) {
                for (int k = 0; k < in_feat; ++k) {
                    float g = 0.0f;
                    for (int of = 0; of < out_f; ++of)
                        g += dx[i * out_f + of] * w[of * in_feat + k];
                    new_dx[i * in_feat + k] = g;
                }
            }
            dx = new_dx;
        }
    }

    return 0;
}

/*
 * Continuous (Gaussian) backward.
 *
 * For Gaussian policy π(a|s) = N(μ, σ²I):
 *   log π(a|s) = -0.5 * Σ_d ((a_d - μ_d)² / σ² + log(2πσ²))
 *
 * PPO clipped surrogate gradient w.r.t. μ:
 *   dL/dμ = signal * (a - μ) / σ²
 * where signal = ratio * adv / mb if not clipped, else 0
 *   ratio = exp(new_lp - old_lp)
 *
 * This is analogous to the discrete case but with (a-μ)/σ² replacing (one_hot - π).
 */
int bear_policy_backward_continuous(BearPolicyNet* net,
                                     const BearTensor* obs,
                                     const BearTensor* actions,
                                     const BearTensor* old_logprobs,
                                     const BearTensor* advantages,
                                     float clip_coef,
                                     float policy_grad_scale,
                                     BearArena* temp_arena) {
    if (!net || !net->fwd_stored) return -1;

    int mb = (int)obs->shape[0];
    int act_dim = net->act_dim;
    int last = net->num_layers - 1;

    float* old_lp = (float*)old_logprobs->data;
    float* adv = (float*)advantages->data;
    float* act = (float*)actions->data;

    /* Recompute mu from stored activation (actor head output = mu) */
    BearLayer* actor = &net->layers[last];
    float* mu = (float*)actor->z_pre.data;

    float ls = net->logstd ? 0.0f : net->logstd_fixed;
    float var = expf(2.0f * ls);

    /* -- Step 1: compute dL/dmu (gradient at output) -- */
    /* dmu: [mb, act_dim] */
    float* dmu = (float*)BEAR_ARENA_ALLOC(temp_arena, float, mb * act_dim);
    if (!dmu) return -1;

    /* We need new_logprobs. Recompute from stored mu and actions. */
    for (int i = 0; i < mb; ++i) {
        /* Recompute new logprob for this sample */
        float new_lp = 0.0f;
        for (int a = 0; a < act_dim; ++a) {
            float diff = act[i * act_dim + a] - mu[i * act_dim + a];
            new_lp += -0.5f * (diff * diff / var + logf(2.0f * 3.14159265f * var));
        }
        float ratio = expf(new_lp - old_lp[i]);
        float clipped = fmaxf(fminf(ratio, 1.0f + clip_coef), 1.0f - clip_coef);
        int is_clipped = (ratio > 1.0f + clip_coef || ratio < 1.0f - clip_coef);
        float signal = is_clipped ? 0.0f : ratio * adv[i] * policy_grad_scale / (float)mb;

        /* dL/dmu_d = signal * (a_d - mu_d) / var */
        for (int a = 0; a < act_dim; ++a) {
            dmu[i * act_dim + a] = signal * (act[i * act_dim + a] - mu[i * act_dim + a]) / var;
        }
    }

    /* -- Step 2: backprop through layers (same as discrete, using dmu instead of dlogit) -- */
    float* dx = (float*)BEAR_ARENA_ALLOC(temp_arena, float, mb * actor->in_features);
    if (!dx) return -1;

    float* w_out = (float*)actor->param->weight.data;
    float* grad_w_out = (float*)actor->param->grad.data;

    float* h_prev;
    int prev_feat;
    if (last > 0) {
        h_prev = (float*)net->layers[last - 1].a_post.data;
        prev_feat = net->layers[last - 1].out_features;
    } else {
        h_prev = (float*)obs->data;
        prev_feat = net->obs_dim;
    }

    /* dL/dW_out[d, k] = sum_i dmu[i,d] * h_prev[i,k] */
    for (int d = 0; d < act_dim; ++d) {
        for (int k = 0; k < prev_feat; ++k) {
            float g = 0.0f;
            for (int i = 0; i < mb; ++i)
                g += dmu[i * act_dim + d] * h_prev[i * prev_feat + k];
            grad_w_out[d * prev_feat + k] += g;
        }
    }

    /* dL/dh_prev[i, k] = sum_d dmu[i,d] * W[d, k] */
    for (int i = 0; i < mb; ++i) {
        for (int k = 0; k < prev_feat; ++k) {
            float g = 0.0f;
            for (int d = 0; d < act_dim; ++d)
                g += dmu[i * act_dim + d] * w_out[d * prev_feat + k];
            dx[i * prev_feat + k] = g;
        }
    }

    /* -- Step 3: backprop through hidden layers (identical to discrete) -- */
    for (int li = last - 1; li >= 0; --li) {
        BearLayer* l = &net->layers[li];
        int in_f = l->in_features;
        int out_f = l->out_features;
        float* w = (float*)l->param->weight.data;
        float* grad_w = (float*)l->param->grad.data;

        /* ReLU backward */
        if (l->act == BEAR_ACT_RELU && l->act_storage) {
            for (int i = 0; i < mb * out_f; ++i) {
                if (((float*)l->a_post.data)[i] <= 0.0f)
                    dx[i] = 0.0f;
            }
        }

        float* layer_input;
        int in_feat;
        if (li > 0) {
            layer_input = (float*)net->layers[li - 1].a_post.data;
            in_feat = net->layers[li - 1].out_features;
        } else {
            layer_input = (float*)obs->data;
            in_feat = net->obs_dim;
        }

        for (int of = 0; of < out_f; ++of) {
            for (int k = 0; k < in_feat; ++k) {
                float g = 0.0f;
                for (int i = 0; i < mb; ++i)
                    g += dx[i * out_f + of] * layer_input[i * in_feat + k];
                grad_w[of * in_feat + k] += g;
            }
        }

        if (li > 0) {
            float* new_dx = (float*)BEAR_ARENA_ALLOC(temp_arena, float, mb * in_feat);
            if (!new_dx) return -1;
            for (int i = 0; i < mb; ++i) {
                for (int k = 0; k < in_feat; ++k) {
                    float g = 0.0f;
                    for (int of = 0; of < out_f; ++of)
                        g += dx[i * out_f + of] * w[of * in_feat + k];
                    new_dx[i * in_feat + k] = g;
                }
            }
            dx = new_dx;
        }
    }

    return 0;
}

/*
 * Value backward: MSE loss L = 0.5 * (V - target)^2
 * dL/dV = (V - target) / mb
 * Then backprop through MLP layers same as policy.
 */
int bear_value_backward(BearValueNet* vnet,
                         const BearTensor* obs,
                         const BearTensor* values,
                         const BearTensor* targets,
                         float vf_coef,
                         BearArena* temp_arena) {
    if (!vnet || !vnet->fwd_stored) return -1;

    int mb = (int)obs->shape[0];
    int last = vnet->num_layers - 1;

    float* v_pred = (float*)values->data;
    float* v_target = (float*)targets->data;

    /* dL/dv = (v_pred - target) * vf_coef / mb */
    float* dv = (float*)BEAR_ARENA_ALLOC(temp_arena, float, mb);
    if (!dv) return -1;
    for (int i = 0; i < mb; ++i)
        dv[i] = (v_pred[i] - v_target[i]) * 2.0f * vf_coef / (float)mb;

    /* Backprop through output layer */
    BearLayer* out_layer = &vnet->layers[last];
    float* w_out = (float*)out_layer->param->weight.data;
    float* grad_w_out = (float*)out_layer->param->grad.data;
    int out_f = out_layer->out_features;  /* = 1 */
    int in_f = out_layer->in_features;

    float* h_prev;
    int prev_feat;
    if (last > 0) {
        h_prev = (float*)vnet->layers[last - 1].a_post.data;
        prev_feat = vnet->layers[last - 1].out_features;
    } else {
        h_prev = (float*)obs->data;
        prev_feat = obs->shape[1];
    }

    /* dL/dW_out[0, k] = sum_i dv[i] * h_prev[i, k] */
    for (int k = 0; k < prev_feat; ++k) {
        float g = 0.0f;
        for (int i = 0; i < mb; ++i)
            g += dv[i] * h_prev[i * prev_feat + k];
        grad_w_out[k] += g;
    }

    /* dL/dh_prev[i, k] = dv[i] * W_out[0, k] */
    float* dx = (float*)BEAR_ARENA_ALLOC(temp_arena, float, mb * prev_feat);
    if (!dx) return -1;
    for (int i = 0; i < mb; ++i)
        for (int k = 0; k < prev_feat; ++k)
            dx[i * prev_feat + k] = dv[i] * w_out[k];

    /* Backprop through hidden layers */
    for (int li = last - 1; li >= 0; --li) {
        BearLayer* l = &vnet->layers[li];
        int li_out = l->out_features;
        int li_in = l->in_features;
        float* w = (float*)l->param->weight.data;
        float* grad_w = (float*)l->param->grad.data;

        /* ReLU backward */
        if (l->act == BEAR_ACT_RELU && l->act_storage) {
            for (int i = 0; i < mb * li_out; ++i) {
                if (((float*)l->a_post.data)[i] <= 0.0f)
                    dx[i] = 0.0f;
            }
        }

        float* layer_input;
        int in_feat;
        if (li > 0) {
            layer_input = (float*)vnet->layers[li - 1].a_post.data;
            in_feat = vnet->layers[li - 1].out_features;
        } else {
            layer_input = (float*)obs->data;
            in_feat = (int)obs->shape[1];
        }

        for (int of = 0; of < li_out; ++of) {
            for (int k = 0; k < in_feat; ++k) {
                float g = 0.0f;
                for (int i = 0; i < mb; ++i)
                    g += dx[i * li_out + of] * layer_input[i * in_feat + k];
                grad_w[of * in_feat + k] += g;
            }
        }

        if (li > 0) {
            float* new_dx = (float*)BEAR_ARENA_ALLOC(temp_arena, float, mb * in_feat);
            if (!new_dx) return -1;
            for (int i = 0; i < mb; ++i) {
                for (int k = 0; k < in_feat; ++k) {
                    float g = 0.0f;
                    for (int of = 0; of < li_out; ++of)
                        g += dx[i * li_out + of] * w[of * in_feat + k];
                    new_dx[i * in_feat + k] = g;
                }
            }
            dx = new_dx;
        }
    }

    return 0;
}

void bear_policy_zero_grad(BearPolicyNet* net) {
    if (!net || !net->layers) return;
    for (int i = 0; i < net->num_layers; ++i) {
        BearParam* p = net->layers[i].param;
        if (p && p->grad.data) {
            int n = (int)bear_tensor_numel(&p->grad);
            float* g = (float*)p->grad.data;
            for (int j = 0; j < n; ++j) g[j] = 0.0f;
        }
    }
}

void bear_value_zero_grad(BearValueNet* vnet) {
    if (!vnet || !vnet->layers) return;
    for (int i = 0; i < vnet->num_layers; ++i) {
        BearParam* p = vnet->layers[i].param;
        if (p && p->grad.data) {
            int n = (int)bear_tensor_numel(&p->grad);
            float* g = (float*)p->grad.data;
            for (int j = 0; j < n; ++j) g[j] = 0.0f;
        }
    }
}

