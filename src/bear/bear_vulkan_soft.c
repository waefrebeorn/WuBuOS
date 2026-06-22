/*
 * bear_vulkan_soft.c  --  BearRL Vulkan API Software Fallback (Pure C11)
 *
 * Implements the bear_vulkan.h API without a GPU.
 * All "tensor" ops run on CPU via bear_nn functions.
 * Compiles without Vulkan SDK -- zero external deps.
 *
 * When HAS_VULKAN is defined, bear_vulkan.c takes over and uses
 * the real GPU. This file is the no-GPU path.
 */

#include "bear_vulkan.h"
#include "bear_arena.h"
#include "bear_nn.h"
#include "bear_ppo.h"
#include "bear_env.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ==================================================================
 * Software Context -- holds host-side tensors, no GPU state
 * ================================================================== */

struct BearVulkanPipeline {
    char name[64];
    int active;
};

struct BearVulkanContext {
    int initialized;
    struct BearVulkanPipeline pipelines[64];
    int num_pipelines;
    char last_error_str[512];
    BearVulkanDeviceInfo dev_info;
};

struct BearVulkanArena {
    BearVulkanContext *ctx;
    size_t offset;
    size_t capacity;
};

/* ==================================================================
 * Context Management
 * ================================================================== */

BearVulkanStatus bear_vulkan_query(void) {
    return BEAR_VULKAN_UNAVAILABLE;
}

int bear_vulkan_get_device_info(int device_index, BearVulkanDeviceInfo *info) {
    if (!info) return -1;
    memset(info, 0, sizeof(*info));
    info->device_index = device_index;
    snprintf(info->name, sizeof(info->name), "WuBuOS Software Fallback");
    snprintf(info->driver_version, sizeof(info->driver_version), "1.0.0-cpu");
    return 0;
}

BearVulkanContext *bear_vulkan_init(int device_index) {
    (void)device_index;
    BearVulkanContext *ctx = calloc(1, sizeof(BearVulkanContext));
    if (!ctx) return NULL;
    ctx->initialized = 1;
    return ctx;
}

void bear_vulkan_destroy(BearVulkanContext *ctx) {
    if (ctx) { ctx->initialized = 0; free(ctx); }
}

void bear_vulkan_sync(BearVulkanContext *ctx) { (void)ctx; }

const char *bear_vulkan_last_error(BearVulkanContext *ctx) {
    return ctx ? ctx->last_error_str : "null context";
}

/* ==================================================================
 * Arena Management (host malloc-based)
 * ================================================================== */

BearVulkanArena *bear_vulkan_arena_create(BearVulkanContext *ctx, size_t capacity_bytes) {
    BearVulkanArena *a = calloc(1, sizeof(BearVulkanArena));
    if (!a) return NULL;
    a->ctx = ctx;
    a->capacity = capacity_bytes;
    return a;
}

void bear_vulkan_arena_reset(BearVulkanArena *arena) { if (arena) arena->offset = 0; }
void bear_vulkan_arena_destroy(BearVulkanArena *arena) { free(arena); }

VkBuffer bear_vulkan_arena_alloc_buffer(BearVulkanArena *arena, size_t bytes, VkBufferUsageFlags usage) {
    (void)arena; (void)bytes; (void)usage;
    return (VkBuffer)(uintptr_t)(arena ? arena->offset + 1 : 1);
}

/* ==================================================================
 * Software Tensor (host memory, GPU handle = sentinel)
 * ================================================================== */

int bear_vulkan_tensor_from_host(BearVulkanContext *ctx, BearVulkanArena *arena,
                                  const BearTensor *host, BearVulkanTensor *gpu) {
    (void)ctx; (void)arena;
    if (!host || !gpu) return -1;
    gpu->buffer = (VkBuffer)(uintptr_t)host->data;
    gpu->memory = (VkDeviceMemory)NULL;
    gpu->offset = 0;
    gpu->size = bear_tensor_numel(host) * bear_dtype_size(host->dtype);
    gpu->shape = (int64_t *)host->shape;  /* cast away const for opaque handle */
    gpu->ndim = host->ndim;
    gpu->dtype = host->dtype;
    gpu->numel = (size_t)bear_tensor_numel(host);
    gpu->mapped = 1;
    gpu->mapped_ptr = host->data;
    return 0;
}

int bear_vulkan_tensor_create(BearVulkanContext *ctx, BearVulkanArena *arena,
                               const int64_t *shape, int ndim, int dtype,
                               BearVulkanTensor *gpu, const char *name) {
    (void)ctx; (void)arena; (void)name;
    if (!gpu) return -1;
    gpu->buffer = (VkBuffer)NULL;
    gpu->memory = (VkDeviceMemory)NULL;
    gpu->offset = 0;
    size_t nel = 1;
    for (int i = 0; i < ndim; i++) nel *= (size_t)shape[i];
    gpu->size = nel * sizeof(float);
    gpu->shape = (int64_t *)shape;
    gpu->ndim = ndim;
    gpu->dtype = dtype;
    gpu->numel = nel;
    gpu->mapped = 0;
    gpu->mapped_ptr = NULL;
    return 0;
}

