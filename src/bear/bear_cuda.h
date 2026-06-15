/*
 * bear_cuda.h  --  BearRL CUDA Plugin Interface
 *
 * Provides a clean abstraction for GPU acceleration of BearRL components.
 * Falls back to CPU implementation when CUDA is unavailable.
 * Pure C11 host code + CUDA kernels in bear_kernels.cu
 */

#ifndef BEAR_CUDA_H
#define BEAR_CUDA_H

#include "bear_arena.h"
#include "bear_nn.h"
#include "bear_ppo.h"
#include "bear_env.h"
#include "bear_opt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===================================================================
 * CUDA Backend Capability Query
 * =================================================================== */

typedef enum {
    BEAR_CUDA_UNAVAILABLE = 0,  /* No CUDA runtime / no compatible GPU */
    BEAR_CUDA_AVAILABLE   = 1,  /* CUDA runtime + GPU detected */
    BEAR_CUDA_ACTIVE      = 2,  /* Context initialized, streams ready */
} BearCudaStatus;

/* Device properties */
typedef struct {
    int device_id;
    char name[256];
    int compute_capability_major;
    int compute_capability_minor;
    size_t global_mem_bytes;
    size_t shared_mem_per_block;
    int max_threads_per_block;
    int max_blocks_per_sm;
    int num_sms;
    int warp_size;
    int clock_rate_khz;
} BearCudaDeviceInfo;

/* Query CUDA availability */
BearCudaStatus bear_cuda_query(void);

/* Get device info (requires active context) */
int bear_cuda_get_device_info(int device_id, BearCudaDeviceInfo* info);

/* ===================================================================
 * CUDA Context Management
 * =================================================================== */

typedef struct BearCudaContext BearCudaContext;

/* Initialize CUDA context (must be called before any GPU ops) */
/* device_id = -1 for auto-select best device */
BearCudaContext* bear_cuda_init(int device_id);

/* Destroy CUDA context */
void bear_cuda_destroy(BearCudaContext* ctx);

/* Synchronize all GPU work */
void bear_cuda_sync(BearCudaContext* ctx);

/* Get last CUDA error string */
const char* bear_cuda_last_error(BearCudaContext* ctx);

/* ===================================================================
 * GPU Memory Management (Arena-like interface)
 * =================================================================== */

typedef struct BearGpuArena BearGpuArena;

/* Create GPU arena for temporary allocations (per-step/per-minibatch) */
BearGpuArena* bear_gpu_arena_create(BearCudaContext* ctx, size_t capacity_bytes);

/* Reset GPU arena (free all temp allocations, keep capacity) */
void bear_gpu_arena_reset(BearGpuArena* arena);

/* Destroy GPU arena */
void bear_gpu_arena_destroy(BearGpuArena* arena);

/* Allocate device memory from GPU arena */
void* bear_gpu_arena_alloc(BearGpuArena* arena, size_t bytes);

/* ===================================================================
 * GPU Tensor Abstraction (SoA, device pointers)
 * =================================================================== */

typedef enum {
    BEAR_GPU_TENSOR_HOST = 0,   /* Data on host */
    BEAR_GPU_TENSOR_DEVICE = 1, /* Data on device */
    BEAR_GPU_TENSOR_UNIFIED = 2,/* Unified memory (CUDA managed) */
} BearGpuTensorLocation;

typedef struct {
    void* data;                 /* Host or device pointer */
    int64_t* shape;             /* Host array of shape dims */
    int ndim;                   /* Number of dimensions */
    int dtype;                  /* BEAR_DTYPE_F32, BEAR_DTYPE_INDICES, etc. */
    BearGpuTensorLocation loc;  /* Where data lives */
    size_t numel;               /* Total elements */
    size_t bytes;               /* Total bytes */
} BearGpuTensor;

/* Create GPU tensor from existing host tensor */
int bear_gpu_tensor_from_host(BearCudaContext* ctx, BearGpuArena* arena,
                               const BearTensor* host, BearGpuTensor* gpu);

/* Create empty GPU tensor (allocates on device) */
int bear_gpu_tensor_create(BearCudaContext* ctx, BearGpuArena* arena,
                            const int64_t* shape, int ndim, int dtype,
                            BearGpuTensor* gpu, const char* name);

/* Copy GPU tensor to host */
int bear_gpu_tensor_to_host(BearCudaContext* ctx, const BearGpuTensor* gpu, BearTensor* host);

/* Copy between GPU tensors (device-to-device) */
int bear_gpu_tensor_copy(BearCudaContext* ctx, const BearGpuTensor* src, BearGpuTensor* dst);

/* Free GPU tensor (if allocated from arena, waits for arena reset) */
void bear_gpu_tensor_free(BearGpuArena* arena, BearGpuTensor* gpu);

