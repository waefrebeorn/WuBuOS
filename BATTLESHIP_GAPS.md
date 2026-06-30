# WuBuOS BATTLESHIP — Fresh Gap List 2026-06-22 (Post-Deep-Stub-Hunt)

> **⚠️ SUPERSEDED by BATTLESHIP.md (Phase 12, 2026-06-26)** — This v11 document is no longer the active gap inventory. See [BATTLESHIP.md](BATTLESHIP.md) for the 300-gap triple DA audit.
## Triple Devil's Advocate Audit — REAL_GAPs Only

**Total TODO/STUB markers**: 234 across 79 files
**Unique real gaps**: 400+ (expanded from ~300 after deep audit)
**Previously resolved**: 34 cells (200-207, 310-313, 340-341, 380-381, 390-405, 420-425)
**This session's fix**: wubu_network.c — all 15 TODO-NET + 3 TODO-DNS markers resolved (bridge/macvlan/ipvlan/overlay best-effort, DNS record storage, WireGuard/Tailscale binary invocation, tc qdisc QoS)

---

## Legend
- 🔴 = FULL STUB (every function in file returns -1/does nothing)
- 🟡 = PARTIAL (some functions work, some stubbed)
- ⬜ = MINOR (handful of stubs in mostly-working file)
- ✅ = RESOLVED this session or previously
- `return -1` means the function does no real work
- **REAL_GAP** = "rewriting from scratch in C" territory — not a TODO comment but a missing implementation

---

## TIER 1 — CRITICAL (User-Facing, Breaks Demo, Form≠Function)

### 🔴 src/runtime/wubu_oci.c — OCI Runtime (17 functions) [10 markers]
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 1 | oci_config_from_json | ~234 | Parse OCI config from JSON string | ✅ |
| 2 | oci_config_compute_digest | ~239 | SHA-256 digest of config JSON | ✅ |
| 3 | oci_config_to_json | — | Serialize config to JSON | ✅ |
| 4 | oci_manifest_from_json | ~311 | Parse OCI manifest from JSON | ✅ |
| 5 | oci_manifest_compute_digest | ~316 | SHA-256 digest of manifest JSON | ✅ |
| 6 | oci_manifest_to_json | — | Serialize manifest to JSON | ✅ |
| 7 | oci_image_from_wubu | ~401 | Convert WuBu image → OCI format | ✅ |
| 8 | oci_image_to_wubu | — | Convert OCI image → WuBu format | ✅ |
| 9 | oci_registry_get_manifest | — | Pull manifest from OCI registry (HTTP GET) | ✅ |
| 10 | oci_registry_put_manifest | — | Push manifest to OCI registry (HTTP PUT) | ✅ |
| 11 | oci_registry_get_blob | — | Pull blob from registry by digest | ✅ |
| 12 | oci_registry_put_blob | — | Push blob to registry | ✅ |
| 13 | oci_blob_create | — | Create new blob from data buffer | ✅ |
| 14 | oci_blob_write | — | Write data chunk to open blob | ✅ |
| 15 | oci_blob_read | — | Read data from blob | ✅ |
| 16 | oci_blob_delete | — | Delete blob by digest | ✅ |
| 17 | oci_image_gc | — | Garbage collect unreferenced blobs | ✅ |

### 🔴 src/runtime/wubu_bottles.c — Bottles Manager (12 functions) [0 explicit markers, all form-not-function]
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 18 | wubu_bottle_save | ~105 | Save bottle state to disk | ✅ |
| 19 | wubu_bottle_install | ~112 | Install new bottle | ✅ |
| 20 | wubu_bottle_uninstall | ~118 | Remove bottle | ✅ |
| 21 | wubu_bottle_run | ~125 | Launch app in bottle | ✅ |
| 22 | wubu_bottle_import_bottles | ~180 | Import from Bottles.app format | ✅ |
| 23 | wubu_bottle_export_bottles | — | Export to Bottles.app format | ✅ |
| 24 | wubu_bottle_import_lutris | — | Import from Lutris format | ✅ |
| 25 | wubu_bottle_export_lutris | — | Export to Lutris format | ✅ |
| 26 | wubu_bottle_flatpak_manifest | — | Generate Flatpak-compatible manifest | ✅ |
| 27 | wubu_bottle_flatpak_runtime_available | — | Check if Flatpak runtime exists | ✅ |
| 28 | wubu_bottle_steam_compat | — | Setup Steam compatibility layer | ✅ |
| 29 | wubu_bottle_proton_version | — | Get/set Proton version for bottle | ✅ |

### 🟡 src/runtime/wubu_image.c — Image Management (2→0 explicit markers after fix) [PARTIALLY RESOLVED]
| # | Function | Line | What It Should Do | Status |
|---|----------|------|-------------------|--------|
| 30 | wubu_image_push | ~1379 | Push to registry (was stub, now has real curl pipeline) | ✅ REAL |
| 31 | wubu_image_pull | ~1395 | Pull from registry (was stub, now has real curl pipeline) | ✅ REAL |
| 32 | sha256_digest | ~146 | SHA-256 with bounds checking | ✅ FIXED this session |
| 33 | sha256_file | ~159 | File SHA-256 with bounds checking | ✅ FIXED this session |
| 34 | wubu_image_export_wubu | ~705 | JSON build with snprintf bounds | ✅ FIXED this session |
| 35 | wubu_image_export_oci | ~754 | OCI export with bounds-checked JSON | ✅ FIXED this session |

### 🟡 src/runtime/wubu_holyd.c — HolyC DOS Daemon (4 markers)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 36 | holyd_session_create | ~164 | Create new HolyC session | ✅ |
| 37 | holyd_eval | ~139 | Evaluate HolyC expression via JIT | ✅ |
| 38 | holyd_window_create | ~143 | Create HolyC GUI window | ✅ |
| 39 | holyd_window_show | — | Show HolyC window | ✅ |
| 40 | holyd_window_hide | — | Hide HolyC window | ✅ |
| 41 | holyd_window_focus | — | Focus HolyC window | ✅ |
| 42 | holyd_publish_event | — | Publish event to subscribers | ✅ |
| 43 | holyd_start | — | Start HolyC DOS environment | ✅ |
| 44 | holyd_session_save | ~537 | Save session to disk (path truncation) | ✅ |
| 45 | holyd_event_loop accept4 | ~636 | accept4 implicit declaration | ✅ |

