/*
 * bear_nn.c — PufferC/BearRL PolicyNet Implementation
 */

#include "bear_nn.h"
#include "bear_arena.h"
#include "bear_simd.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ═══════════════════════════════════════════════════════════════════
 * Network Creation
 * ═══════════════════════════════════════════════════════════════════ */

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
    net->num_layers = num_hid + 1;  /* hidden layers + actor head (no critic head) */

    net->layers = BEAR_ARENA_ALLOC(param_arena, BearLayer, net->num_layers);
    if (!net->layers) return -1;

    int prev = obs_dim;
    int layer_idx = 0;

    /* Hidden layers */
    for (int i = 0; i < num_hid; ++i) {
        BearLayer* l = &net->layers[layer_idx++];
        l->in_features = prev;
        l->out_features = hid_sizes[i];
        l->act = BEAR_ACT_RELU;
        l->param = BEAR_ARENA_ALLOC(param_arena, BearParam, 1);
        if (!l->param) return -1;
        if (bear_param_create(param_arena, l->param, hid_sizes[i], prev, "policy.hid") != 0) return -1;
        prev = hid_sizes[i];
    }

    /* Actor head */
    BearLayer* actor = &net->layers[layer_idx++];
    actor->in_features = prev;
    actor->out_features = act_dim;
    actor->act = BEAR_ACT_NONE;  /* logits for discrete, raw for continuous */
    actor->param = BEAR_ARENA_ALLOC(param_arena, BearParam, 1);
    if (!actor->param) return -1;
    if (bear_param_create(param_arena, actor->param, act_dim, prev, "policy.actor") != 0) return -1;

    net->hid_size = 0;  /* no recurrent state */
    return 0;
}