/* ===================================================================
 * Accelerated Operations  --  Policy Network (Forward/Backward)
 * =================================================================== */

/* Forward pass: obs -> (actions, logprobs, values, h_out) */
/* Automatically selects GPU if available and batch size >= threshold */
void bear_policy_forward_gpu(BearCudaContext* ctx,
                             const BearPolicyNet* net,
                             const BearGpuTensor* obs,
                             const BearGpuTensor* h_in,
                             BearGpuTensor* actions,
                             BearGpuTensor* logprobs,
                             BearGpuTensor* values,
                             BearGpuTensor* h_out,
                             BearGpuArena* temp_arena);

/* Backward pass for discrete actions */
int bear_policy_backward_discrete_gpu(BearCudaContext* ctx,
                                       BearPolicyNet* net,
                                       const BearGpuTensor* obs,
                                       const BearGpuTensor* actions,
                                       const BearGpuTensor* old_logprobs,
                                       const BearGpuTensor* advantages,
                                       float clip_coef,
                                       float policy_grad_scale,
                                       BearGpuArena* temp_arena);

/* Backward pass for continuous (Gaussian) actions */
int bear_policy_backward_continuous_gpu(BearCudaContext* ctx,
                                         BearPolicyNet* net,
                                         const BearGpuTensor* obs,
                                         const BearGpuTensor* actions,
                                         const BearGpuTensor* old_logprobs,
                                         const BearGpuTensor* advantages,
                                         float clip_coef,
                                         float policy_grad_scale,
                                         BearGpuArena* temp_arena);

/* MinGRU step (recurrent core) */
void bear_mingru_step_gpu(BearCudaContext* ctx,
                           const BearMinGRU* gru,
                           const BearGpuTensor* x,
                           const BearGpuTensor* h_in,
                           BearGpuTensor* h_out,
                           BearGpuArena* temp_arena);

/* ===================================================================
 * Accelerated Operations  --  Value Network
 * =================================================================== */

void bear_value_forward_gpu(BearCudaContext* ctx,
                             const BearValueNet* vnet,
                             const BearGpuTensor* obs,
                             BearGpuTensor* values,
                             BearGpuArena* temp_arena);

int bear_value_backward_gpu(BearCudaContext* ctx,
                             BearValueNet* vnet,
                             const BearGpuTensor* obs,
                             const BearGpuTensor* values,
                             const BearGpuTensor* targets,
                             float vf_coef,
                             BearGpuArena* temp_arena);

/* ===================================================================
 * Accelerated Operations  --  PPO Training Loop
 * =================================================================== */

/* GAE advantage computation on GPU */
void bear_compute_advantages_gpu(BearCudaContext* ctx,
                                  BearTrajectory* t,
                                  const BearPPOConfig* cfg,
                                  BearGpuArena* temp_arena);

/* V-Trace on GPU */
void bear_vtrace_compute_gpu(BearCudaContext* ctx,
                              const float* rewards,
                              const uint8_t* dones,
                              const float* values,
                              const float* logprobs,
                              const float* target_logprobs,
                              float* advantages,
                              float* returns,
                              int T, int B, const BearPPOConfig* cfg,
                              BearGpuArena* temp_arena);

/* Minibatch sampler (shuffle + gather) on GPU */
int bear_sampler_next_gpu(BearCudaContext* ctx,
                           BearMinibatchSampler* s,
                           BearTrajectory* t,
                           BearGpuTensor* mb_obs,
                           BearGpuTensor* mb_actions,
                           BearGpuTensor* mb_logprobs,
                           BearGpuTensor* mb_advantages,
                           BearGpuTensor* mb_returns,
                           BearGpuTensor* mb_values,
                           BearGpuTensor* mb_old_logprobs,
                           BearGpuArena* temp_arena);

/* PPO loss computation on GPU */
BearPPOLoss bear_ppo_loss_gpu(BearCudaContext* ctx,
                               const BearPolicyNet* policy,
                               const BearValueNet* critic,
                               const BearGpuTensor* obs,
                               const BearGpuTensor* actions,
                               const BearGpuTensor* old_logprobs,
                               const BearGpuTensor* advantages,
                               const BearGpuTensor* returns,
                               const BearGpuTensor* old_values,
                               const BearPPOConfig* cfg,
                               BearGpuArena* temp_arena);

/* Apply gradients via Adam on GPU */
void bear_ppo_apply_gradients_gpu(BearCudaContext* ctx,
                                   BearPolicyNet* policy,
                                   BearValueNet* critic,
                                   BearOptimizer* opt_policy,
                                   BearOptimizer* opt_critic);

