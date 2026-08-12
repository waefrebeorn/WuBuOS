# CUDA-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: CUDA gaps

CUDA bind libcuda driver + NVIDIA compute capability table. CUDA 11.8
= earliest cc8.9/cc9.0 support (NVIDIA Dev Forums). Compute capability
defines GPU features/instructions. StackOverflow: match CUDA version
to GPU cc. "Runs on everything" includes CUDA routing.

### Impl routing (wubu_cuda.c)

| Route | Path |
|-------|------|
| CUDA driver library  | /usr/lib/libcuda.so |
| GPU enumeration      | /proc/driver/nvidia/gpus |

CUDA compute capability (cc). cc 8.9/9.0 needs CUDA 11.8+. libcuda.so =
NVIDIA CUDA driver. /proc/driver/nvidia/gpus = GPU enumeration.