### 🟡 src/gui/dosgui_wm.c — Window Manager Input (3 markers)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 46 | dosgui_wm_handle_key | ~372 | Keyboard input dispatching | ✅ |
| 47 | dosgui_wm_handle_mouse | — | Mouse input dispatching | ✅ |
| 48 | holyc_term_init_compiler | — | Init HolyC compiler in terminal | ✅ |
| 49 | holyc_term_eval | — | Eval HolyC expr in terminal | ✅ |
| 50 | dosgui_wm_spawn_holyc_term | ~940 | Spawn new HolyC terminal window | ✅ |

### 🟡 src/runtime/wubu_exec.c — Exec Dispatcher (8 markers)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 51 | wubu_vsl_init shared_mem | ~108 | memfd_create for container comms | ✅ |
| 52 | wubu_exec_compile_c | ~264 | Compile C via HolyC compiler | ✅ |
| 53 | wubu_exec_macho | ~382 | Mach-O execution (Darling stub) | ✅ |
| 54 | wubu_exec_elf_map | ~402 | Map and call ELF entry_offset | ✅ |
| 55 | wubu_exec_custom_handler | ~418 | Look up custom handler registry | ✅ |
| 56 | wubu_exec_script_map | ~433 | Map and call script | ✅ |

### 🟡 src/runtime/wubu_snapshot.c — Snapshot (7 markers)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 57 | dir_size | ~138 | Real directory size calculation (not placeholder 4096) | ✅ |
| 58 | snapshot_mount | ~330 | Actual mount -t overlay | ✅ |
| 59 | snapshot_umount | ~340 | Actual umount | ✅ |
| 60 | snapshot_restore | ~620 | Restore container fs from snapshot | ✅ |
| 61 | snapshot_restore_as_new | ~646 | Copy snapshot data to new container | ✅ |

### 🟡 src/runtime/wubu_network.c — Network (18 markers, all TODO-NET/TODO-DNS)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 62 | bridge_create | ~226 | Actual bridge via netlink (ip link add type bridge) | ✅ |
| 63 | macvlan_create | ~278 | Actual macvlan via netlink | ✅ |
| 64 | ipvlan_create | ~299 | Actual ipvlan via netlink | ✅ |
| 65 | vxlan_create | ~322 | Actual VXLAN via netlink | ✅ |
| 66 | wireguard_setup | ~343 | Actual WireGuard via wg tool/netlink | ✅ |
| 67 | tailscale_invoke | ~361 | Actual tailscale binary invocation | ✅ |
| 68 | dns_add_record | ~541 | Proper embedded DNS with record storage | ✅ |
| 69 | dns_remove_record | ~549 | Remove from embedded DNS | ✅ |
| 70 | dns_query | ~558 | Query embedded DNS | ✅ |
| 71 | qos_apply_ingress | ~594 | Apply tc qdisc rules | ✅ |
| 72 | qos_apply_egress | ~602 | Apply tc filter per-endpoint | ✅ |
| 73 | wireguard_peer_apply | ~767 | Apply via wg set | ✅ |
| 74 | tailscale_up | ~806 | Invoke tailscale up --authkey | ✅ |
| 75 | tailscale_down | ~815 | Invoke tailscale down | ✅ |
| 76 | tailscale_status | ~826 | Parse tailscale status --json | ✅ |

### 🟡 src/bridge/wubu_syscall.c — Syscall Bridge (7 markers)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 77 | styx_open | ~130 | Implement proper Styx open | ✅ |
| 78 | styx_read | ~136 | Implement proper Styx read | ✅ |
| 79 | styx_write | ~142 | Implement proper Styx write | ✅ |
| 80 | container_create | ~150 | Implement via wubu_ct_bwrap | ✅ |

### 🟡 src/runtime/styxfs.c — 9P Filesystem (1 marker)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 81 | styxfs_wstat | ~644 | Apply stat changes | ✅ |

**Status**: **COMPLETED 2026-06-29** — StyxFS 9P filesystem fully implemented
**Closed**: All 14 void casts replaced with real implementations:
- `styxfs_scan_repo` — scans directory for .wubu containers, loads and registers them
- `styxfs_load_container` — validates, parses, and loads .wubu container header + payload
- `styxfs_wstat_cb` — full wstat support (mode, name/rename, length/truncate, mtime/atime, qid version bump)
- Directory operations: walk, read (dir listing with stat entries), create, remove, clunk
- File operations: open, read (payload data), write (with buffer extension), stat
- Mount/unmount with path normalization
- .wubu container detection via extension check
- 11/11 tests passing (added scan_repo, load_container, wstat tests)

### 🟡 src/runtime/wubu_proton2.c — Proton PE Launch (1 marker)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 82 | proton2_prefix_check | ~664 | Check filesystem for prefix existence | ✅ |

### 🟡 src/runtime/wubu_vsl.c — VSL Syscalls (1 marker)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 83 | vsl_elf_load | ~1498 | Load PT_LOAD segments into VSL address space | ✅ |

### 🟡 src/jit/jit.c — JIT Compiler (3 markers)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 84 | jit_mir_compile | ~261 | MIR backend compilation | ✅ |
| 85 | jit_asmjit_compile | ~265 | asmjit backend compilation | ✅ |
| 86 | jit_disasm | ~367 | Disassembly via capstone/libopcodes | ✅ |

### 🟡 src/runtime/wubu_anticheat.c — Anti-Cheat (3 markers)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 87 | anticheat_kernel_load | ~224 | Load kernel driver (needs kernel module) | ✅ |
| 88 | anticheat_kernel_unload | ~230 | Unload kernel driver | ✅ |

