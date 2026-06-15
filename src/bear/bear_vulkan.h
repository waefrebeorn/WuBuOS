/*
 * bear_vulkan.h  --  BearRL Vulkan Compute Shader Plugin Interface
 *
 * Provides a clean abstraction for GPU acceleration via Vulkan compute shaders.
 * Mirrors the CUDA interface for unified dispatch.
 * Pure C11 host code + GLSL compute shaders in bear_vulkan_shaders/
 */

#ifndef BEAR_VULKAN_H
#define BEAR_VULKAN_H

#include "bear_arena.h"
#include "bear_nn.h"
#include "bear_ppo.h"
#include "bear_env.h"
#include "bear_opt.h"
#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===================================================================
 * Vulkan Backend Capability Query
 * =================================================================== */

typedef enum {
    BEAR_VULKAN_UNAVAILABLE = 0,  /* No Vulkan loader / no compatible GPU */
    BEAR_VULKAN_AVAILABLE   = 1,  /* Vulkan instance + GPU with compute */
    BEAR_VULKAN_ACTIVE      = 2,  /* Context initialized, pipelines ready */
} BearVulkanStatus;

/* Device properties */
typedef struct {
    int device_index;
    char name[256];
    char driver_version[64];
    uint32_t api_version;
    uint32_t vendor_id;
    uint32_t device_id;
    size_t max_compute_workgroup_count[3];
    uint32_t max_compute_workgroup_size[3];
    uint32_t max_compute_workgroup_invocations;
    size_t max_storage_buffer_range;
    size_t max_uniform_buffer_range;
    size_t max_push_constants_size;
    uint32_t subgroup_size;
    int has_subgroup_ops;
    int has_shader_float16;
    int has_shader_float64;
} BearVulkanDeviceInfo;

/* Query Vulkan availability */
BearVulkanStatus bear_vulkan_query(void);

/* Get device info (requires active context) */
int bear_vulkan_get_device_info(int device_index, BearVulkanDeviceInfo* info);

/* ===================================================================
 * Vulkan Context Management
 * =================================================================== */

typedef struct BearVulkanContext BearVulkanContext;

/* Initialize Vulkan context (must be called before any GPU ops) */
/* device_index = -1 for auto-select best device with compute queue */
BearVulkanContext* bear_vulkan_init(int device_index);

/* Destroy Vulkan context */
void bear_vulkan_destroy(BearVulkanContext* ctx);

/* Synchronize all GPU work */
void bear_vulkan_sync(BearVulkanContext* ctx);

/* Get last Vulkan error string */
const char* bear_vulkan_last_error(BearVulkanContext* ctx);

/* ===================================================================
 * GPU Memory Management (Arena-like interface)
 * =================================================================== */

typedef struct BearVulkanArena BearVulkanArena;

/* Create Vulkan arena for temporary allocations (per-step/per-minibatch) */
BearVulkanArena* bear_vulkan_arena_create(BearVulkanContext* ctx, size_t capacity_bytes);

/* Reset Vulkan arena (free all temp allocations, keep capacity) */
void bear_vulkan_arena_reset(BearVulkanArena* arena);

/* Destroy Vulkan arena */
void bear_vulkan_arena_destroy(BearVulkanArena* arena);

/* Allocate device memory from Vulkan arena (returns buffer handle) */
VkBuffer bear_vulkan_arena_alloc_buffer(BearVulkanArena* arena, size_t bytes, VkBufferUsageFlags usage);

/* ===================================================================
 * GPU Tensor Abstraction (SoA, device buffers)
 * =================================================================== */

typedef struct {
    VkBuffer buffer;              /* Vulkan buffer handle */
    VkDeviceMemory memory;        /* Backing memory */
    VkDeviceSize offset;          /* Offset into buffer */
    VkDeviceSize size;            /* Size in bytes */
    int64_t* shape;               /* Host array of shape dims */
    int ndim;                     /* Number of dimensions */
    int dtype;                    /* BEAR_DTYPE_F32, etc. */
    size_t numel;                 /* Total elements */
    int mapped;                   /* Whether host-mapped */
    void* mapped_ptr;             /* Host pointer if mapped */
} BearVulkanTensor;