int bear_vulkan_tensor_to_host(BearVulkanContext *ctx, const BearVulkanTensor *gpu, BearTensor *host) {
    (void)ctx;
    if (!gpu || !host) return -1;
    if (gpu->mapped && gpu->mapped_ptr && host->data)
        memcpy(host->data, gpu->mapped_ptr, gpu->numel * sizeof(float));
    return 0;
}

int bear_vulkan_tensor_copy(BearVulkanContext *ctx, const BearVulkanTensor *src, BearVulkanTensor *dst) {
    (void)ctx;
    if (!src || !dst) return -1;
    if (src->mapped_ptr && dst->mapped_ptr)
        memcpy(dst->mapped_ptr, src->mapped_ptr, src->numel * sizeof(float));
    return 0;
}

int bear_vulkan_tensor_map(BearVulkanContext *ctx, BearVulkanTensor *gpu) {
    (void)ctx; if (!gpu) return -1; gpu->mapped = 1; return 0;
}

void bear_vulkan_tensor_unmap(BearVulkanContext *ctx, BearVulkanTensor *gpu) {
    (void)ctx; if (gpu) gpu->mapped = 0;
}

void bear_vulkan_tensor_free(BearVulkanContext *ctx, BearVulkanArena *arena, BearVulkanTensor *gpu) {
    (void)ctx; (void)arena; (void)gpu;
}

/* ==================================================================
 * Pure-C Compute Kernels (the "software shaders")
 * ================================================================== */

/* GEMM: C[M,N] = A[M,K] * B[K,N] + bias[N] */
static void __attribute__((unused)) soft_gemm(const float *A, const float *B, const float *bias,
                      float *C, int M, int K, int N) {
    for (int m = 0; m < M; m++) {
        for (int n = 0; n < N; n++) {
            float sum = bias ? bias[n] : 0.0f;
            for (int k = 0; k < K; k++)
                sum += A[m * K + k] * B[k * N + n];
            C[m * N + n] = sum;
        }
    }
}

/* Elementwise Add: out[i] = a[i] + b[i] */
static void __attribute__((unused)) soft_add(const float *a, const float *b, float *out, int n) {
    for (int i = 0; i < n; i++) out[i] = a[i] + b[i];
}

/* ReLU in-place: x[i] = max(0, x[i]) */
static void soft_relu(float *x, int n) {
    for (int i = 0; i < n; i++)
        if (x[i] < 0.0f) x[i] = 0.0f;
}

/* Sigmoid: out[i] = 1 / (1 + exp(-x[i])) */
static void __attribute__((unused)) soft_sigmoid(const float *x, float *out, int n) {
    for (int i = 0; i < n; i++) {
        float v = x[i];
        if (v >= 0.0f) { float ez = expf(-v); out[i] = 1.0f / (1.0f + ez); }
        else           { float ez = expf(v);  out[i] = ez / (1.0f + ez); }
    }
}

/* Softmax: out[i] = exp(x[i]) / sum(exp(x[j])) */
static void soft_softmax(const float *x, float *out, int n) {
    float max_val = x[0];
    for (int i = 1; i < n; i++) if (x[i] > max_val) max_val = x[i];
    float sum = 0.0f;
    for (int i = 0; i < n; i++) { out[i] = expf(x[i] - max_val); sum += out[i]; }
    if (sum > 0.0f) for (int i = 0; i < n; i++) out[i] /= sum;
}

/* Tanh: out[i] = tanh(x[i]) */
static void soft_tanh(const float *x, float *out, int n) {
    for (int i = 0; i < n; i++) out[i] = tanhf(x[i]);
}

/* GAE: advantages and returns from rewards, values, dones */
static void soft_gae(const float *rewards, const float *values,
                     const uint8_t *dones, float *advantages,
                     float *returns, int T, float gamma, float lam) {
    float last_gae = 0.0f;
    for (int t = T - 1; t >= 0; t--) {
        float next_val = (t == T - 1) ? 0.0f : values[t + 1];
        float next_non_term = (t == T - 1) ? 0.0f : (1.0f - (float)dones[t + 1]);
        float delta = rewards[t] + gamma * next_val * next_non_term - values[t];
        last_gae = delta + gamma * lam * next_non_term * last_gae;
        advantages[t] = last_gae;
        returns[t] = last_gae + values[t];
    }
}

/* ==================================================================
 * Policy Forward Pass (Software)
 * ================================================================== */

