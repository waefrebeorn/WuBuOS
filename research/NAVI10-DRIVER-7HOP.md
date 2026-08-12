# NAVI10-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: AMD Navi10 gaps

AMD Navi10 (RX 5700/XT, RDNA1) binds amdgpu kernel driver + RADV
Vulkan in Mesa. Gentoo Wiki: requires kernel 5.3, Mesa 19.2, LLVM 9+.
AMD Radeon 25.20.3: "100% open-source core" — proprietary OpenGL/Vulkan
removed. RADV from Mesa, not AMDVLK (which is Windows).

### Impl routing (wubu_navi10.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| PCI vendor metric    | /sys/class/drm/card0/device/vendor |

RDNA1/Navi10. amdgpu kernel 5.3+. RADV Vulkan from Mesa. AMDVLK is
Windows-only. AMD 25.20.3 = 100% open-source (no proprietary GL/VK).