int bear_policy_create_mingru(BearPolicyNet* net, BearArena* param_arena,
                               int obs_dim, int act_dim, int act_discrete,
                               int hid_size) {
    fflush(stdout);
    if (!net || !param_arena || obs_dim <= 0 || act_dim <= 0 || hid_size <= 0) return -1;
    fflush(stdout);
    
    net->type = BEAR_NET_MINGU;
    net->obs_dim = obs_dim;
    net->act_dim = act_dim;
    net->act_discrete = act_discrete;
    net->hid_size = hid_size;
    net->param_arena = param_arena;

    /* Input projection: obs -> hid */
    net->num_layers = 2;  /* in_proj, actor (gru separate, no critic head) */
    net->layers = BEAR_ARENA_ALLOC(param_arena, BearLayer, net->num_layers);
    if (!net->layers) return -1;

    int layer_idx = 0;

    /* Input projection */
    BearLayer* in_proj = &net->layers[layer_idx++];
    in_proj->in_features = obs_dim;
    in_proj->out_features = hid_size;
    in_proj->act = BEAR_ACT_RELU;
    in_proj->param = BEAR_ARENA_ALLOC(param_arena, BearParam, 1);
    if (!in_proj->param) return -1;
    if (bear_param_create(param_arena, in_proj->param, hid_size, obs_dim, "policy.in_proj") != 0) return -1;

    /* Actor head: hid -> act_dim */
    BearLayer* actor = &net->layers[layer_idx++];
    actor->in_features = hid_size;
    actor->out_features = act_dim;
    actor->act = BEAR_ACT_NONE;
    actor->param = BEAR_ARENA_ALLOC(param_arena, BearParam, 1);
    if (!actor->param) return -1;
    if (bear_param_create(param_arena, actor->param, act_dim, hid_size, "policy.actor") != 0) return -1;

    /* GRU core (separate from layers array) */
    net->gru = BEAR_ARENA_ALLOC(param_arena, BearMinGRU, 1);
    if (!net->gru) return -1;
    if (bear_mingru_create(param_arena, net->gru, hid_size, hid_size) != 0) {
        return -1;
    }

    net->param_arena = param_arena;
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════
 * Forward Pass
 * ═══════════════════════════════════════════════════════════════════ */

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
            BearLayer* l = &net->layers[i];
            bear_tensor_create(temp_arena, &layer_out, (int64_t[]){batch, l->out_features}, 2, BEAR_DTYPE_F32, "layer_out");
            bear_mlp_layer(&x, &l->param->weight, NULL, &layer_out, l->act, temp_arena);
            x = layer_out;
        }

        /* Actor head: last layer */
        BearLayer* actor = &net->layers[net->num_layers - 1];
        bear_tensor_create(temp_arena, &layer_out, (int64_t[]){batch, act_dim}, 2, BEAR_DTYPE_F32, "actor_logits");
        bear_mlp_layer(&x, &actor->param->weight, &actor->param->bias, &layer_out, BEAR_ACT_NONE, temp_arena);

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
                int sampled = 0;
                float max_p = 0.0f;
                for (int a = 0; a < act_dim; ++a)
                    if (probs[b * act_dim + a] > max_p) { max_p = probs[b * act_dim + a]; sampled = a; }
                for (int a = 0; a < act_dim; ++a) probs[b * act_dim + a] = 0.0f;
                probs[b * act_dim + sampled] = 1.0f;
                ((float*)logprobs->data)[b] = logf(max_p + 1e-8f);
            }
        } else {
            memcpy(actions->data, layer_out.data, batch * act_dim * sizeof(float));
            for (int b = 0; b < batch; ++b) ((float*)logprobs->data)[b] = 0.0f;
        }

        /* Values: filled by caller via value network; zero here as placeholder */
        if (values) {
            memset(values->data, 0, batch * sizeof(float));
        }

    } else if (net->type == BEAR_NET_MINGU) {
        int hid = net->hid_size;

        /* Input projection: obs -> hid */
        BearTensor x_proj;
        bear_tensor_create(temp_arena, &x_proj, (int64_t[]){batch, hid}, 2, BEAR_DTYPE_F32, "x_proj");
        bear_mlp_layer(obs, &net->layers[0].param->weight, NULL, &x_proj, BEAR_ACT_RELU, temp_arena);

        /* GRU step */
        BearTensor h_next;
        bear_tensor_create(temp_arena, &h_next, (int64_t[]){batch, hid}, 2, BEAR_DTYPE_F32, "h_next");
        bear_mingru_step(net->gru, &x_proj, h_in, &h_next, temp_arena);

        if (h_out) *h_out = h_next;

        /* Actor head: hid -> act_dim */
        BearTensor actor_out;
        bear_tensor_create(temp_arena, &actor_out, (int64_t[]){batch, act_dim}, 2, BEAR_DTYPE_F32, "actor_logits");
        bear_mlp_layer(&h_next, &net->layers[1].param->weight, &net->layers[1].param->bias, &actor_out, BEAR_ACT_NONE, temp_arena);

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
                int sampled = 0; float max_p = 0.0f;
                for (int a = 0; a < act_dim; ++a)
                    if (probs[b * act_dim + a] > max_p) { max_p = probs[b * act_dim + a]; sampled = a; }
                for (int a = 0; a < act_dim; ++a) probs[b * act_dim + a] = 0.0f;
                probs[b * act_dim + sampled] = 1.0f;
                ((float*)logprobs->data)[b] = logf(max_p + 1e-8f);
            }
        } else {
            memcpy(actions->data, actor_out.data, batch * act_dim * sizeof(float));
            for (int b = 0; b < batch; ++b) ((float*)logprobs->data)[b] = 0.0f;
        }

        /* Values: filled by caller via value network */
        if (values) {
            memset(values->data, 0, batch * sizeof(float));
        }
    }
}