/* Gradient clipping on GPU */
float bear_clip_grad_norm_gpu(BearCudaContext* ctx,
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
    
    /* Physics parameters (constant per pole, same for all envs) */
    float* pole_mass;      /* [n_poles] */
    float* pole_length;    /* [n_poles] */
    float* pole_com;       /* [n_poles] */
    float* pole_inertia;   /* [n_poles] */
    
    /* Per-environment state */
    float* x;              /* [num_envs] cart position */
    float* x_dot;          /* [num_envs] cart velocity */
    float* theta;          /* [num_envs, n_poles] pole angles */
    float* theta_dot;      /* [num_envs, n_poles] pole angular velocities */
    
    /* Force applied this step */
    float* force;          /* [num_envs] */
    
    /* Step outputs */
    float* reward;         /* [num_envs] */
    uint8_t* done;         /* [num_envs] */
    
    /* Constants */
    float gravity;
    float cart_mass;
    float dt;
    float force_mag;
    float angle_threshold;      /* 12 deg = 0.20944 rad */
    float pos_threshold;        /* 2.4 m */
} BearGpuEnvState;

/* Create GPU environment states */
BearGpuEnvState* bear_gpu_env_create(BearCudaContext* ctx,
                                      int n_poles, int num_envs,
                                      const float* pole_mass,
                                      const float* pole_length,
                                      float gravity, float cart_mass,
                                      float dt, float force_mag);

/* Destroy GPU environment */
void bear_gpu_env_destroy(BearGpuEnvState* env);

/* Reset environments (randomize initial state) */
void bear_gpu_env_reset(BearCudaContext* ctx,
                         BearGpuEnvState* env,
                         uint64_t rng_seed);

/* Step environments with given actions */
/* actions: [num_envs] int (discrete) or float (continuous force) */
void bear_gpu_env_step(BearCudaContext* ctx,
                        BearGpuEnvState* env,
                        const void* actions, int act_discrete,
                        uint64_t rng_seed);

/* Copy observations to host tensor [num_envs * n_poles * 4] (x, x_d, theta_i, theta_d_i) */
void bear_gpu_env_get_obs(BearCudaContext* ctx,
                           const BearGpuEnvState* env,
                           float* obs_host);

/* ===================================================================
 * Unified API  --  Auto-dispatch CPU/GPU
 * =================================================================== */

/* Configuration for unified dispatch */
typedef struct {
    int use_gpu;              /* 1 = try GPU, 0 = force CPU */
    int min_batch_for_gpu;    /* Minimum batch size to use GPU (default: 256) */
    int device_id;            /* GPU device (-1 = auto) */
    
    /* Fallback behavior */
    int fallback_to_cpu;      /* 1 = silently fall back on GPU error */
} BearCudaConfig;

extern BearCudaConfig bear_cuda_config;

/* Initialize unified backend (call once at startup) */
int bear_backend_init(const BearCudaConfig* cfg);

/* Shutdown unified backend */
void bear_backend_shutdown(void);

/* Unified forward pass  --  auto-dispatches based on config and batch size */
void bear_policy_forward_unified(const BearPolicyNet* net,
                                  const BearTensor* obs,
                                  const BearTensor* h_in,
                                  BearTensor* actions,
                                  BearTensor* logprobs,
                                  BearTensor* values,
                                  BearTensor* h_out,
                                  BearArena* temp_arena);

/* Unified backward  --  auto-dispatches */
int bear_policy_backward_unified(BearPolicyNet* net,
                                  const BearTensor* obs,
                                  const BearTensor* actions,
                                  const BearTensor* old_logprobs,
                                  const BearTensor* advantages,
                                  float clip_coef,
                                  float policy_grad_scale,
                                  BearArena* temp_arena);

/* Unified value forward */
void bear_value_forward_unified(const BearValueNet* vnet,
                                 const BearTensor* obs,
                                 BearTensor* values,
                                 BearArena* temp_arena);

/* Unified value backward */
int bear_value_backward_unified(BearValueNet* vnet,
                                 const BearTensor* obs,
                                 const BearTensor* values,
                                 const BearTensor* targets,
                                 float vf_coef,
                                 BearArena* temp_arena);

/* Unified trainer iteration */
float bear_trainer_iter_unified(BearTrainer* trainer, uint64_t rng_state[2]);

/* ===================================================================
 * Performance Profiling
 * =================================================================== */

typedef struct {
    const char* name;
    double elapsed_ms;
    double gpu_elapsed_ms;
    int launch_count;
    size_t bytes_transferred;
} BearCudaProfileEvent;

void bear_cuda_profile_enable(BearCudaContext* ctx, int enable);
int bear_cuda_profile_get_events(BearCudaContext* ctx, BearCudaProfileEvent* out, int max_events);
void bear_cuda_profile_reset(BearCudaContext* ctx);

#ifdef __cplusplus
}
#endif

#endif /* BEAR_CUDA_H */