# The Kernel Interchange Network — design + the OS deep-dive gap map

> "reproduce and graft every missing piece from the other operating systems …
> and start working on the kernel interchange network that allows us to load
> and swap in kernels of different operating systems. This will work for
> Linux, Windows and macOS."
>
> (The user dictates through Siri speech-to-text: "Colonel" = **kernel**,
> "Lennox" = **Linux**.  The in-repo `wubu_colonel.c` app-dispatcher is a
> different, pre-existing thing and is NOT what this doc is about.)

## 1. What shipped

`src/runtime/wubu_kernel_xchg.{h,c}` + `test_kernel_xchg` (wired into
`test_critical_runtime`).

A kernel is a **registered object with a versioned contract**, not the
foundation of the box:

```c
typedef struct wubu_kernel_ops {
    const char *name;        /* "linux", "nt", "macos", "holyd", "cpm" */
    int abi_version;
    int schema_version;      /* /n/trace schema major; 0 = emits none */
    const char *formats;     /* "elf64" "pe" "macho" ... */
    int  (*try_dispatch)(uint64_t num, uint64_t *regs, int64_t *out);
    int  (*probe)(const char *path, const uint8_t *hdr, size_t n);
    int  (*exec)(const char *path, int argc, char *const argv[]);
} wubu_kernel_ops_t;
```

The four behaviours that make it a network rather than a table:

1. **Register** — a duplicate name is *rejected*, not silently overwritten.
   Two kernels answering to one name is the drift the interchange prevents.
   A kernel that can neither dispatch, probe nor exec is rejected as inert.
2. **Fail-closed contract** — `wubu_kernel_select()` refuses any kernel whose
   trace-schema major differs from ours.  A kernel that cannot be observed at
   the boundary must not be allowed to run (MEGA_COMPAT DA-2).
3. **Routing with fallthrough** — `wubu_kernel_interchange()` asks the active
   kernel first, then every other registered kernel, and the *first one that
   claims the call* answers it.  A foreign kernel's unimplemented syscall can
   still be answered by one that knows it.  `wubu_kernel_dispatch_active()`
   is the strict no-fallthrough variant.
4. **Observable swaps** — every change of the active kernel is appended to a
   swap log (`select` and `swap` funnel through one function, so the log cannot
   disagree with the state it describes).  `wubu_kernel_xchg_dump()` renders
   the whole network as plain text for the `/n/kernels` publisher.

## 2. The deep dive — what is already grafted

The `os-studies/` README maps reference OS features to module names.  **Part of
that map is aspirational** — the names are written down, the files do not exist.
This table was re-verified against the tree (`ls src/**/*<name>*.c`), not
copied from the README.