/* Create Vulkan tensor from existing host tensor (uploads) */
int bear_vulkan_tensor_from_host(BearVulkanContext* ctx, BearVulkanArena* arena,
                                  const BearTensor* host, BearVulkanTensor* gpu);

/* Create empty Vulkan tensor (allocates on device) */
int bear_vulkan_tensor_create(BearVulkanContext* ctx, BearVulkanArena* arena,
                               const int64_t* shape, int ndim, int dtype,
                               BearVulkanTensor* gpu, const char* name);

/* Download Vulkan tensor to host */
int bear_vulkan_tensor_to_host(BearVulkanContext* ctx, const BearVulkanTensor* gpu, BearTensor* host);

/* Copy between Vulkan tensors (device-to-device via compute shader) */
int bear_vulkan_tensor_copy(BearVulkanContext* ctx, const BearVulkanTensor* src, BearVulkanTensor* dst);

/* Map tensor for host access (persistent mapping) */
int bear_vulkan_tensor_map(BearVulkanContext* ctx, BearVulkanTensor* gpu);

/* Unmap tensor */
void bear_vulkan_tensor_unmap(BearVulkanContext* ctx, BearVulkanTensor* gpu);

/* Free Vulkan tensor */
void bear_vulkan_tensor_free(BearVulkanContext* ctx, BearVulkanArena* arena, BearVulkanTensor* gpu);

/* ===================================================================
 * Accelerated Operations  --  Policy Network (Forward/Backward)
 * =================================================================== */

/* Forward pass: obs -> (actions, logprobs, values, h_out) */
/* Automatically selects GPU if available and batch size >= threshold */
void bear_policy_forward_vulkan(BearVulkanContext* ctx,
                                const BearPolicyNet* net,
                                const BearVulkanTensor* obs,
                                const BearVulkanTensor* h_in,
                                BearVulkanTensor* actions,
                                BearVulkanTensor* logprobs,
                                BearVulkanTensor* values,
                                BearVulkanTensor* h_out,
                                BearVulkanArena* temp_arena);

/* Backward pass for discrete actions */
int bear_policy_backward_discrete_vulkan(BearVulkanContext* ctx,
                                          BearPolicyNet* net,
                                          const BearVulkanTensor* obs,
                                          const BearVulkanTensor* actions,
                                          const BearVulkanTensor* old_logprobs,
                                          const BearVulkanTensor* advantages,
                                          float clip_coef,
                                          float policy_grad_scale,
                                          BearVulkanArena* temp_arena);

/* Backward pass for continuous (Gaussian) actions */
int bear_policy_backward_continuous_vulkan(BearVulkanContext* ctx,
                                            BearPolicyNet* net,
                                            const BearVulkanTensor* obs,
                                            const BearVulkanTensor* actions,
                                            const BearVulkanTensor* old_logprobs,
                                            const BearVulkanTensor* advantages,
                                            float clip_coef,
                                            float policy_grad_scale,
                                            BearVulkanArena* temp_arena);

/* MinGRU step (recurrent core) */
void bear_mingru_step_vulkan(BearVulkanContext* ctx,
                              const BearMinGRU* gru,
                              const BearVulkanTensor* x,
                              const BearVulkanTensor* h_in,
                              BearVulkanTensor* h_out,
                              BearVulkanArena* temp_arena);

/* ===================================================================
 * Accelerated Operations  --  Value Network
 * =================================================================== */

void bear_value_forward_vulkan(BearVulkanContext* ctx,
                                const BearValueNet* vnet,
                                const BearVulkanTensor* obs,
                                BearVulkanTensor* values,
                                BearVulkanArena* temp_arena);