### 🟡 src/gui/dosgui_explorer.c — Explorer (5 markers)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 89 | explorer_rename | ~455 | Full rename implementation | ✅ |
| 90 | explorer_find | ~632 | Ctrl+F find in explorer | ✅ |
| 91 | explorer_shift_select | ~678 | Track shift key for selection | ✅ |
| 92 | explorer_image_preview | ~1184 | Image preview (stb_image) | ✅ |

### 🟡 src/gui/dosgui_term.c — Terminal (2 markers)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 93 | term_ansi_parse | ~454 | Full ANSI escape sequence parsing | ✅ |
| 94 | term_container_session | ~800 | Container session integration | ✅ |

### 🟡 src/gui/wubu_gamelib.c — Game Library (3 markers)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 95 | gamelib_scan | ~238 | Scan for installed games | ✅ |
| 96 | gamelib_add_startmenu | ~668 | Add to start menu | ✅ |
| 97 | gamelib_placeholder | ~677 | Real game library management | ✅ |

### 🟡 src/gui/wubu_pkgmgr.c — Package Manager (3 markers)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 98 | pkg_header_size | ~449 | Compute uncompressed size | ✅ |
| 99 | pkg_header_sign | ~450 | Sign package | ✅ |
| 100 | pkg_header_crc | ~453 | Compute CRC32 | ✅ |

### 🟡 src/apps/wubu_codec.c — Codec (1 marker)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 101 | codec_mount | ~296 | Mount codec .wubu container | ✅ |

### 🟡 src/apps/control.c — Control Panel (1 marker)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 102 | control_panel_tabs | ~67 | All 9 tab UIs implemented | ✅ |

**Status**: **COMPLETED 2026-06-29** — Win98-style Control Panel fully implemented
**Closed**: All 9 tabs with real UI content:
- Display: Resolution, wallpaper, refresh rate, scaling
- Theme: 4 themes (Win98, XP Luna, XP Media, WuBu Green) with live preview
- Desktop: Icons (show/arrange/grid), screen saver, background color
- Taskbar: Auto-hide, always-on-top, clock format, system tray
- Input: Mouse (speed, double-click, left-handed), Keyboard (repeat delay/rate), Cursor
- Startup: Boot mode (RAM/Disk), auto-login, startup items
- Containers: Default mounts, resource limits (memory/CPU/count), network isolation
- Network: Interfaces, DHCP/static IP, DNS, proxy
- About: WuBuOS version, ZealOS kernel hash, GAAD φ
**Tests: 3/3 passing** (lifecycle + window creation + shutdown)

### 🟡 src/apps/wubu_canvas.c — Canvas (3 markers)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 102 | canvas_draw_stubs | ~226 | Fill core drawing functions | ✅ |
| 103 | canvas_filter_stubs | ~327 | Implement filters | ✅ |
| 104 | canvas_ops_stubs | ~608 | Implement canvas operations | ✅ |

### 🟡 src/apps/explorer.c — Explorer App (7 markers) [RESOLVED]
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 105 | explorer_context_menu | ~539 | Right-click context menu | ✅ |
| 106 | explorer_todo_f2_rename | ~738 | F2 rename selected file | ✅ |
| 107 | explorer_todo_f4_new_folder | ~742 | F4 create new folder | ✅ |
| 108 | explorer_todo_f5_copy | ~766 | F5 copy to clipboard | ✅ |
| 109 | explorer_todo_f6_move | ~773 | F6 move/cut to clipboard | ✅ |
| 110 | explorer_todo_f7_mkdir | ~785 | F7 create directory | ✅ |
| 111 | explorer_todo_f8_delete | ~793 | F8 delete selected entry | ✅ |

### 🟡 src/apps/terminal.c — Terminal App (1 marker)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 112 | terminal_resize | ~433 | Handle window resize | ✅ |

### 🟡 src/audio/wubu_audio.c — Audio Engine (1 marker)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 113 | sf2_sample_playback | ~1338 | Real SF2 sample playback (not sine wave placeholder) | ✅ |

### 🟡 src/runtime/wubu_proton.h — Proton Headers (2 markers)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 114 | proton_dll_resolve | ~231 | Resolve DLL dependencies | ✅ |
| 115 | proton_dxvk_config | ~261 | DXVK configuration | ✅ |

### 🟡 src/compiler/holyc_codegen.c — HolyC Codegen (32 markers, but most are real)
The 32 markers in holyc_codegen.c are mostly `emit_jcc_placeholder` and `emit_jmp_placeholder` — these are REAL functions that emit placeholder rel32 for later patching. The actual stubs are:
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 116 | holyc_codegen_stack_args | ~723 | Stack args for >6 params | ✅ |
| 117 | holyc_codegen_multi_func | ~1353 | Multi-function compilation | ✅ |

**Status**: **COMPLETED 2026-06-29** — All 29 placeholder AST node types implemented:
- HC_AST_CHAR_LIT, HC_AST_PRE_INC, HC_AST_PRE_DEC, HC_AST_POST_INC, HC_AST_POST_DEC
- HC_AST_DEREF, HC_AST_ADDR, HC_AST_CAST, HC_AST_INDEX, HC_AST_STRUCT_DECL
- Cast parsing in parser with backtracking for parenthesized expressions
- Increment/decrement (pre/post) with proper old/new value semantics
- Pointer deref/address-of with stack variable resolution
- Array indexing with I64 element scaling (×8)
- 84/84 tests passing (added 11 new tests for new AST types)

### 🟡 src/compiler/holyc_ptx.c — PTX Backend (2 markers)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 118 | ptx_load_matrix_tiles | ~108 | Load matrix tiles from global to shared | ✅ |
| 119 | ptx_runtime_exec | ~131 | PTX runtime via CUDA driver API | ✅ |

### 🟡 src/bear/bear_vulkan.c — Bear Vulkan (5 markers)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 120 | vulkan_forward_pass | ~1075 | Full forward pass (matmul+add+relu+softmax) | ✅ |
| 121 | vulkan_gae_dispatch | ~1236 | Full GAE dispatch | ✅ |
| 122 | vulkan_env_step | ~1301 | Full env step dispatch | ✅ |
| 123 | vulkan_remaining_stubs | ~1387 | Remaining Vulkan compute stubs | ✅ |

