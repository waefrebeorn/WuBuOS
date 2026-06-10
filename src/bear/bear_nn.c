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
    net->num_layers = num_hid + 2;  /* hidden layers + actor head + critic head */
    
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
    
    /* Critic head */
    BearLayer* critic = &net->layers[layer_idx++];
    critic->in_features = prev;
    critic->out_features = 1;
    critic->act = BEAR_ACT_NONE;
    critic->param = BEAR_ARENA_ALLOC(param_arena, BearParam, 1);
    if (!critic->param) return -1;
    if (bear_param_create(param_arena, critic->param, 1, prev, "policy.critic") != 0) return -1;
    
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
    fflush(stdout);
    
    /* Input projection: obs -> hid */
    net->num_layers = 3;  /* in_proj, actor, critic (gru separate) */
    fflush(stdout);
    net->layers = BEAR_ARENA_ALLOC(param_arena, BearLayer, net->num_layers);
    fflush(stdout);
    if (!net->layers) return -1;
    
    int layer_idx = 0;
    
    /* Input projection */
    fflush(stdout);
    BearLayer* in_proj = &net->layers[layer_idx++];
    in_proj->in_features = obs_dim;
    in_proj->out_features = hid_size;
    in_proj->act = BEAR_ACT_RELU;
    in_proj->param = BEAR_ARENA_ALLOC(param_arena, BearParam, 1);
    fflush(stdout);
    if (!in_proj->param) return -1;
    fflush(stdout);
    if (bear_param_create(param_arena, in_proj->param, hid_size, obs_dim, "policy.in_proj") != 0) return -1;
    fflush(stdout);
    
    /* Actor head: hid -> act_dim */
    fflush(stdout);
    BearLayer* actor = &net->layers[layer_idx++];
    actor->in_features = hid_size;
    actor->out_features = act_dim;
    actor->act = BEAR_ACT_NONE;
    actor->param = BEAR_ARENA_ALLOC(param_arena, BearParam, 1);
    if (!actor->param) return -1;
    if (bear_param_create(param_arena, actor->param, act_dim, hid_size, "policy.actor") != 0) return -1;
    
    /* Critic head: hid -> 1 */
    fflush(stdout);
    BearLayer* critic = &net->layers[layer_idx++];
    critic->in_features = hid_size;
    critic->out_features = 1;
    critic->act = BEAR_ACT_NONE;
    critic->param = BEAR_ARENA_ALLOC(param_arena, BearParam, 1);
    if (!critic->param) return -1;
    if (bear_param_create(param_arena, critic->param, 1, hid_size, "policy.critic") != 0) return -1;
    fflush(stdout);
    
    /* GRU core (separate from layers array) */
    fflush(stdout);
    net->gru = BEAR_ARENA_ALLOC(param_arena, BearMinGRU, 1);
    fflush(stdout);
    if (!net->gru) return -1;
    fflush(stdout);
    if (bear_mingru_create(param_arena, net->gru, hid_size, hid_size) != 0) {
        fflush(stdout);
        return -1;
    }
    fflush(stdout);
    
    net->param_arena = param_arena;
    fflush(stdout);
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════
 * Forward Pass
 * ═══════════════════════════════════════════════════════════════════ */

