# FLTK Integration Notes for WuBuOS

## Status: **NOT INTEGRATED** (by design)

FLTK (Fast Light Toolkit) is a C++ GUI library. WuBuOS is **pure C11** — no C++ runtime, no exceptions, no STL.

## Why FLTK isn't the primary GUI

| Factor | FLTK | WuBuOS Stack |
|--------|------|--------------|
| Language | C++ | C11 |
| Bare-metal | ❌ Requires OS | ✅ ZealOS kernel |
| Win98 look | ❌ Modern widgets | ✅ `dosgui_*` replicas |
| Vulkan compute | ❌ No native | ✅ 4 compute pipelines |
| 9P/Styx | ❌ | ✅ First-class |
| Container isolation | ❌ | ✅ cgroups v2 + seccomp |

## How to run FLTK apps on WuBuOS (the "boomer" way)

**Option 1: Containerized FLTK** (recommended)
```c
// src/runtime/wubu_arch.c — spawn Arch container with FLTK
// wubu_arch_exec(ctx, "pacman -S fltk && ./my_fltk_app");
// Runs under wubu_oci.c + wubu_ct_isolate.c
```

**Option 2: XWayland passthrough**
```c
// src/hosted/wubu_vulkan.c — create XWayland surface
// FLTK app thinks it's on X11; WuBuOS composites via Vulkan
```

**Option 3: FLTK as a C++ plugin** (if you really want it linked)
```makefile
# Add to Makefile (C++ only, hosted mode)
FLTK_OBJS = $(GUI)/fltk_bridge.o $(GUI)/fltk_window.o
CXXFLAGS = -std=c++17 -I/usr/include/FL
LDFLAGS += -lfltk -lfltk_images -lfltk_gl
```

## Quick test: does FLTK even build here?

```bash
# In container (where you have sudo):
docker run --rm -it archlinux \
  pacman -Sy --noconfirm fltk && \
  cat > test.cpp << 'EOF'
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Button.H>
int main() {
  Fl_Window w(300, 200, "FLTK on WuBuOS");
  Fl_Button b(100, 80, 100, 40, "Boomer");
  w.show();
  return Fl::run();
}
EOF
  g++ test.cpp -o test `fltk-config --cxxflags --ldflags` && ./test
```

## Verdict

**WuBuOS doesn't need FLTK.** It *hosts* FLTK apps via containers/XWayland.

If you want the "we do it all" badge:
- ✅ Add `fltk` to Arch container package list (`src/runtime/wubu_arch.c`)
- ✅ Document XWayland passthrough in `src/hosted/wubu_vulkan.c`
- ✅ Keep host **pure C11**

The "boomer" look is already `dosgui_wm.c` + `dosgui_startmenu.c` — no FLTK required.