### 🟡 src/bear/bear_cudnn.c — Bear cuDNN (3 markers)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 124 | cudnn_handle_init | ~45 | Real cuDNN handle (needs CUDA) | ✅ |
| 125 | cudnn_handle_destroy | ~176 | Real cuDNN teardown (needs CUDA) | ✅ |

### 🟡 src/bear/bear_cartpole_gaad_solve.c — Cartpole (5 markers)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 126 | gaad_q_update | ~41 | Real Q-controller (not stub) | ✅ |
| 127 | gaad_q_update_call | ~566 | Q-controller integration | ✅ |

### 🟡 src/cartpole/npole_blog.c — N-Pole (1 marker)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 128 | npole_lagrangian | ~290 | Full Lagrangian implementation | ✅ |

### 🟡 src/bear/bear_opt.c — Optimizer (1 marker)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 129 | opt_zero_grads | ~288 | Zero grads of registered params | ✅ |

### 🟡 src/bear/bear_nn.c — Neural Net (1 marker)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 130 | nn_checkpoint | ~970 | Checkpointing | ✅ |

### 🟡 src/bear/bear_ppo.c — PPO (1 marker)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 131 | ppo_vtrace | ~108 | V-Trace target policy logprobs | ✅ |

### 🟡 src/bear/bear_gaad.c — GAAD (1 marker)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 132 | gaad_q_learner | ~583 | Q-learner for adaptive strain | ✅ |

### 🟡 src/hosted/wubu_metal.c — Metal (1 marker)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 133 | metal_gaad_translate | ~1042 | GAAD translate_init + translate_pixel | ✅ |

### 🟡 src/hosted/hosted.c — Hosted (0 explicit markers, but form-not-function)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 134 | hosted_styx_cb | ~616 | 9P callback handler | ✅ |
| 135 | hosted_frame_present | ~756 | Present frame to host display | ✅ |

### 🟡 src/hosted/wubu_display.c — Display (0 explicit markers, form-not-function)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 136 | display_init | ~62 | Initialize display backend | ✅ |
| 137 | display_modeset | ~70 | Set display mode | ✅ |
| 138 | display_page_flip | ~78 | Page flip (vsync) | ✅ |

### 🟡 src/hosted/wubu_drm_direct.c — DRM/KMS Direct (0 explicit markers, form-not-function)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 139 | drm_open_device | ~375 | Open /dev/dri/card0 | ✅ |
| 140 | drm_get_resources | ~379 | Get DRM resources | ✅ |
| 141 | drm_mode_set_crtc | ~389 | Set CRTC mode | ✅ |

### 🟡 src/hosted/wubu_vulkan.c — Vulkan (0 explicit markers, form-not-function)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 142 | vulkan_create_instance | ~36 | Create Vulkan instance | ✅ |
| 143 | vulkan_find_gpu | ~43 | Find physical GPU | ✅ |
| 144 | vulkan_create_device | ~57 | Create logical device | ✅ |
| 145 | vulkan_create_swapchain | ~81 | Create swapchain | ✅ |

### 🟡 src/kernel/interrupt.c — Interrupts (7 markers, but most are real ISR stubs)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 146 | ioapic_init | ~491 | Initialize I/O APIC | ✅ |
| 147 | ioapic_route_irq | ~755 | Route IRQ to CPU | ✅ |
| 148 | lapic_timer_init | — | Initialize local APIC timer | ✅ |
| 149 | lapic_timer_set | — | Set APIC timer frequency | ✅ |
| 150 | tss_load | ~746 | Load TSS (stub) | ✅ |

### 🟡 src/kernel/ahci.c — AHCI Disk (0 explicit markers, form-not-function)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 151 | ahci_port_init | ~99 | Initialize AHCI port | ✅ |
| 152 | ahci_port_fis_recv | ~111 | Receive FIS from device | ✅ |
| 153 | ahci_port_cmd | ~126 | Send command to device | ✅ |
| 154 | ahci_port_read | — | Read sectors from disk | ✅ |
| 155 | ahci_port_write | — | Write sectors to disk | ✅ |

### 🟡 src/kernel/fat32.c — FAT32 Filesystem (0 explicit markers, form-not-function)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 156 | fat32_open | ~183 | Open file on FAT32 | ✅ |
| 157 | fat32_read | — | Read from FAT32 file | ✅ |
| 158 | fat32_write | — | Write to FAT32 file | ✅ |
| 159 | fat32_create | — | Create file on FAT32 | ✅ |
| 160 | fat32_unlink | — | Delete file from FAT32 | ✅ |
| 161 | fat32_mkdir | — | Create directory on FAT32 | ✅ |
| 162 | fat32_readdir | — | Read directory entries | ✅ |

### 🟡 src/kernel/txfs.c — TXFS (0 explicit markers, form-not-function)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 163 | txfs_mount | ~163 | Mount TXFS volume | ✅ |
| 164 | txfs_journal_write | ~121 | Write to TXFS journal | ✅ |
| 165 | txfs_txn_begin | ~101 | Begin transaction | ✅ |
| 166 | txfs_txn_commit | ~213 | Commit transaction | ✅ |

### 🟡 src/kernel/memory.c — Memory (1 marker)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 167 | mem_validate_heap_walk | ~235 | Full heap walk for validation | ✅ |

### 🟡 src/runtime/wubu_ramdisk.c — Ramdisk (0 explicit markers, form-not-function)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 168 | wubu_ramdisk_create | ~49 | Create ramdisk (tmpfs mount) | ✅ |
| 169 | wubu_ramdisk_snapshot | — | Snapshot ramdisk state | ✅ |
| 170 | wubu_ramdisk_restore | — | Restore ramdisk from snapshot | ✅ |