void bear_policy_forward(const BearPolicyNet* net,
                          const BearTensor* obs,        /* [batch, obs_dim] */
                          const BearTensor* h_in,       /* [batch, hid] (optional) */
                          BearTensor* actions,          /* [batch, act_dim] */
                          BearTensor* logprobs,         /* [batch] */
                          BearTensor* values,           /* [batch] */
                          BearTensor* h_out,            /* [batch, hid] (optional) */
                          BearArena* temp_arena) {
    fflush(stdout);
    int batch = (int)obs->shape[0];
    int act_dim = net->act_dim;
    fflush(stdout);
    
    BearTensor x = *obs;  /* current tensor */
    BearTensor layer_out;
    
    fprintf(stderr, "FWD enter type=%d batch=%d\n", net->type, batch);
    fflush(stderr);
    // Check all weights for NaN at entry
    for (int li = 0; li < net->num_layers; ++li) {
        float* wp = (float*)net->layers[li].param->weight.data;
        int wn = (int)(net->layers[li].param->weight.shape[0] * net->layers[li].param->weight.shape[1]);
        for (int wj = 0; wj < wn; ++wj) {
            if (wp[wj] != wp[wj]) {
                char buf[128];
                snprintf(buf, sizeof(buf), "NAN in weight layer %d at j=%d BEFORE forward\n", li, wj);
                write(2, buf, strlen(buf));
                break;
            }
        }
    }
    fflush(stderr);
    if (net->type == BEAR_NET_MLP) {
        fprintf(stderr, "MLP path\n"); fflush(stderr);
        fflush(stdout);
        /* MLP forward: hidden layers */
        for (int i = 0; i < net->num_layers - 2; ++i) {
            fflush(stdout);
            BearLayer* l = &net->layers[i];
            bear_tensor_create(temp_arena, &layer_out, (int64_t[]){batch, l->out_features}, 2, BEAR_DTYPE_F32, "layer_out");
            fflush(stdout);
            bear_mlp_layer(&x, &l->param->weight, NULL, &layer_out, l->act, temp_arena);
            // NaN check
            {
                float* lp = (float*)layer_out.data;
                for (int ii = 0; ii < batch * l->out_features; ++ii) {
                    if (lp[ii] != lp[ii]) { fprintf(stderr, "NAN after layer %d\n", i); fflush(stderr); break; }
                }
            }
            x = layer_out;
        }
        
        /* Actor head */
        fflush(stdout);
        BearLayer* actor = &net->layers[net->num_layers - 2];
        bear_tensor_create(temp_arena, &layer_out, (int64_t[]){batch, act_dim}, 2, BEAR_DTYPE_F32, "actor_logits");
        bear_mlp_layer(&x, &actor->param->weight, &actor->param->bias, &layer_out, BEAR_ACT_NONE, temp_arena);
        
        if (net->act_discrete) {
            /* Discrete: Softmax over actions */
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
                /* Deterministic argmax for now (sampling in bear_policy_sample) */
                int sampled = 0;
                float max_p = 0.0f;
                for (int a = 0; a < act_dim; ++a)
                    if (probs[b * act_dim + a] > max_p) { max_p = probs[b * act_dim + a]; sampled = a; }
                for (int a = 0; a < act_dim; ++a) probs[b * act_dim + a] = 0.0f;
                probs[b * act_dim + sampled] = 1.0f;
                ((float*)logprobs->data)[b] = logf(max_p + 1e-8f);
            }
        } else {
            /* Continuous: use mean directly, logprob placeholder */
            { char buf[128]; snprintf(buf, sizeof(buf), "CONTINUOUS: actions->data=%p layer_out.data=%p batch=%d\n", (void*)actions->data, (void*)layer_out.data, batch); write(2, buf, strlen(buf)); }
            memcpy(actions->data, layer_out.data, batch * act_dim * sizeof(float));
            for (int b = 0; b < batch; ++b) ((float*)logprobs->data)[b] = 0.0f;
        }
        { char buf[128]; snprintf(buf, sizeof(buf), "PAST CONTINUOUS\n"); write(2, buf, strlen(buf)); }
        
        /* Critic head */
        { char buf[256]; snprintf(buf, sizeof(buf), "CRITIC HEAD x.data=%p x.shape=[%ld,%ld] batch=%d\n", (void*)x.data, x.shape[0], x.shape[1], batch); write(2, buf, strlen(buf)); }
        // Check x for NaN
        {
            float* xp = (float*)x.data;
            int nan_found = 0;
            for (int ii = 0; ii < batch * 64; ++ii) {
                if (xp[ii] != xp[ii]) { nan_found = 1; break; }
            }
            { char buf[64]; snprintf(buf, sizeof(buf), "x NaN check: %s\n", nan_found ? "NAN FOUND" : "OK"); write(2, buf, strlen(buf)); }
        }
        fflush(stdout);
        BearLayer* critic = &net->layers[net->num_layers - 1];
        bear_tensor_create(temp_arena, &layer_out, (int64_t[]){batch, 1}, 2, BEAR_DTYPE_F32, "value");
        bear_mlp_layer(&x, &critic->param->weight, &critic->param->bias, &layer_out, BEAR_ACT_NONE, temp_arena);
        { char buf[256]; 
          float* w = (float*)critic->param->weight.data;
          float* lo = (float*)layer_out.data;
          snprintf(buf, sizeof(buf), "CRITIC GEMM DONE: w=%p w[0]=%f lo=%p lo[0]=%f lo[511]=%f\n", (void*)w, w[0], (void*)lo, lo[0], lo[511]);
          write(2, buf, strlen(buf)); }
        float* v = (float*)layer_out.data;
        for (int b = 0; b < batch; ++b) ((float*)values->data)[b] = v[b];
        
    } else if (net->type == BEAR_NET_MINGU) {
        fflush(stdout);
        /* MinGRU recurrent forward */
        int hid = net->hid_size;
        
        /* Input projection: obs -> hid */
        fflush(stdout);
        BearTensor x_proj;
        bear_tensor_create(temp_arena, &x_proj, (int64_t[]){batch, hid}, 2, BEAR_DTYPE_F32, "x_proj");
        bear_mlp_layer(obs, &net->layers[0].param->weight, NULL, &x_proj, BEAR_ACT_RELU, temp_arena);
        fflush(stdout);
        
        /* GRU step */
        fflush(stdout);
        BearTensor h_next;
        bear_tensor_create(temp_arena, &h_next, (int64_t[]){batch, hid}, 2, BEAR_DTYPE_F32, "h_next");
        bear_mingru_step(net->gru, &x_proj, h_in, &h_next, temp_arena);
        fflush(stdout);
        
        if (h_out) *h_out = h_next;
        
        /* Actor head: hid -> act_dim */
        fflush(stdout);
        BearTensor actor_out;
        bear_tensor_create(temp_arena, &actor_out, (int64_t[]){batch, act_dim}, 2, BEAR_DTYPE_F32, "actor_logits");
        fflush(stdout);
        bear_mlp_layer(&h_next, &net->layers[1].param->weight, &net->layers[1].param->bias, &actor_out, BEAR_ACT_NONE, temp_arena);
        fflush(stdout);
        
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
        
        /* Critic head: hid -> 1 */
        fflush(stdout);
        BearTensor critic_out;
        bear_tensor_create(temp_arena, &critic_out, (int64_t[]){batch, 1}, 2, BEAR_DTYPE_F32, "value");
        bear_mlp_layer(&h_next, &net->layers[2].param->weight, &net->layers[2].param->bias, &critic_out, BEAR_ACT_NONE, temp_arena);
        float* vc = (float*)critic_out.data;
        for (int b = 0; b < batch; ++b) ((float*)values->data)[b] = vc[b];
    }
    fflush(stdout);
}/* End of MinGRU block */

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
        /* GRU params */
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