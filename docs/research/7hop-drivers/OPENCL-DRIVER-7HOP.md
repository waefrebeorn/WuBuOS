# OPENCL-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: OpenCL gaps

OpenCL bind clinfo runtime detection. AMD: ROCm runtime. NVIDIA:
CUDA runtime. clinfo lists platforms/devices. Pixls.us: "clinfo
command is your friend. Focus on getting clinfo to see GPU first."
Debian: rocm-opencl-runtime for AMD.

### Impl routing (wubu_opencl.c)

| Route | Path |
|-------|------|
| ICD loader           | /usr/lib/libOpenCL.so |
| Platform detection   | clinfo binary |

OpenCL unified. AMD = ROCm. NVIDIA = CUDA. clinfo detects.