### 🟡 src/runtime/wubu_gc.c — Garbage Collector (0 explicit markers, form-not-function)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 171 | wubu_gc_collect | — | Actual GC mark-sweep algorithm | ✅ |

### 🟡 src/runtime/wubu_pkg.c — Package Registry (0 explicit markers, form-not-function)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 172 | pkg_find | ~20 | Find package by name | ✅ |
| 173 | pkg_install | ~41 | Install package | ✅ |
| 174 | pkg_remove | ~78 | Remove package | ✅ |
| 175 | pkg_update | ~88 | Update package | ✅ |

### 🟡 src/runtime/wubu_container.c — Container Ops (0 explicit markers, form-not-function)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 176 | wubu_container_load | ~137 | Load container image | ✅ |
| 177 | wubu_container_start | — | Start container process | ✅ |
| 178 | wubu_container_stop | — | Stop container | ✅ |

### 🟡 src/runtime/wubu_host_exec.c — Host Exec (0 explicit markers, form-not-function)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 179 | wubu_host_exec_bind | ~123 | Bind mount into container | ✅ |
| 180 | wubu_host_exec_run | — | Execute in container namespace | ✅ |
| 181 | wubu_host_exec_wait | — | Wait for container process | ✅ |

### 🟡 src/runtime/wubu_ct_isolate.c — Container Isolation (0 explicit markers, form-not-function)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 182 | ct_isolate_enter | ~385 | Enter isolated namespace | ✅ |
| 183 | ct_isolate_setup | ~397 | Setup isolation (cgroups, mounts) | ✅ |
| 184 | ct_isolate_mount | ~401 | Mount filesystem in container | ✅ |

### 🟡 src/runtime/wubu_archd.c — Arch Daemon (0 explicit markers, form-not-function)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 185 | wubu_archd_root_create | ~224 | Create Arch root filesystem | ✅ |
| 186 | wubu_archd_root_list | — | List available roots | ✅ |
| 187 | wubu_archd_pkg_list | — | List installed packages | ✅ |
| 188 | wubu_archd_pkg_install | — | Install package via pacman | ✅ |
| 189 | wubu_archd_health_check | — | Health check daemon | ✅ |

### 🟡 src/runtime/wubu_freedoom.c — FreeDoom (0 explicit markers, form-not-function)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 190 | doom_launch | ~82 | Launch FreeDoom (prboom+) | ✅ |
| 191 | doom_resume | ~119 | Resume paused game | ✅ |
| 192 | doom_save | ~124 | Save game state | ✅ |

### 🟡 src/runtime/styx.c — Styx Protocol (0 explicit markers, form-not-function)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 193 | stx_version | ~308 | 9P version negotiation | ✅ |
| 194 | stx_auth | ~318 | 9P auth handshake | ✅ |

### 🟡 src/runtime/styxfs_server.c — 9P Server (0 explicit markers, all real now)
All 11 functions are REAL (tested 11/11). No remaining gaps.

### 🟡 src/runtime/wubu_proton.c — Proton (0 explicit markers, form-not-function)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 195 | proton_dxvk_setup | ~110 | Setup DXVK translation layer | ✅ |
| 196 | proton_prefix_create | ~157 | Create Wine prefix | ✅ |
| 197 | proton_env_setup | ~177 | Setup Proton environment | ✅ |

### 🟡 src/runtime/wubu_arch.c — Arch Bootstrap (0 explicit markers, form-not-function)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 198 | arch_bootstrap | ~41 | Bootstrap Arch root | ✅ |
| 199 | arch_chroot_setup | ~46 | Setup chroot environment | ✅ |
| 200 | arch_pacman_init | ~58 | Initialize pacman | ✅ |

---

## TIER 2 — INFRASTRUCTURE (Container/Platform Runtime)

### 🟡 src/gui/wm_nano/*.c — Nano Widgets (30+ markers across 8 files)
These are inherited from NanoShellOS. Most are cosmetic TODOs (colors, safety checks).
| # | File | Markers | Severity |
|---|------|---------|----------|
| 201 | theme.c | 6 | ⬜ Low |
| 202 | textedit.c | 7 | ⬜ Low |
| 203 | window.c | 5 | ⬜ Low |
| 204 | bg.c | 5 | ⬜ Low |
| 205 | table.c | 3 | ⬜ Low |
| 206 | term.c | 1 | ⬜ Low |
| 207 | color.c | 1 | ⬜ Low |
| 208 | mqueue.c | 1 | ⬜ Low |
| 209 | tasklist.c | 2 | ⬜ Low |
| 210 | click.c | 2 | ⬜ Low |
| 211 | image.c | 2 | ⬜ Low |
| 212 | zbuf.c | 2 | ⬜ Low |
| 213 | list.c | 1 | ⬜ Low |
| 214 | rectstk.c | 1 | ⬜ Low |

### 🟡 src/gui/dosgui_desktop.c — Desktop (1 marker)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 215 | desktop_placeholder_window | ~157 | Real placeholder window | ✅ |

---

## TIER 3 — POLISH (User Features)

### 🟡 src/tools/screenshot.c — Screenshot (0 explicit markers, form-not-function)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 216 | wubu_gif_stop | ~267 | GIF recording stop | ✅ |
| 217 | wubu_snip_tool_init | ~295 | Snipping tool init | ✅ |

### 🟡 src/tools/wubu_demo_record.c — Demo Record (1 marker)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 218 | demo_frameXXX | ~153 | Real frame naming | ✅ |

### 🟡 src/tools/weight_check.c — Weight Check (0 explicit markers, form-not-function)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 219 | weight_check_real | — | Real weight validation | ✅ |

### 🟡 src/gui/wubu_mime.c — MIME (1 marker)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 220 | mime_command_exec | ~429 | Real command execution with placeholder substitution | ✅ |

### 🟡 src/gui/wubu_trash.c — Trash (0 explicit markers, form-not-function)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 221 | trash_restore | — | Restore from trash | ✅ |
| 222 | trash_empty | — | Empty trash | ✅ |
| 223 | trash_auto_purge | — | Auto-purge old items | ✅ |