int bear_value_backward_vulkan(BearVulkanContext* ctx,
                                BearValueNet* vnet,
                                const BearVulkanTensor* obs,
                                const BearVulkanTensor* values,
                                const BearVulkanTensor* targets,
                                float vf_coef,
                                BearVulkanArena* temp_arena);

/* ===================================================================
 * Accelerated Operations  --  PPO Training Loop
 * =================================================================== */

/* GAE advantage computation on GPU */
void bear_compute_advantages_vulkan(BearVulkanContext* ctx,
                                     BearTrajectory* t,
                                     const BearPPOConfig* cfg,
                                     BearVulkanArena* temp_arena);

/* V-Trace on GPU */
void bear_vtrace_compute_vulkan(BearVulkanContext* ctx,
                                 const float* rewards,
                                 const uint8_t* dones,
                                 const float* values,
                                 const float* logprobs,
                                 const float* target_logprobs,
                                 float* advantages,
                                 float* returns,
                                 int T, int B, const BearPPOConfig* cfg,
                                 BearVulkanArena* temp_arena);

/* Minibatch sampler (shuffle + gather) on GPU */
int bear_sampler_next_vulkan(BearVulkanContext* ctx,
                              BearMinibatchSampler* s,
                              BearTrajectory* t,
                              BearVulkanTensor* mb_obs,
                              BearVulkanTensor* mb_actions,
                              BearVulkanTensor* mb_logprobs,
                              BearVulkanTensor* mb_advantages,
                              BearVulkanTensor* mb_returns,
                              BearVulkanTensor* mb_values,
                              BearVulkanTensor* mb_old_logprobs,
                              BearVulkanArena* temp_arena);

/* PPO loss computation on GPU */
BearPPOLoss bear_ppo_loss_vulkan(BearVulkanContext* ctx,
                                  const BearPolicyNet* policy,
                                  const BearValueNet* critic,
                                  const BearVulkanTensor* obs,
                                  const BearVulkanTensor* actions,
                                  const BearVulkanTensor* old_logprobs,
                                  const BearVulkanTensor* advantages,
                                  const BearVulkanTensor* returns,
                                  const BearVulkanTensor* old_values,
                                  const BearPPOConfig* cfg,
                                  BearVulkanArena* temp_arena);

/* Apply gradients via Adam on GPU */
void bear_ppo_apply_gradients_vulkan(BearVulkanContext* ctx,
                                      BearPolicyNet* policy,
                                      BearValueNet* critic,
                                      BearOptimizer* opt_policy,
                                      BearOptimizer* opt_critic);

/* Gradient clipping on GPU */
float bear_clip_grad_norm_vulkan(BearVulkanContext* ctx,
                                  BearPolicyNet* policy,
                                  BearValueNet* critic,
                                  float max_norm);

/* ===================================================================
 * Accelerated Operations  --  Environment (Vectorized Physics)
 * =================================================================== */

/* GPU Environment state for N-pole cartpole */
typedef struct {
    int n_poles;
    int num_envs;
    
    /* Physics parameters (constant per pole) */
    VkBuffer pole_mass;       /* [n_poles] */
    VkBuffer pole_length;     /* [n_poles] */
    VkBuffer pole_com;        /* [n_poles] */
    VkBuffer pole_inertia;    /* [n_poles] */
    
    /* Per-environment state */
    VkBuffer x;               /* [num_envs] cart position */
    VkBuffer x_dot;           /* [num_envs] cart velocity */
    VkBuffer theta;           /* [num_envs, n_poles] pole angles */
    VkBuffer theta_dot;       /* [num_envs, n_poles] pole angular velocities */
    
    /* Force applied this step */
    VkBuffer force;           /* [num_envs] */
    
    /* Step outputs */
    VkBuffer reward;          /* [num_envs] */
    VkBuffer done;            /* [num_envs] uint8_t */
    
    /* Constants */
    float gravity;
    float cart_mass;
    float dt;
    float force_mag;
    float angle_threshold;    /* 12 deg = 0.20944 rad */
    float pos_threshold;      /* 2.4 m */
} BearVulkanEnvState;

