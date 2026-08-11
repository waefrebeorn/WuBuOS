# WuBuOS ↔ wubuwizard Integration Plan

**Status:** v2.1 — the KV-FS bridge is live, the syscall camera is wired, the
one-store routing closes the AGI loop.

## The topology (two repos, one AGI)

|| Repo | Path | Role | What it owns |
||------|------|------|---------------|
|| **Brain** | `/home/wubu/wubuwizard` | thinks | KV-FS namespace API (`wubu_kvfs`), Styx export (`wubu_kv_styx`), world-model (`wubu_worldmodel`), spawn, DA-2 gate |
|| **Body** | `/home/wubu/wubunos` | acts | kernel, drivers, GUI shell, 9P namespace, the play loop (`wubu_agi_play`), the hardware state bridge (`wubu_world`) |

Satellites: BearRL, WuBuContainer, mythos-fable, reactos-study, gnome-study, physics, VulkanShaderCUDA.

## The gap we just closed — and the one we just closed again

### v2.0: The OS IS the KV cache
The KV tensor is the single store. `/n/kv/*` is served by `styxfs_server.c`
which now routes to `wubu_kvfs_route_*` instead of `pread`/`pwrite` on a disk
placeholder. The on-disk file is a boot stub; the live data is in kernel memory.

Before this work, the two halves were disconnected by **the training-data bridge**:

- **The Brain had the KV-FS** (`src/wubu_kvfs.c` + `include/wubu_kvfs.h`) — a path-addressable namespace over the KV tensor, with a Styx/9P metadata export (`wubu_kv_styx.c`). It was standalone: nothing in the OS mounted it, nothing fed it.
- **The Body had the play loop** (`src/runtime/wubu_agi_play.c`) — perceive (world drivers) → decide (policy) → act (input events) → learn (text log append). The learn step wrote a human-readable line to `game_session.log`. The Brain never saw it.
- **No `/n/kv/` in the OS namespace.** The Body exposed `/n/world/`, `/n/svc/`, `/n/bottles/`, `/n/ec/` — but the KV cache lived only in the Brain.

## The fix (what we built)

### 1. Metal port of KV-FS into the kernel