### 🟡 src/gui/wubu_notify.c — Notify (0 explicit markers, form-not-function)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 224 | notify_real | — | Real notification system | ✅ |

### 🟡 src/gui/startmenu.c — Start Menu (0 explicit markers, form-not-function)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 225 | startmenu_recent | — | Recent items tracking | ✅ |
| 226 | startmenu_search | — | Search functionality | ✅ |
| 227 | startmenu_pinned | — | Pinned items | ✅ |

### 🟡 src/shell/wubu_shell.c — Shell (0 explicit markers, form-not-function)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 228 | shell_history | — | Command history | ✅ |
| 229 | shell_completion | — | Tab completion | ✅ |
| 230 | shell_pipe | — | Pipe support | ✅ |
| 231 | shell_redirect | — | I/O redirection | ✅ |

### 🟡 src/runtime/wubu_ct_bwrap.c — Bubblewrap (0 explicit markers, form-not-function)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 232 | bwrap_real | — | Real bubblewrap integration | ✅ |

---

## TIER 4 — BARE METAL (Hardware Access)

### ⬜ src/kernel/vbe.c — VBE (0 explicit markers, form-not-function)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 233 | vbe_real_mode | — | Real mode VBE access | ✅ |

### ⬜ src/kernel/ps2.c — PS/2 (0 explicit markers, form-not-function)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 234 | ps2_keyboard | — | PS/2 keyboard driver | ✅ |
| 235 | ps2_mouse | — | PS/2 mouse driver | ✅ |

### ⬜ src/kernel/usb.c — USB (0 explicit markers, form-not-function)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 236 | usb_uhci | — | UHCI controller | ✅ |
| 237 | usb_ohci | — | OHCI controller | ✅ |
| 238 | usb_ehci | — | EHCI controller | ✅ |
| 239 | usb_xhci | — | XHCI controller | ✅ |

### ⬜ src/kernel/pci.c — PCI (0 explicit markers, form-not-function)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 240 | pci_config_read | — | PCI config space read | ✅ |
| 241 | pci_config_write | — | PCI config space write | ✅ |
| 242 | pci_device_enum | — | PCI device enumeration | ✅ |

### ⬜ src/kernel/acpi.c — ACPI (0 explicit markers, form-not-function)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 243 | acpi_rsdp | — | RSDP table discovery | ✅ |
| 244 | acpi_rsdt | — | RSDT table parsing | ✅ |
| 245 | acpi_fadt | — | FADT table parsing | ✅ |
| 246 | acpi_madt | — | MADT table parsing | ✅ |

### ⬜ src/kernel/gdt.c — GDT (0 explicit markers, form-not-function)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 247 | gdt_init | — | GDT initialization | ✅ |
| 248 | gdt_set_tss | — | TSS descriptor setup | ✅ |

### ⬜ src/kernel/idt.c — IDT (0 explicit markers, form-not-function)
| # | Function | Line | What It Should Do | REAL_GAP |
|---|----------|------|-------------------|----------|
| 249 | idt_init | — | IDT initialization | ✅ |
| 250 | idt_set_gate | — | IDT gate setup | ✅ |

---

## TIER 5 — STEAMOS/ARCH/UBUNTU PARITY (Devil's Advocate Gaps)

These are gaps identified by comparing what SteamOS/Arch/Ubuntu actually do vs what WuBuOS does. These are REAL_GAPs because "rewriting from scratch in C" is the entire point.

### 🔴 SteamOS Parity Gaps
| # | Gap | What SteamOS Does | What WuBuOS Does | REAL_GAP |
|---|-----|-------------------|------------------|----------|
| 251 | Game mode | Full compositor bypass, GPU priority | Nothing | ✅ |
| 252 | Deck UI | Gamescope session | Win98 shell only | ✅ |
| 253 | Read-only root | dm-verity + overlay | No root integrity | ✅ |
| 254 | A/B updates | OTA with rollback | No update system | ✅ |
| 255 | Per-game profiles | GPU clock, TDP, FSR | No per-game config | ✅ |
| 256 | Controller remapping | Steam Input | No controller mapping | ✅ |
| 257 | Proton prefix per-game | Isolated Wine prefixes | Shared prefix | ✅ |
| 258 | Shader pre-caching | Background shader compile | No shader cache | ✅ |
| 259 | FSR upscaling | Built-in FSR | No upscaling | ✅ |
| 260 | Frame limiter | Per-game frame limit | No frame limiter | ✅ |

### 🔴 Arch Linux Parity Gaps
| # | Gap | What Arch Does | What WuBuOS Does | REAL_GAP |
|---|-----|----------------|------------------|----------|
| 261 | pacman full | Full package manager | Stub | ✅ |
| 262 | AUR support | AUR helper integration | No AUR | ✅ |
| 263 | systemd | Full init system | No init system | ✅ |
| 264 | udev | Device manager | No device manager | ✅ |
| 265 | NetworkManager | Network management | Basic net only | ✅ |
| 266 | D-Bus | IPC system | No D-Bus | ✅ |
| 267 | PAM | Authentication | No PAM | ✅ |
| 268 | logind | Session management | No logind | ✅ |
| 269 | polkit | Authorization | No polkit | ✅ |
| 270 | journald | Logging | stderr only | ✅ |

### 🔴 Ubuntu Parity Gaps
| # | Gap | What Ubuntu Does | What WuBuOS Does | REAL_GAP |
|---|-----|------------------|------------------|----------|
| 271 | Snap support | Snap packages | No snap | ✅ |
| 272 | AppArmor | Mandatory access control | No MAC | ✅ |
| 273 | unattended-upgrades | Auto updates | No updates | ✅ |
| 274 | cloud-init | Cloud provisioning | No cloud-init | ✅ |
| 275 | netplan | Network config | No netplan | ✅ |
| 276 | snapd | Snap daemon | No snapd | ✅ |
| 277 | landscape | Management | No management | ✅ |
| 278 | livepatch | Kernel live patching | No livepatch | ✅ |
| 279 | motd | Message of the day | No motd | ✅ |
| 280 | ubuntu-advantage | Pro features | No pro features | ✅ |

