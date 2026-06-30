# Phase 3-5: Bridge / Hosted / Bear RL CPU Fallbacks (ARCHIVED — COMPLETE)

**Completed**: 2026-06-29
**Status**: ✅ ALL TESTS PASSING
**Files**: 3 major subsystems
**Tests**: 150+ assertions across bridge + hosted + bear targets

---

## Phase 3: Bridge — wubu_syscall.c (COMPLETE)

**File**: `bridge/wubu_syscall.c`
**Before**: 97 void casts + 2 `system()` calls
**After**: fd-based file handlers, Styx offset tracking, container fork+exec, 26 trampolines
**Tests**: 5/5 passing

### Implementations Delivered

| Subsystem | Before | After |
|-----------|--------|-------|
| File handlers | `FILE*` API, `(void)fd` | fd-based open/read/write/close with internal fd→FILE* map |
| Styx handlers | lseek on every call | Persistent offset tracking per fd (O(1) seek) |
| Container handlers | `system("wubu_ct_bwrap...")` | fork+exec bwrap directly; container registry (PID tracking) |
| Syscall trampolines | Empty stubs | 26 inline asm trampolines using `syscall` instruction |

### Eliminated

- 2 `system()` calls → direct fork+exec
- 97 void casts → real parameter usage
- All 5 bridge tests pass

---

## Phase 4: Hosted — hosted/hosted.c (COMPLETE)

**File**: `hosted/hosted.c`
**Before**: 72 void casts in Wayland callbacks
**After**: All registry/keyboard/pointer callbacks wired with state tracking
**Tests**: All hosted tests pass (integrated)

### Callbacks Implemented

| Callback | Implementation |
|----------|----------------|
| `registry_global_remove` | Cleanup: destroy proxy, remove from registry list |
| `keyboard_enter` | Focus tracking: set `keyboard_focus` surface, reset modifier state |
| `keyboard_leave` | Focus tracking: clear `keyboard_focus`, reset modifiers |
| `keyboard_modifiers` | Track `mods_depressed`, `mods_latched`, `mods_locked`, `group` |
| `keyboard_repeat_info` | Store `rate` and `delay` for key repeat synthesis |
| `pointer_leave` | Reset pointer state: clear focus, button state, axis accumulators |
| `pointer_frame` | Frame flush: commit accumulated pointer events to input queue |
| `pointer_axis_source` | Track scroll source: wheel/finger/continuous/tilt |
| `pointer_axis_stop` | Handle axis stop: finalize scroll gesture |
| `pointer_axis_discrete` | Handle discrete steps: accumulate for high-res scroll |

### State Now Tracked

- Keyboard focus surface + modifier state (depressed/latched/locked/group)
- Pointer focus surface + button mask + axis accumulators
- Scroll axis source (wheel vs finger vs continuous)
- Key repeat rate/delay

---

## Phase 5: Bear RL — bear/bear_cudnn.c (COMPLETE)

**File**: `bear/bear_cudnn.c`
**Before**: 117 void casts in `#else` stub blocks (CPU fallback when CUDA not available)
**After**: Full CPU implementations using `bear_simd.h` optimizations
**Tests**: All bear tests pass

### cuBLAS CPU Implementations

| Function | Implementation |
|----------|----------------|
| `cublasSgemm` | `bear_gemm` with SIMD (AVX2/NEON), row/col major, transa/transb |
| `cublasSaxpy` | `bear_axpy` SIMD vector: y = a*x + y |
| `cublasSdot` | `bear_dot` SIMD dot product with Kahan summation |
| `cublasSnrm2` | `bear_nrm2` SIMD Euclidean norm with scaling |
| `cublasSscal` | `bear_scal` SIMD vector scale: x = a*x |

### cuDNN CPU Implementations

| Function | Implementation |
|----------|----------------|
| Tensor descriptors | Shape/stride packing, NCHW/NHWC, dtype validation |
| `cudnnConvolutionForward` | Im2col + GEMM (bear_gemm) with padding/stride/dilation |
| `cudnnConvolutionBackwardData` | Col2im + GEMM transpose |
| `cudnnConvolutionBackwardFilter` | Gradient accumulation via GEMM |
| Activation (sigmoid/relu/tanh) | `bear_act_scalar` / `bear_act_batch` SIMD |
| Pooling (max/avg) | Windowed reduce with SIMD |
| Softmax (instance/channel) | Stable softmax: max-subtract + exp + sum + divide |
| Workspace queries | Return computed size (no allocation) |
| `cudaMalloc`/`cudaFree` | Host malloc/free with alignment tracking |

### bear_simd.h Integration

- `bear_gemm`: Blocked matrix multiply with AVX2/NEON intrinsics
- `bear_axpy`, `bear_dot`, `bear_nrm2`, `bear_scal`: Vector ops
- `bear_act_batch`: Batch activation (ReLU/sigmoid/tanh/gelu)
- `bear_act_scalar`: Scalar activation fallbacks
- Runtime CPU feature detection (AVX2, NEON, SSE4.2)

---

## REAL_GAPs Closed

- wubu_syscall.c: 97 → 0
- hosted.c: 72 → 0
- bear_cudnn.c: 117 → 0

**Phase 3-5 Total**: 286 REAL_GAPs closed