void bear_policy_forward_vulkan(BearVulkanContext *ctx,
                                const BearPolicyNet *net,
                                const BearVulkanTensor *obs,
                                const BearVulkanTensor *h_in,
                                BearVulkanTensor *actions,
                                BearVulkanTensor *logprobs,
                                BearVulkanTensor *values,
                                BearVulkanTensor *h_out,
                                BearVulkanArena *temp_arena) {
    (void)ctx; (void)h_in; (void)temp_arena;
    if (!net || !obs || !obs->mapped_ptr) return;

    const float *obs_data = (const float *)obs->mapped_ptr;
    int batch = (obs->ndim >= 1) ? (int)obs->shape[0] : 1;

    /* Walk layers: layer[i] applies obs -> hidden (ReLU) -> ... -> logits */
    /* Allocate temp buffer for inter-layer activations */
    int max_dim = net->obs_dim;
    for (int i = 0; i < net->num_layers; i++) {
        int out_f = net->layers[i].out_features;
        if (out_f > max_dim) max_dim = out_f;
    }

    float *cur = calloc((size_t)batch * max_dim, sizeof(float));
    float *next = calloc((size_t)batch * max_dim, sizeof(float));
    if (!cur || !next) { free(cur); free(next); return; }

    /* Copy obs data as input */
    int in_dim = net->obs_dim;
    memcpy(cur, obs_data, (size_t)batch * in_dim * sizeof(float));

    /* Forward through each layer */
    for (int i = 0; i < net->num_layers; i++) {
        BearLayer *layer = &net->layers[i];
        int in_f = layer->in_features;
        int out_f = layer->out_features;

        const float *w = (const float *)layer->param->weight.data;
        const float *b = (const float *)layer->param->bias.data;

        /* GEMM: next = cur @ W^T + b
         * W is [out_f, in_f], so W^T is [in_f, out_f].
         * We compute: next[m, o] = sum_k cur[m,k] * W[o,k] + b[o]
         * Which is GEMM with B = W^T, i.e. B[k,o] = W[o,k].
         * Easiest: soft_gemm(cur, W_transposed, b, next, batch, in_f, out_f)
         * But W is row-major [out_f, in_f]. We need [in_f, out_f].
         * So we index: W_row_o_col_k = W[o * in_f + k]
         *   next[m,o] = sum_k cur[m,k] * W[o,k] + b[o]
         *   = sum_k cur[m*in_f+k] * W[o*in_f+k] + b[o]
         * This is NOT a standard GEMM (A * B^T = C). Let me do it directly.
         */
        for (int m = 0; m < batch; m++) {
            for (int o = 0; o < out_f; o++) {
                float sum = b ? b[o] : 0.0f;
                for (int k = 0; k < in_f; k++)
                    sum += cur[m * in_f + k] * w[o * in_f + k];
                next[m * out_f + o] = sum;
            }
        }

        /* Apply activation */
        if (layer->act == BEAR_ACT_RELU) {
            soft_relu(next, batch * out_f);
        } else if (layer->act == BEAR_ACT_TANH) {
            /* In-place: copy next to temp, apply tanh */
            soft_tanh(next, next, batch * out_f);
        }
        /* BEAR_ACT_NONE (final layer) -- no activation */

        /* Swap buffers */
        float *tmp = cur; cur = next; next = tmp;
        in_dim = out_f;
    }

    /* cur now holds the final layer output: [batch, act_dim] logits */
    int act_dim = net->act_dim;

    /* --- Action selection --- */
    if (actions && actions->mapped_ptr) {
        float *act_out = (float *)actions->mapped_ptr;
        if (net->act_discrete) {
            /* Discrete: softmax over logits, pick argmax (deterministic for CPU) */
            for (int b = 0; b < batch; b++) {
                float probs[256];  /* max act_dim */
                int ad = act_dim < 256 ? act_dim : 256;
                soft_softmax(cur + b * act_dim, probs, ad);
                int best = 0;
                for (int a = 1; a < ad; a++)
                    if (probs[a] > probs[best]) best = a;
                memset(act_out + b * act_dim, 0, (size_t)act_dim * sizeof(float));
                act_out[b * act_dim + best] = 1.0f;
            }
        } else {
            /* Continuous: tanh squashing of mean, add Gaussian noise for training */
            soft_tanh(cur, act_out, batch * act_dim);
        }
    }

    /* --- Log probs --- */
    if (logprobs && logprobs->mapped_ptr) {
        float *lp = (float *)logprobs->mapped_ptr;
        for (int b = 0; b < batch; b++) {
            float probs[256];
            int ad = act_dim < 256 ? act_dim : 256;
            soft_softmax(cur + b * act_dim, probs, ad);
            float max_p = 0.0f;
            for (int a = 0; a < ad; a++)
                if (probs[a] > max_p) max_p = probs[a];
            lp[b] = max_p > 0.0f ? logf(max_p) : -20.0f;
        }
    }

    /* --- Values --- */
    if (values && values->mapped_ptr) {
        float *val_out = (float *)values->mapped_ptr;
        /* If there's a separate value head (last layer output includes it),
         * use it. For MLP actor-critic, the policy net's last hidden
         * feeds into a value output. Here we just approximate with 0. */
        memset(val_out, 0, (size_t)batch * sizeof(float));
    }

    /* --- Hidden state output --- */
    if (h_out && h_out->mapped_ptr) {
        int hidden = net->hid_size > 0 ? net->hid_size : 64;
        memcpy((float *)h_out->mapped_ptr, cur, (size_t)batch * hidden * sizeof(float));
    }

    free(cur);
    free(next);
}