/* Create Vulkan environment states */
BearVulkanEnvState* bear_vulkan_env_create(BearVulkanContext* ctx,
                                            int n_poles, int num_envs,
                                            const float* pole_mass,
                                            const float* pole_length,
                                            float gravity, float cart_mass,
                                            float dt, float force_mag);

/* Destroy Vulkan environment */
void bear_vulkan_env_destroy(BearVulkanEnvState* env);

/* Reset environments (randomize initial state) */
void bear_vulkan_env_reset(BearVulkanContext* ctx,
                            BearVulkanEnvState* env,
                            uint64_t rng_seed);

/* Step environments with given actions */
void bear_vulkan_env_step(BearVulkanContext* ctx,
                           BearVulkanEnvState* env,
                           const void* actions, int act_discrete,
                           uint64_t rng_seed);

/* Copy observations to host tensor */
void bear_vulkan_env_get_obs(BearVulkanContext* ctx,
                              const BearVulkanEnvState* env,
                              float* obs_host);

/* ===================================================================
 * Unified API  --  Auto-dispatch CPU/Vulkan
 * =================================================================== */

/* Configuration for unified dispatch */
typedef struct {
    int use_vulkan;           /* 1 = try Vulkan, 0 = force CPU */
    int min_batch_for_vulkan; /* Minimum batch size to use GPU (default: 256) */
    int device_index;         /* GPU device (-1 = auto) */
    int fallback_to_cpu;      /* 1 = silently fall back on Vulkan error */
    int enable_validation;    /* 1 = enable Vulkan validation layers */
} BearVulkanConfig;

extern BearVulkanConfig bear_vulkan_config;

/* Initialize unified backend (call once at startup) */
int bear_backend_init_vulkan(const BearVulkanConfig* cfg);

/* Shutdown unified backend */
void bear_backend_shutdown_vulkan(void);

/* Unified forward pass  --  auto-dispatches based on config and batch size */
void bear_policy_forward_unified_v(const BearPolicyNet* net,
                                    const BearTensor* obs,
                                    const BearTensor* h_in,
                                    BearTensor* actions,
                                    BearTensor* logprobs,
                                    BearTensor* values,
                                    BearTensor* h_out,
                                    BearArena* temp_arena);

/* Unified backward  --  auto-dispatches */
int bear_policy_backward_unified_v(BearPolicyNet* net,
                                    const BearTensor* obs,
                                    const BearTensor* actions,
                                    const BearTensor* old_logprobs,
                                    const BearTensor* advantages,
                                    float clip_coef,
                                    float policy_grad_scale,
                                    BearArena* temp_arena);

/* Unified value forward */
void bear_value_forward_unified_v(const BearValueNet* vnet,
                                   const BearTensor* obs,
                                   BearTensor* values,
                                   BearArena* temp_arena);

/* Unified value backward */
int bear_value_backward_unified_v(BearValueNet* vnet,
                                   const BearTensor* obs,
                                   const BearTensor* values,
                                   const BearTensor* targets,
                                   float vf_coef,
                                   BearArena* temp_arena);

/* Unified trainer iteration */
float bear_trainer_iter_unified_v(BearTrainer* trainer, uint64_t rng_state[2]);

/* ===================================================================
 * Performance Profiling
 * =================================================================== */

typedef struct {
    const char* name;
    double elapsed_ms;
    double gpu_elapsed_ms;
    int dispatch_count;
    size_t bytes_transferred;
} BearVulkanProfileEvent;

void bear_vulkan_profile_enable(BearVulkanContext* ctx, int enable);
int bear_vulkan_profile_get_events(BearVulkanContext* ctx, BearVulkanProfileEvent* out, int max_events);
void bear_vulkan_profile_reset(BearVulkanContext* ctx);

#ifdef __cplusplus
}
#endif

#endif /* BEAR_VULKAN_H */