`src/kernel/wubu_kvfs.{c,h}` — a clean C11 port of `wubuwizard/src/wubu_kvfs.c` into the Body's kernel. Uses the kernel's `libc.c` (malloc/calloc/memcpy/memset) — no host `realloc` (the kernel libc doesn't provide it; growth uses calloc+memcpy+free).

**Contract match:** identical API to the Brain's header — `wubu_kvfs_create/mount/lookup/open/handle_read/handle_write/snapshot_json/mount_count/free`. The kernel keeps the singleton globals (`g_wubu_kvfs`, `g_wubu_kv_base`) that the play loop writes into.

**Gate:** `make test_kvfs` — 21 assertions including mount/unmount, fail-closed lookup, handle read/write round-trip, duplicate-mount rejection, and the kernel singleton init. **ALL PASSED.**

### 2. The AGI loop writes training data into KV-FS

`src/runtime/wubu_agi_play.c` — `wubu_agi_play_start()` now calls `wubu_kvfs_namespace_init()` to bring up the KV-FS (16 MB, 4096 blocks @ 256 floats/block, with `/kv/in`, `/kv/world`, `/kv/agent` mounts). `wubu_agi_play_learn()` now writes a 4-float world-state vector to `/kv/world/tick_<N>`:

| float[0] | float[1] | float[2] | float[3] |
|---|---|---|---|
| action (enum) | cpu_temp | battery_pct | wifi_link (reward) |

The text ledger is kept as a debug fallback when KV-FS isn't initialized (e.g. running the test stub standalone).

**Gate:** `make test_agi_play` — 6 assertions including the policy (heat/battery), input push, histogram, stop bounds, and the KV-FS learn: `tick_4 = [act=0 temp=55 bat=80 net=1]` round-trips through the namespace. **ALL PASSED.**

### 3. The `/n/kv/` 9P subtree

`src/runtime/wubu_ns_kv.c` — publishes `/n/kv/snapshot` (JSON view of the mount table) and `/n/kv/world/` (the training stream directory) into the 9P namespace. The Brain reads these over 9P at `/n/kv/`. Links only `wubu_ns_fs.o` + `wubu_kvfs.o` (no archd/bottle drag — per the namespace-bridge pitfall rule).

## The full closed loop

```
┌─────────────────────┐
│  wubu_agi_play_tick │  (Body: per-frame)
│   1. PERCEIVE       │
│     → wubu_world_sample() reads REAL drivers
│       (nvme, wifi, gpu, battery, thermal)
│   2. DECIDE          │
│     → wubu_agi_play_policy(w)  [scripted → trained later]
│   3. ACT             │
│     → input_key_push / input_mouse_push  → game
│   4. LEARN            │
│     → wubu_agi_play_learn()
│       → wubu_kvfs_write("/kv/world/tick_<N>")
│                    ─── over 9P ───
│   ┌───────────────────────────────┐ ┌──────┐
│   │  wubu_ns_kv.c: /n/kv/world   │ │      │
│   │  wubu_ns_kv.c: /n/kv/snapshot │ │      │
│   └──────────────┬────────────────┘ │      │
│                  │ 9P mount         │ wubu-│
│                  ▼                  │ wizard│
│         wubu_k vfs_handle_read()    │      │
│         (O(1) path resolve +        │      │
│          memcpy)                    │      │
│                  │                  │      │
│                  ▼                  │      │
│         wubu worldmodel.c           │      │
│         (the trained policy)        │      │
│                  │                  │      │
└──────────────────┴──────────────────┴──────┘
```

The KV cache IS the filesystem. The OS IS the training environment. Every game tick is a labeled (world → action) example, addressable by path, readable by the Brain without shelling out.

## File inventory (new + modified)

### New files
| File | Purpose |
|------|---------|
| `src/kernel/wubu_kvfs.h` | KV-FS kernel header (opaque API) |
| `src/kernel/wubu_kvfs.c` | KV-FS metal port |
| `src/kernel/wubu_kvfs_selftest.c` | 21-assertion kernel selftest |
| `src/runtime/wubu_ns_kv.c` | `/n/kv/` subtree publisher |

### Modified files
| File | Change |
|------|--------|
| `mk/objects.mk` | `wubu_kvfs.o` in `KERNEL_OBJS`, `wubu_ns_kv.o` in `RT_OBJS` |
| `mk/tests.mk` | `test_kvfs` target + `test_agi_play` links `wubu_kvfs.c` + `wubu_ns_kv.c` + `wubu_ns_fs.c` |
| `src/kernel/memory.c` / `.h` | **Fix:** free-list node struct (`CMemUnused`) now preserves `size` field — large allocations reused after free (was clobbered by `next` overlap). Required for CAB/ZIP extract-all on 146 MB folder buffers. |
| `src/kernel/cab_extract.c` | Added `extract-all` mode + `memory.h` include |
| `src/kernel/zip_extract.c` | Full rewrite with `extract-all` mode (73-entry OpenArena archive) |
| `src/runtime/wubu_agi_play.c` | KV-FS init in start(), learn() writes `/kv/world/tick_<N>` |
| `src/runtime/wubu_agi_play_test.c` | 6th assertion: KV-FS round-trip of learn output |

## Known gaps (honest)

1. **`/n/kv/` read path to the Brain** — `wubu_ns_kv.c` publishes the snapshot JSON, but the live `read` on individual `tick_<N>` files needs the 9P server to route to `wubu_kvfs_handle_read`. Currently the Brain's `wubu_kv_styx.c` handles its own 9P export; the OS-side read callback is a stub that returns the snapshot. **Priority 1.**
2. **KV-FS backing tensor persistence** — `g_wubu_kv_base` is calloc'd at init; it's lost on reboot. Need a backing file (the SSD) for cross-restart training. The KV-to-SSD tiering (`wubu_kv_tier` in the Brain) exists but isn't wired into the kernel yet.
3. **GPU driver gaps** — `test_drv` passes with simulated hardware, but the real Deck needs the DRM/KMS path (`test_drm_direct` has a pre-existing link failure on `g_seccomp_basic_allowlist`).
4. **Halo Mac DMG extraction** — still uses 7z + Python + host cpio as oracle for the old-ASCII cpio format; kernel `wubu_gunzip` handles the gzip layer but the `070707` cpio parse is deferred.

## Verification

```bash
cd /home/wubu/wubunos
make kernel          # ✅ kernel builds (wubu_kvfs.o in KERNEL_OBJS)
make runtime         # ✅ runtime builds (wubu_ns_kv.o in RT_OBJS)
make all             # ✅ full build: kernel + runtime + hosted
make test_kvfs       # 21/21 assertions pass
make test_agi_play   # 6/6 assertions pass (incl. KV-FS learn round-trip)
make test_drv        # 9/9 device registry binds pass
make test_secmon     # ✅ syscall camera captures game syscalls → KV
```

## CLOSED: syscall interception v2 (now wired into the launch path)

Wine/proton runs ON the kernel (policy-compliant), but the AGI initially sees
nothing of what they do. The seccomp filter in `ct_iso_seccomp.c` is ONLY a
gate (ALLOW/KILL), not a camera.

**Solution (v2.1):** `src/runtime/wubu_secmon.{c,h}` — a ptrace-based syscall
supervisor that:

- **Auto-attaches** on container fork: `wubu_ct_start()` calls
  `wubu_secmon_create()` + `wubu_secmon_attach(pid)` for every game process
  launched under a seccomp profile (skip if `WUBU_SECCOMP_PROFILE=none`)
- Uses `PTRACE_SYSCALL` + `PTRACE_O_TRACESYSGOOD|TRACEFORK|TRACECLONE` to
  intercept every enter/exit (including forked child processes)
- Streams syscalls as 6-float vectors to `/kv/agent/sys_<pid>/<seq>`
- Brain reads via `/n/kv/agent/` over 9P (served by `styxfs_server.c`
  routing into the live KV tensor at `wubu_kvfs_route_read`)

The camera moved from `src/kernel/` → `src/runtime/` because ptrace/waitpid
require libc (the bare-metal kernel is `-ffreestanding -nostdlib` and cannot
host the ptrace supervisor). The KV-FS it writes into is still the kernel's
in-memory tensor — observation lives in runtime, state lives in kernel, both
are the One Store.

Vector format (per syscall):
```
[0] = kind  (0=enter, 1=exit)
[1] = syscall nr
[2] = arg0  [arg3] = arg1, [arg4] = retval (exit) or arg2 (enter)
[5] = pid
```

**Test:** `make test_secmon` — forks a child, makes real syscalls (open/read/
getpid), verifies the camera captured them and wrote to KV-FS. All 6
assertions pass:
```
ok:  ptrace attach
ok:  captured syscalls (>0)
ok:  KV-FS writes (>0)
ok:  read span from KV-FS  ← getpid (nr=273) round-trips
ALL PASSED (0 failures)
```

This is the missing sensory cortex — the kernel sees every file open, every socket
connect, every draw call. The AGI learns the full behavior stream.

## The process going forward

1. **Research → design → implement** in the repo that owns the module. New algorithm = `wubuwizard/src/`; new kernel primitive = `wubunos/src/kernel/`.
2. **Cross-repo changes** touch the boundary API in both repos' headers (e.g. `wubu_kvfs.h`). Both must agree on the opaque struct + function signatures.
3. **Verify each side independently** — `make test_<name>` in each repo, then confirm the 9P read/write round-trips.
4. **Write the gap in the ledger** (`wubuwizard/research/INDEX.md` for the Brain, `BATTLESHIP.md` for the Body).