void bear_policy_sample(BearPolicyNet* net, BearTensor* actions, BearTensor* logprobs,
                         uint64_t* rng_state) {
    (void)rng_state;  /* placeholder - would use rng_state for sampling */
    int batch = (int)actions->shape[0];
    int act_dim = net->act_dim;

    if (net->act_discrete) {
        float* probs = (float*)actions->data;
        for (int b = 0; b < batch; ++b) {
            /* Categorical sampling via Gumbel-Max trick */
            float max_logit = -INFINITY;
            for (int a = 0; a < act_dim; ++a) {
                if (probs[b * act_dim + a] > max_logit) 
                    max_logit = probs[b * act_dim + a];
            }
            float sum_exp = 0.0f;
            for (int a = 0; a < act_dim; ++a)
                sum_exp += expf(probs[b * act_dim + a] - max_logit);
            for (int a = 0; a < act_dim; ++a)
                probs[b * act_dim + a] = expf(probs[b * act_dim + a] - max_logit) / sum_exp;
            
            /* Gumbel noise using rng_state (simplified) */
            float max_val = -INFINITY;
            int sampled = 0;
            for (int a = 0; a < act_dim; ++a) {
                float gumbel = -logf(-logf((float)rand() / RAND_MAX + 1e-8f));
                float score = logf(probs[b * act_dim + a] + 1e-8f) + gumbel;
                if (score > max_val) { max_val = score; sampled = a; }
            }
            
            for (int a = 0; a < act_dim; ++a) probs[b * act_dim + a] = 0;
            probs[b * act_dim + sampled] = 1.0f;
            ((float*)logprobs->data)[b] = logf(probs[b * act_dim + sampled] + 1e-8f);
        }
    } else {
        /* Continuous: add Gaussian noise to mean (placeholder) */
        for (int b = 0; b < batch; ++b) ((float*)logprobs->data)[b] = 0.0f;
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
    /* Continuous: actions already contain mean */
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
        fprintf(stderr, "  layer %d: param=%p weight.data=%p\n", i, (void*)l->param, (void*)l->param->weight.data);
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

/* Orthogonal initialization for value network (no GRU) */
void bear_value_orthogonal_init(BearValueNet* vnet, float gain) {
    for (int i = 0; i < vnet->num_layers; ++i) {
        BearLayer* l = &vnet->layers[i];
        bear_orthogonal_init(&l->param->weight, gain);
        if (l->param->bias.data) bear_tensor_fill(&l->param->bias, 0.0f);
    }
}

/* ══════════════════════════════════════════════════════════════════
 * Value Network
 * ═══════════════════════════════════════════════════════════════════ */

int bear_value_create(BearValueNet* vnet, BearArena* param_arena,
                       int obs_dim, const int* hid_sizes, int num_hid) {
    if (!vnet || !param_arena) return -1;
    
    vnet->param_arena = param_arena;
    vnet->num_layers = num_hid + 1;
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
        prev = hid_sizes[i];
    }
    
    /* Output head: prev -> 1 */
    BearLayer* out = &vnet->layers[vnet->num_layers - 1];
    out->in_features = prev;
    out->out_features = 1;
    out->act = BEAR_ACT_NONE;
    out->param = BEAR_ARENA_ALLOC(param_arena, BearParam, 1);
    if (!out->param) return -1;
    if (bear_param_create(param_arena, out->param, 1, prev, "value.out") != 0) return -1;
    
    return 0;
}

void bear_value_forward(const BearValueNet* vnet,
                         const BearTensor* obs,  /* [batch, obs_dim] */
                         BearTensor* values,     /* [batch] */
                         BearArena* temp_arena) {
    BearTensor x = *obs;
    BearTensor layer_out;
    int batch = (int)obs->shape[0];
    
    for (int i = 0; i < vnet->num_layers; ++i) {
        BearLayer* l = &vnet->layers[i];
        bear_tensor_create(temp_arena, &layer_out, (int64_t[]){batch, l->out_features}, 2, BEAR_DTYPE_F32, "value_layer");
        bear_mlp_layer(&x, &l->param->weight, &l->param->bias, &layer_out, l->act, temp_arena);
        x = layer_out;
    }
    
    float* v = (float*)x.data;
    for (int b = 0; b < batch; ++b) ((float*)values->data)[b] = v[b];
}

/* ═══════════════════════════════════════════════════════════════════
 * Checkpointing (stub)
 * ═══════════════════════════════════════════════════════════════════ */
int bear_checkpoint_save(const BearPolicyNet* net, const char* path) {
    (void)net; (void)path;
    return 0;  /* stub */
}

int bear_checkpoint_load(BearPolicyNet* net, const char* path) {
    (void)net; (void)path;
    return 0;  /* stub */
}