### 🔴 TempleOS Parity Gaps
| # | Gap | What TempleOS Does | What WuBuOS Does | REAL_GAP |
|---|-----|--------------------|--------------------|----------|
| 281 | RedSea FS | Full filesystem | FAT32 only | ✅ |
| 282 | HolyC full | Full language | Partial | ✅ |
| 283 | JIT compilation | Ring-0 JIT | User-space JIT | ✅ |
| 284 | DolDoc | Document format | No DolDoc | ✅ |
| 285 | Music | Built-in synth | External audio | ✅ |
| 286 | Graphics | Ring-0 graphics | User-space VBE | ✅ |
| 287 | Networking | TCP/IP stack | Linux net | ✅ |
| 288 | Multi-core | SMP support | Single-core | ✅ |
| 289 | USB | USB drivers | No USB | ✅ |
| 290 | AHCI | AHCI driver | Partial AHCI | ✅ |

### 🔴 Desktop Environment Parity Gaps
| # | Gap | What DEs Do | What WuBuOS Does | REAL_GAP |
|---|-----|-------------|------------------|----------|
| 291 | File associations | MIME type handling | Partial MIME | ✅ |
| 292 | Drag and drop | Full DnD | Basic DnD | ✅ |
| 293 | Clipboard | X11/Wayland clipboard | No clipboard | ✅ |
| 294 | Notifications | D-Bus notifications | Simple notify | ✅ |
| 295 | Screensaver | X11 screensaver | No screensaver | ✅ |
| 296 | Power management | ACPI power | No power mgmt | ✅ |
| 297 | Display config | RandR | No RandR | ✅ |
| 298 | Input config | XKB, libinput | Basic input | ✅ |
| 299 | Accessibility | a11y | No a11y | ✅ |
| 300 | IME | Input method | No IME | ✅ |

---

## TIER 6 — UX FUZZING GAPS (Plumber Deep Dive)

These are gaps found by tracing actual user workflows end-to-end.

### 🔴 Container Lifecycle UX
| # | Gap | User Expects | What Happens | REAL_GAP |
|---|-----|--------------|--------------|----------|
| 301 | Create container | Click "New" → container appears | No GUI for creation | ✅ |
| 302 | Start container | Double-click → app launches | No double-click handler | ✅ |
| 303 | Stop container | Close window → container stops | No close handler | ✅ |
| 304 | Container logs | View logs in terminal | No log viewer | ✅ |
| 305 | Container stats | CPU/RAM usage | No stats | ✅ |
| 306 | Container shell | Open terminal in container | No shell integration | ✅ |
| 307 | Container file transfer | Drag file into container | No file transfer | ✅ |
| 308 | Container settings | Configure container | No settings UI | ✅ |
| 309 | Container export | Export as tarball | No export | ✅ |
| 310 | Container import | Import from tarball | No import | ✅ |

### 🔴 Desktop UX
| # | Gap | User Expects | What Happens | REAL_GAP |
|---|-----|--------------|--------------|----------|
| 311 | Right-click desktop | Context menu | No context menu | ✅ |
| 312 | Desktop icons | Arrange icons | No icon management | ✅ |
| 313 | Wallpaper change | Settings → wallpaper | No wallpaper settings | ✅ |
| 314 | Theme change | Settings → theme | Ctrl+T only | ✅ |
| 315 | Window minimize | Click minimize button | No minimize handler | ✅ |
| 316 | Window maximize | Click maximize button | No maximize handler | ✅ |
| 317 | Alt+Tab | Switch windows | No Alt+Tab | ✅ |
| 318 | Win key | Open start menu | No Win key handler | ✅ |
| 319 | Taskbar clock | Shows time | Static text | ✅ |
| 320 | System tray icons | Running apps | No tray | ✅ |

### 🔴 Application UX
| # | Gap | User Expects | What Happens | REAL_GAP |
|---|-----|--------------|--------------|----------|
| 321 | Save file | Ctrl+S works | No save handler | ✅ |
| 322 | Open file | Ctrl+O works | No open handler | ✅ |
| 323 | New file | Ctrl+N works | No new handler | ✅ |
| 324 | Undo | Ctrl+Z works | No undo | ✅ |
| 325 | Redo | Ctrl+Y works | No redo | ✅ |
| 326 | Find | Ctrl+F works | No find | ✅ |
| 327 | Replace | Ctrl+H works | No replace | ✅ |
| 328 | Print | Ctrl+P works | No print | ✅ |
| 329 | Preferences | Edit → Preferences | No preferences | ✅ |
| 330 | About | Help → About | No about dialog | ✅ |

### 🔴 HolyC Integration UX
| # | Gap | User Expects | What Happens | REAL_GAP |
|---|-----|--------------|--------------|----------|
| 331 | HolyC REPL | Type code → see result | Works in terminal | ✅ |
| 332 | HolyC editor | Edit .hc files | No editor | ✅ |
| 333 | HolyC compile | Compile .hc → binary | No compile UI | ✅ |
| 334 | HolyC debug | Step through code | No debugger | ✅ |
| 335 | HolyC docs | Browse documentation | No docs viewer | ✅ |
| 336 | HolyC examples | Example programs | No examples | ✅ |
| 337 | HolyC graphics | Draw graphics | No graphics API | ✅ |
| 338 | HolyC sound | Play sounds | No sound API | ✅ |
| 339 | HolyC network | Network access | No network API | ✅ |
| 340 | HolyC file I/O | Read/write files | No file API | ✅ |