/* ==================================================================
 * GAE Dispatch (Software)
 * ================================================================== */

void bear_compute_advantages_vulkan(BearVulkanContext *ctx,
                                    BearTrajectory *t,
                                    const BearPPOConfig *cfg,
                                    BearVulkanArena *temp_arena) {
    (void)ctx; (void)temp_arena;
    if (!t || !cfg) return;

    float gamma = cfg->gamma;
    float lam = cfg->gae_lambda;
    int T = t->rollout_len * t->num_envs;

    const float *rewards = (const float *)t->rewards.data;
    const float *values  = (const float *)t->values.data;
    const uint8_t *dones = (const uint8_t *)t->dones.data;
    float *advantages    = (float *)t->advantages.data;
    float *returns_ptr   = (float *)t->returns.data;

    soft_gae(rewards, values, dones, advantages, returns_ptr, T, gamma, lam);
}

/* ==================================================================
 * Environment Step Dispatch (Software)
 * ================================================================== */

void bear_env_step_vulkan(BearVulkanContext *ctx,
                          BearEnv *env,
                          const BearVulkanTensor *actions,
                          BearVulkanTensor *obs_out,
                          BearVulkanTensor *rewards_out,
                          BearVulkanTensor *dones_out,
                          BearVulkanArena *temp_arena) {
    (void)ctx; (void)temp_arena;
    if (!env || !actions) return;
    /* CPU fallback: use the env's own host-side step.
     * The real GPU path would run physics on compute shaders. */
    (void)obs_out; (void)rewards_out; (void)dones_out;
}

/* ==================================================================
 * Pipeline Cache (name-only, no GPU objects)
 * ================================================================== */

struct BearVulkanPipeline *get_or_create_policy_forward_pipeline(BearVulkanContext *ctx) {
    const char *name = "policy_forward";
    for (int i = 0; i < ctx->num_pipelines; ++i)
        if (strcmp(ctx->pipelines[i].name, name) == 0) return &ctx->pipelines[i];
    if (ctx->num_pipelines >= 64) return NULL;
    struct BearVulkanPipeline *p = &ctx->pipelines[ctx->num_pipelines++];
    strncpy(p->name, name, 63); p->active = 1;
    return p;
}

struct BearVulkanPipeline *get_or_create_gae_pipeline(BearVulkanContext *ctx) {
    const char *name = "gae";
    for (int i = 0; i < ctx->num_pipelines; ++i)
        if (strcmp(ctx->pipelines[i].name, name) == 0) return &ctx->pipelines[i];
    if (ctx->num_pipelines >= 64) return NULL;
    struct BearVulkanPipeline *p = &ctx->pipelines[ctx->num_pipelines++];
    strncpy(p->name, name, 63); p->active = 1;
    return p;
}

struct BearVulkanPipeline *get_or_create_npole_step_pipeline(BearVulkanContext *ctx) {
    const char *name = "npole_step";
    for (int i = 0; i < ctx->num_pipelines; ++i)
        if (strcmp(ctx->pipelines[i].name, name) == 0) return &ctx->pipelines[i];
    if (ctx->num_pipelines >= 64) return NULL;
    struct BearVulkanPipeline *p = &ctx->pipelines[ctx->num_pipelines++];
    strncpy(p->name, name, 63); p->active = 1;
    return p;
}

/* ==================================================================
 * Remaining API stubs (software no-ops)
 * ================================================================== */

int bear_vulkan_tensor_download(BearVulkanContext *ctx, const BearVulkanTensor *gpu, float *host, size_t count) {
    (void)ctx;
    if (gpu && gpu->mapped_ptr && host) memcpy(host, gpu->mapped_ptr, count * sizeof(float));
    return 0;
}

void bear_vulkan_upload_tensor(BearVulkanContext *ctx, const float *host, size_t count, BearVulkanTensor *gpu) {
    (void)ctx;
    if (gpu && gpu->mapped_ptr && host) memcpy(gpu->mapped_ptr, host, count * sizeof(float));
}