| Reference OS | Grafted, real module | Genuinely missing |
|---|---|---|
| **GNOME 46+** | D-Bus activation `runtime/wubu_bus.c`; socket activation `wubu_svc_socket.c`; polkit `wubu_auth.c`; logind `wubu_logind.c`; the whole session/service runtime (`wubu_session_boot/daemon/mgr`, `wubu_ns_background`) | **GVfs** (SMB/SFTP/cloud/trash VFS), **Tracker** (content indexer), **AT-SPI2** (the a11y bus — `gui/wubu_a11y.c` exists but is the WM's own a11y surface, not the AT-SPI2 wire protocol) |
| **SteamOS 3.x** | DXVK config + UI (`runtime/wubu_proton_dxvk.c`, `wubu_dxvk_conf.c`, `gui/wubu_proton_dxvk.c`); GameScope (`wubu_proton2_gamescope.c`); Pressure Vessel (`wubu_pressure_vessel`); Steam Input + the real Deck report protocol (`wubu_steaminput.c`, `wubu_ns_steaminput.c`); EC fan/PWM (`wubu_ec_control.c`) | **VKD3D-Proton** (D3D12→Vulkan — no file), **Steam Networking** (P2P/relay/SDR — no file), **shader pre-cache / fossilize** (no file) |
| **TempleOS/ZealOS** | HolyC lexer/parser/codegen + JIT, the REPL (`runtime/wubu_holyd*.c`), the whole ZealOS parity header | **DolDoc** (hyperlinked doc format), **RedSea FS** (contiguous B-tree, no paths), **raw VGA/VESA** from HolyC |
| **Arch Linux** | package registry + pacman-style ops (`runtime/wubu_pkg.c`, `wubu_ns_pkg.c`); container/OCI runtime; snapshots; the archd daemon | **makepkg/PKGBUILD**, **AUR**, **mkinitcpio** (encrypt/lvm/raid/btrfs hooks), **systemd-homed/sysusers** |
| **Windows (NT)** | 296-slot NT dispatch table + 11 extension modules (`runtime/vsl/vsl_nt_*.c`), ReactOS ordinal transliteration, PE loader (`wubu_proton_pe.c`), the Win98/XP shell | **VKD3D** (see SteamOS), **ALPC/WNF/Enclave** blocks of the NT table (the `nt-parity-wip` branch carries 60 colliding slots and was deliberately NOT merged) |
| **macOS** | Mach-O loader (`vsl_macho.c`), 77 BSD + 13 Mach trap handlers (`vsl_syscall_mac.c`), ~250 syscall numbers | **IOKit parity**, **Metal/GPU parity**, the mach trap coverage beyond the 13 implemented |
| **CachyOS** | scheduler study + `chwd`-class hw detect (`wubu_hw_detect`) | **BORE/EEVDF/scx** scheduler plugins (the kernel tasking is EEVDF already; the plugins are not) |
| **Inferno/Plan 9** | Styx/9P client + server, the whole `/n` namespace bridge | the 9P **kernel** server (the server is userspace) |
| **CP/M, classic Mac** | both are VSL personalities (`vsl_cpm_syscall_dispatch`, `vsl_macclassic_syscall_dispatch`) | — |

## 3. The graft order (highest leverage first)

Each item names the **interchange slot** that will own it — that is the point of
the network: a graft lands as a registered kernel, not as a hard-coded branch.

1. ✅ **`/n/kernels` surface** — `runtime/wubu_ns_kernels.{h,c}`, published from
   the desktop boot. `list` / `active` / `history` are re-rendered from the live
   state on every refresh, and `wubu_ns_kernels_select()` is the operator's
   control path (a swap, so fail-closed on the contract). Verified by
   `test_ns_kernels` (14 assertions, in `test_medium_other`).
2. ✅ **Wire the VSL personalities into the interchange** — done in
   `runtime/vsl/vsl_kernel_adapt.c`; the class-bit if-chain in
   `vsl_syscall_dispatch()` is gone, each personality owns a disjoint number
   range, and the router decides. Verified by `test_vsl_kernel_adapt`
   (in `test_high_bridge`) plus the existing VSL suites.
3. ◧ **VKD3D** (NT/SteamOS slot) — D3D12→Vulkan.  The largest single gaming gap.
   *Slice 1 done* (`runtime/wubu_vkd3d.{h,c}`, `test_vkd3d` in `test_medium_other`):
   the resource-state translation core — the full single-bit state table with
   real Vulkan equivalents, composite states mapping to the OR of their parts,
   unknown bits rejected rather than guessed, the barrier translator, and the
   per-resource state tracker.  *Slice 2 done* (`runtime/wubu_vkd3d_heap.{h,c}`,
   `test_vkd3d_heap`): the descriptor heaps -- configurable descriptor size,
   handle arithmetic, bounds-checked writes, and a copy path that forbids
   cross-type and mismatched-stride copies, uses memmove for overlaps, and
   leaves the destination unchanged on rejection.  The Vulkan mapping is
   honest about what does not map: CBV_SRV_UAV is a MIXED heap (its pool is
   sized for the union of four descriptor types), and RTV/DSV are framebuffer
   ATTACHMENTS rather than descriptors, so their pools need no sizes.
   GPU-visible heaps carry UPDATE_AFTER_BIND on both the pool and the binding.
   *Slice 3 done* (`runtime/wubu_vkd3d_cmd.{h,c}`, `test_vkd3d_cmd`): command
   allocators and lists -- the four-state machine with every illegal
   transition refused (recording cannot execute, closed cannot re-close,
   executing cannot reset, allocator cannot reset while any list is in
   flight, allocator-reset lists are INVALID and must be re-created).  The
   Vulkan mapping is honest about a distinction Vulkan does not have:
   DIRECT -> PRIMARY, BUNDLE -> SECONDARY, COMPUTE/COPY -> PRIMARY on the
   compute/transfer QUEUE FAMILY (Vulkan has no such buffer level).
   *Slice 4 done* (`runtime/wubu_vkd3d_pso.{h,c}`, `test_vkd3d_pso`): pipeline
   state objects -- the whole-desc FNV-1a key over a canonical serialisation
   (plain uint32_t fields, because bitfield padding would make two identical
   descs hash differently), a hash match treated as only a CANDIDATE and
   confirmed with a full memcmp so a collision is a miss rather than a wrong
   pipeline, a cache that rejects when full instead of evicting, and a Vulkan
   mapping honest about the two APIs disagreeing (D3D12 comparison funcs are
   1-based and Vulkan's are 0-based -- the first version passed the value
   through, turning LESS_EQUAL into GREATER, and the test caught it; blend
   factors are 1-based with a hole at 12; blend MIN/MAX swap positions).
   The load-bearing assertion is the MUTATION SWEEP: all 37 fields changed in
   turn, every one must change the key.
   *Slice 7* (`wubu_vkd3d_swapchain.{h,c}`): the present path -- swap effect ->
   present mode, sync interval -> present mode, and SetMaximumFrameLatency ->
   minImageCount.  Vulkan cannot express a SyncInterval above 1 (it has FIFO and
   IMMEDIATE, nothing between), so the translation reports `needs_app_throttle`
   rather than pretending FIFO is "every Nth frame".  There is deliberately NO
   "frame latency exceeds the buffer count" check: Vulkan may return MORE
   images than requested, so the requirement is met by RAISING the image count,
   not by rejecting a legal D3D12 config.
   *Slices 5 and 6* (`wubu_vkd3d_device.{h,c}`, `wubu_vkd3d_fence.{h,c}`):
   the feature-level device (created AT a level, never downgraded; validates a
   PSO desc against the caps that level implies, returning the first violated
   rule; caps are INHERITANCE, and a fall-through switch lost 11.1's
   tessellation and geometry shaders on the first attempt) and fences as
   Vulkan TIMELINE semaphores (strictly-increasing values, because a
   non-increasing signal collapses two submissions into one and lets the CPU
   reuse memory the GPU is still reading).
   *Slices 8-13* -- the device boundary and everything after it:
   `wubu_vkd3d_physdev` (physical device selection: API version + required queue
   families + extensions + every required feature bit SET, answering with a
   REASON so an app can fall back; present capability checked per queue family,
   since a transfer-only family cannot present), `wubu_vkd3d_vkdev` (the
   logical device; a family may not be requested twice, and every request is
   validated before `vkCreateDevice` because the driver's answer names no
   field), `wubu_vkd3d_renderpass` (D3D12's NO_ACCESS must become DONT_CARE on
   BOTH sides -- LOAD reads stale contents, STORE keeps them, and D3D12's
   contract is that they are undefined afterwards), `wubu_vkd3d_framebuffer`
   (aspect flags come from the FORMAT, not the caller's claim, and Vulkan
   requires every attachment to share the framebuffer's extent and sample count
   -- D3D12 never checks either), `wubu_vkd3d_memory` (DEFAULT/UPLOAD/READBACK
   heaps -> memory types; the type must satisfy the heap, the app's extra
   requirements AND the resource's memoryTypeBits; READBACK prefers
   HOST_CACHED via a two-pass selection), and `wubu_vkd3d_descset` (root
   signature -> pipeline layout: a D3D12 register is 32 BITS so root constant
   N is byte N*4, and the push constant range is ONE contiguous block covering
   the highest register used).
   *LIVE on hardware (separate GPU-requiring targets, deliberately NOT in the
   gate):* `test_vkd3d_live` (enumerate -> pick the discrete GPU -> create a
   real VkDevice), `test_vkd3d_live_render` (real images, a real VkRenderPass
   and VkFramebuffer, a submitted clear), `test_vkd3d_live_pipeline` (real
   SPIR-V, a pipeline built FROM the PSO slice's mapped state, a draw, and a
   READBACK proving the triangle landed at the computed coordinates),
   `test_vkd3d_live_binding` (a root constant AND a real CBV, both proven by
   changing either and re-reading the pixel), and `test_vkd3d_live_texture`
   (upload -> SRV + sampler -> sample; the centre pixel is the LINEAR AVERAGE
   of four texels, so filtering is proven too).
   *Still open:* only the swapchain/present leg (it needs a surface, and this
   box is headless).  Slices 14 and 15 added MSAA (SAMPLE_DESC -> rasterization
   samples + sample shading, an explicit RESOLVE, and the rule that a pipeline's
   sample count must EQUAL the attachment's) and the shader pre-cache (a
   device-bound, versioned, ATOMICALLY-saved disk cache, so a second launch does
   not recompile).  A live 4x MSAA target with a resolve is verified by readback
   in `test_vubu_vkd3d_live_msaa`, and a UAV/storage-buffer compute dispatch in
   `test_vkd3d_live_uav`.
4. ~**Shader pre-cache / fossilize**~ (SteamOS slot) — **CORE DONE**
   (`wubu_vkd3d_precache`): a cache bound to vendor+device+driver and a VERSION,
   discarded WHOLE when a file is truncated or corrupt, bounded, refreshing
   rather than duplicating, and saved ATOMICALLY (temp + rename).  Without it
   every Proton launch stutters on first shader compile.
5. ~**Steam Networking**~ (SteamOS slot) — **CORE DONE**, four layers:
   `wubu_sdr` (routing tickets: one connection, expiring, capped window,
   half-open, tamper-evident, and a route table where a virtual address may be
   claimed once), `wubu_sdr_relay` (fail-closed, first failure decides, a
   not-ready route refused because forwarding would black-hole the packet),
   `wubu_sdr_session` (a port is NEVER reassigned until its session is REAPED,
   so one peer cannot receive another's traffic; free-list recycling).
6. ~**GVfs + Tracker**~ (GNOME slot) — **GVfs DONE**, three slices:
   `wubu_gvfs` (the async op state machine: terminal states, a cancel of a
   finished handle refused, only CLOSING finishes), `wubu_gvfs_path`
   (normalization, traversal REJECTED not clamped, the base is a DIRECTORY, the
   reference is joined BEFORE it is normalized), `wubu_gvfs_mount` (LONGEST
   PREFIX WINS or the deeper mount's files go to the shallower filesystem, and
   a mount root must already be canonical).
   *Still open:* Tracker (the full-text search indexer).
7. ~**makepkg / AUR / mkinitcpio**~ (Arch slot) — **CORE DONE**, four slices:
   `wubu_pkg` extended (numeric version compare, constraints, post-order
   resolution with cycle/missing detection, and pkg_remove now refuses to break
   an installed dependent), `wubu_pkgbuild` (a parser that PARSES and never
   executes -- a `$(...)` is a literal), `wubu_aur` (URL encoding, request
   splitting rather than truncation, and resultcount checked against the body),
   `wubu_hook` (fixed stage order, non-executable skipped, optional continues,
   required aborts with nothing after it running).
8. ~**DolDoc / RedSea / raw VGA**~ (HolyC slot) — **DolDoc CORE DONE**
   (`wubu_doldoc`): magic and version checked, every length field bounds-checked
   against the bytes present, an unknown node type refused rather than skipped,
   a link without a target refused, depth capped, and the node array bounded.
   *Still open:* RedSea (the filesystem) and raw VGA.
9. ~**AT-SPI2**~ (GNOME slot) — **CORE DONE**, three slices: `wubu_dbus` (the
   wire format: per-type alignment, a string's length EXCLUDING the NUL, a
   signature's BYTE length, and an ARRAY's count IN BYTES not elements),
   `wubu_atspi` (the CANONICAL role names a screen reader announces, -1 meaning
   NOT CACHED rather than empty, and object-path/bus-name validation),
   `wubu_atspi_tree` (the walk: a cycle detected, depth and node count capped, a
   NULL root valid, the callback able to abort).
10. ~**NT ALPC/WNF/Enclave**~ — **COLLISION QUESTION RESOLVED.**  The
    `nt-parity-wip` branch no longer exists (locally or remotely), so the map's
    "60 colliding slots" claim was stale and the live tree had never been
    checked.  Registration is now wrapped (`vsl_nt_collision_count`,
    `vsl_nt_collision_at`, `vsl_nt_stub_replace_count`): a REAL handler replacing
    another REAL handler is a recorded collision, and a real handler replacing a
    STUB is a counted upgrade.  Measured: 412 slots covered, 81 stub upgrades,
    ZERO real collisions.  ALPC/WNF/Enclave handlers are registered in the live
    tree; what was missing was the proof and the regression tripwire.

## 4. Honest status

- Shipped and verified: the interchange registry, contract, routing, swap log
  and dump — `test_kernel_xchg`, 60 assertions, all passing.
- The VSL personalities are **not yet registered** into the interchange; the
  class-bit router in `vsl_syscall_dispatch()` is still the live path.  Until
  step 2 lands, the network is a working skeleton with no production kernels
  in it — deliberately reported rather than papered over.
- The graft list above is ordered by leverage, not by ease.  Items 3–5 are the
  gaming surface; items 6–9 are the desktop surface.