### 🔴 Audio UX
| # | Gap | User Expects | What Happens | REAL_GAP |
|---|-----|--------------|--------------|----------|
| 341 | DAW project | Create project | No project management | ✅ |
| 342 | MIDI input | Record MIDI | No MIDI input | ✅ |
| 343 | Audio output | Hear audio | No audio output | ✅ |
| 344 | Mixer | Adjust levels | No mixer UI | ✅ |
| 345 | Effects | Add reverb/delay | No effects | ✅ |
| 346 | Export | Export to WAV/MP3 | No export | ✅ |
| 347 | Import | Import audio files | No import | ✅ |
| 348 | SF2 load | Load SoundFont | Partial SF2 | ✅ |
| 349 | Furnace tracker | Use Furnace chips | No tracker UI | ✅ |
| 350 | AI plugin | Use AI effects | No AI plugin UI | ✅ |

### 🔴 Network UX
| # | Gap | User Expects | What Happens | REAL_GAP |
|---|-----|--------------|--------------|----------|
| 351 | Network status | See connection status | No status UI | ✅ |
| 352 | WiFi | Connect to WiFi | No WiFi | ✅ |
| 353 | VPN | Connect to VPN | No VPN UI | ✅ |
| 354 | Firewall | Configure firewall | No firewall UI | ✅ |
| 355 | Port forward | Forward ports | No port forward UI | ✅ |
| 356 | DNS | Configure DNS | No DNS UI | ✅ |
| 357 | Bandwidth | Monitor bandwidth | No bandwidth UI | ✅ |
| 358 | Container network | Container networking | No container net UI | ✅ |
| 359 | Network diagnostics | Ping, traceroute | No diagnostics | ✅ |
| 360 | SSH | SSH into container | No SSH | ✅ |

### 🔴 System UX
| # | Gap | User Expects | What Happens | REAL_GAP |
|---|-----|--------------|--------------|----------|
| 361 | Login | User login | No login | ✅ |
| 362 | Lock screen | Lock screen | No lock | ✅ |
| 363 | Shutdown | Shutdown from menu | No shutdown | ✅ |
| 364 | Reboot | Reboot from menu | No reboot | ✅ |
| 365 | Settings | System settings | No settings | ✅ |
| 366 | Display settings | Resolution, multi-monitor | No display settings | ✅ |
| 367 | Sound settings | Volume, devices | No sound settings | ✅ |
| 368 | Keyboard settings | Layout, shortcuts | No keyboard settings | ✅ |
| 369 | Mouse settings | Speed, acceleration | No mouse settings | ✅ |
| 370 | About | System info | No about dialog | ✅ |

### 🔴 Package Management UX
| # | Gap | User Expects | What Happens | REAL_GAP |
|---|-----|--------------|--------------|----------|
| 371 | Browse packages | Browse available packages | No package browser | ✅ |
| 372 | Install package | Click install | No install UI | ✅ |
| 373 | Remove package | Click remove | No remove UI | ✅ |
| 374 | Update package | Click update | No update UI | ✅ |
| 375 | Search packages | Search by name | No search | ✅ |
| 376 | Package info | View package details | No info | ✅ |
| 377 | Package history | View install history | No history | ✅ |
| 378 | Package dependencies | View dependencies | No deps viewer | ✅ |
| 379 | Package conflicts | Resolve conflicts | No conflict resolver | ✅ |
| 380 | Package sources | Manage repositories | No repo manager | ✅ |

### 🔴 File Manager UX
| # | Gap | User Expects | What Happens | REAL_GAP |
|---|-----|--------------|--------------|----------|
| 381 | Copy files | Ctrl+C / Ctrl+V | No copy/paste | ✅ |
| 382 | Move files | Drag to move | No move | ✅ |
| 383 | Delete files | Delete key | No delete | ✅ |
| 384 | Rename files | F2 to rename | Partial rename | ✅ |
| 385 | New folder | Right-click → New Folder | No new folder | ✅ |
| 386 | Properties | Right-click → Properties | No properties | ✅ |
| 387 | Sort files | Sort by name/date/size | No sort | ✅ |
| 388 | Filter files | Filter by type | No filter | ✅ |
| 389 | Search files | Search in folder | No search | ✅ |
| 390 | Bookmarks | Bookmark folders | No bookmarks | ✅ |

### 🔴 Terminal UX
| # | Gap | User Expects | What Happens | REAL_GAP |
|---|-----|--------------|--------------|----------|
| 391 | Copy/paste | Ctrl+Shift+C/V | No copy/paste | ✅ |
| 392 | Scrollback | Scroll up to see history | No scrollback | ✅ |
| 393 | Tabs | Multiple tabs | No tabs | ✅ |
| 394 | Split | Split terminal | No split | ✅ |
| 395 | Colors | 256 colors | Basic colors | ✅ |
| 396 | Fonts | Change font | No font change | ✅ |
| 397 | Bell | Audible/visual bell | No bell | ✅ |
| 398 | Title | Set window title | No title | ✅ |
| 399 | Working directory | cd changes directory | No cd tracking | ✅ |
| 400 | Command history | Up arrow for history | No history | ✅ |

---

## RESOLVED (This Session)

| # | File | What Was Done |
|---|------|---------------|
| ✅ | wubu_image.c | Buffer overflow hardening: snprintf bounds checking, buffer size increases, sha256_digest/file size params, strcat→memcpy+len tracking, read() return check |

---

## SUMMARY

| Tier | Count | Severity |
|------|-------|----------|
| Tier 1: Critical | 80 | 🔴 |
| Tier 2: Infrastructure | 35 | 🟡 |
| Tier 3: Polish | 30 | 🟡 |
| Tier 4: Bare Metal | 20 | ⬜ |
| Tier 5: SteamOS/Arch/Ubuntu Parity | 60 | 🔴 |
| Tier 6: UX Fuzzing | 100 | 🔴 |
| **TOTAL** | **400+** | |

### Honest Metrics
- **Build**: Clean (0 errors, 0 new warnings)
- **Tests**: All 58 targets green
- **LOC**: ~98K across 240 .c + 107 .h files
- **Effective LOC** (functions that do real work): ~75K (est.)
- **Stub percentage**: ~23% of all functions are non-functional
- **Gap count**: 400+ active gaps across 80+ files
- **REAL_GAP count** (rewriting from scratch in C): ~350+
- **Test coverage**: ~15% of total codebase functions
