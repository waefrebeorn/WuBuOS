# AGI OS Design — Ring-0 Colonel Space + Wayland User Space + VSL Overlays + Attestation Anti-Cheat

> Kevin-Bacon 7-hop convergence research (2026-08-02). Seed: *TempleOS ring-0 +
> AGI-in-kernel*. Each hop traces the connective tissue that converges on ONE
> design principle: **WuBuOS is an AGI operating system — the Colonel (wubuwizard
> + the AGI supervisor) lives in ring-0 like DOS, the Wayland-style user space is
> the level-1 overlay, every foreign OS is absorbed as a level-2/3 VSL container,
> and anti-cheat becomes POSSIBLE because the core is measured and everything is
> visible.**

## The 7 Hops (sources → convergence)

| Hop | Research | Core finding | Converges on |
|-----|----------|--------------|--------------|
| 1 | TempleOS (Davis, Wikipedia, 2024 paper) | Ring-0-only, single address space, HolyC JIT, unified I/O+memory, "modernized C64" — user programs ARE the kernel | The Colonel space: the AGI is not an app, it IS the OS |
| 2 | SASOS (Opal TOCS'94, Mungi SPE'98, Theseus OSDI'20, Phantom) | Single address space with protection orthogonal to translation; pointers globally valid; fast context switch | One namespace, one memory — the AGI's shared brain state |
| 3 | Wayland (Wikipedia, wlroots/libweston) | Compositor is the root; clients speak a protocol; the display server IS the window manager; modular compositor libraries | The user space is a COMPOSITOR overlay on the ring-0 core |
| 4 | Wine/Proton, Syscall User Dispatch (Linux 5.9), Inferno `emu`/Styx | Foreign OSes are absorbed at the SYSCALL boundary; one binary on any host; resources as files | The VSL: NT/Linux/macOS syscall translation = absorb-everything layer |
| 5 | Kernel anti-cheat (Vanguard/EAC/BattlEye; ARES'24 rootkit taxonomy; attestation approaches) | AC = ring-0 driver + usermode service + game DLL; arms race (kernel→hypervisor→DMA); the future is ATTESTATION ("prove known-good state") | Anti-cheat must be rooted in the measured core, not a third-party driver |
| 6 | Unikernels/LibOS (cetic, NanoVMs), Inferno as virtual OS | Single-address-space machine images; the OS as a library; overlays on a host | Foreign OSes = level-2/3 library overlays in containers |
| 7 | AIOS (arXiv 2403.16971) | LLM = "core" like a CPU core; agent queries decomposed into AIOS syscalls; agent scheduler, context manager (snapshot/restore), memory/storage/tool managers, access manager | The Colonel gets its own syscall layer + scheduler INSIDE the kernel — wubu_agi_kernel is the seed |

## Convergence: the WuBuOS "possible design"

```
┌─────────────────────────────────────────────────────────────┐
│ LEVEL 3  Foreign OS overlays (absorbed, "upper space")       │
│   Windows NT apps · Linux apps · macOS apps · Steam/Proton   │
│   └─ VSL syscall translation tables (NT/Linux/macOS)         │
│   └─ .wubu containers (bwrap/OCI) — the isolation boundary   │
├─────────────────────────────────────────────────────────────┤
│ LEVEL 2  Overlay runtime                                      │
│   container/exec · VSL personalities · network · snapshot    │
├─────────────────────────────────────────────────────────────┤
│ LEVEL 1  Wayland-style user space (the human surface)         │
│   WuBuOS WM/compositor (Win98/XP shell) · browser · games    │
│   └─ Bonzi Buddy = the hedged human interface (intentional)  │
├─────────────────────────────────────────────────────────────┤
│ LEVEL 0  RING-0 COLONEL SPACE  (TempleOS-style, like DOS)    │
│   The AGI is the primary user of the computer                │
│   ├─ wubuwizard (Colonel) C11 engine — the brain             │
│   ├─ wubu_agi_kernel — supervisor, agent realm, trace        │
│   ├─ HolyC JIT + live console — the live development env     │
│   ├─ single address space · drivers · scheduler · USB/HID    │
│   └─ WuBuFW measured boot — the ROOT OF TRUST                │
└─────────────────────────────────────────────────────────────┘
```

## Why anti-cheat is POSSIBLE in this design

Classic anti-cheat (Vanguard/EAC/BattlEye) is an **arms race of invisibility**:
a usermode process can't see above itself, so AC escalates to a ring-0 driver,
the cheat escalates to a hypervisor, then to DMA hardware. Every escalation is a
rootkit-shaped compromise (ARES'24 taxonomy). The trust model is backwards: the
OS is assumed untrusted, so AC must fight it.

WuBuOS inverts the model — **the core is measured and everything is visible**:

1. **Ring-0-only means no user space to hide in.** There is no process boundary
   a cheat can hide behind; the kernel sees everything because everything IS the
   kernel. The cheat's "ring-0 driver" trick is meaningless when the whole OS is
   ring 0.
2. **The measured boot chain is the root of trust.** WuBuFW already measures the
   loader (PCR4) → kernel digest → attestation table. The AGI supervisor refuses
   promotion without a LIVE attestation with a valid digest. Extend this to
   games: the game server verifies the WuBuOS attestation (PCR0-7 + kernel
   digest + container digests) before accepting the session — the
   "attestation-based approach" the AC literature says is the future.
3. **The Colonel is the anti-cheat substrate.** Instead of a third-party kernel
   driver (Vanguard's `vgk.sys` boot-start rootkit-shaped component), the ring-0
   Colonel space hosts an anti-cheat module that is PART of the measured OS:
   `wubu_anticheat.c` registers system-wide callbacks (the same primitives AC
   uses — but as first-class OS citizens, not rootkits), scans memory, and
   reports to the game via the VSL overlay.
4. **Containers give per-game attestation scopes.** Each .wubu overlay container
   carries its own digest; the host PCR + container digest = a session proof.
5. **DMA/hypervisor threats are covered by hardware roots:** IOMMU/VT-d
   configuration in the measured firmware + the attestation chain covers the
   below-OS plane.

The user's framing — "they all operate in upper space at level two and level
three" — is exactly right: the foreign OSes are overlays whose trust is DERIVED
from the measured ring-0 core, not independent kernels to be fought.

## What this means for the build (metal-first)

The hosted binary is the scaffold; EVERYTHING moves to metal:

- **Ring-0 Colonel space (level 0)** — in progress this session: live console
  REPL (done), PCI (done), APIC/LAPIC timer tick (done: 100 Hz, interrupts
  deliver), scheduler (cooperative stable; preempt fix pending), HolyC JIT port
  to metal (next), USB xHCI + HID (next).
- **Wayland user space (level 1)** — the WuBuOS WM/compositor + Bonzi already
  exist hosted; port to the framebuffer on metal (the Win98 shell on the vbe
  fb, compositor-style).
- **VSL overlays (levels 2/3)** — the VSL NT/Linux/macOS translation tables
  exist hosted; they run on top of the metal core as containers once exec +
  the FS land on metal.
- **Anti-cheat (level 0 module)** — `wubu_anticheat` as a measured ring-0
  module + per-container attestation scopes + game-server session proofs.

## Convergence principle (the one sentence)

**TempleOS proved an AGI-sized computer can be all-ring-0; AIOS proved the LLM
belongs in the kernel with its own syscall layer; VSL proved foreign OSes are
just translation tables; the measured boot chain proves the state — so WuBuOS
is a ring-0 Colonel space (the AGI's DOS), with a Wayland user space on top and
every other OS absorbed as attested overlays, making anti-cheat a proof
problem, not an arms race.**

---

# PART 2 — Absorb-Everything, Memory Design, and the Magical Colonel
*(Kevin-Bacon pass 2, 2026-08-02 — same method, deeper hops)*

## The 7 Hops (pass 2)

| Hop | Research | Core finding | Converges on |
|-----|----------|--------------|--------------|
| 1 | Windows NT subsystems + WSL1 + Wine | NT was DESIGNED as many personalities on one kernel (OS/2, POSIX, Win32); WSL1 = Linux as another environment subsystem; "Windows is Wine on NT" | One kernel, many personalities — the VSL is the substrate; add a PERSONALITY REGISTRY |
| 2 | IBM i single-level store (Multics→IBM i), "Operating Systems for Far Out Memories" (2023 diss.) | Memory IS the whole store: no files, only persistent segments; programs+data reappear on restart; a data-centric OS for far-out memories | The Colonel's memory is the storage — single-level store + orthogonal persistence |
| 3 | Memory virtualization (EPT/SLAT, shadow paging, IOMMU/VT-d, IC lecture) | Two-level translation (guest VA→guest PA→real PA); shadow paging too slow nested; EPT exposes hardware tables; IOMMU gives DMA remapping + IOVAs + device isolation + interrupt remapping | The overlay memory model: every foreign OS gets an EPT-style second translation; IOMMU covers DMA/anti-cheat below the OS |
| 4 | Phantom OS orthogonal persistence | OS state survives shutdown/restart; applications don't feel restarts | The OS is a persistent world, not a boot cycle |
| 5 | Genera / Symbolics Lisp Machine | "Awe-inspiring development environment"; user has FREE access to every part of the running OS; everything interacts with everything; changes written live | The WILLY WONKA heritage: the OS as an immersive, live, magical creative space — the Colonel's native feel |
| 6 | MemGPT + A-MEM agent memory | Context window = constrained memory; OS-style hierarchy (core=RAM, archival=disk); agents manage their own memory; A-MEM: atomic notes, memory network, 0.31µs retrieval @ 1M memories | The hive data structure IS the agentic memory substrate — MemGPT hierarchy in C11 |
| 7 | Nix/Guix/ostree + OCI registry | Deterministic, content-addressed, immutable software; atomic installs/upgrades; instant reproducible environments | The Colonel connects, resolves, and INSTANTLY materializes software (wubu_oci already exists) |

## The full design (now understood)

### 1. Absorb-EVERYTHING: one kernel, many personalities

NT's original design (subsystems: OS/2, POSIX, Win32) + WSL1 (Linux as another
subsystem) + Wine (Win32 on the syscall boundary) + Inferno (one binary, Styx
namespace) prove the pattern: **a kernel that hosts multiple OS personalities
at the syscall layer absorbs every OS ever made.**

WuBuOS's VSL already has NT, Linux, and macOS translation tables. The
"absorb everything" design extends this into a **personality registry**:

- Every historical OS personality (CP/M, DOS, Windows 3.1/9x/NT, classic Mac,
  UNIX/BSD, Linux, OS/2, AmigaOS, BeOS…) gets a translation table entry.
- The 8086 DOS shim (wubu_dos_emu — in-process, already built!) is the
  pattern: an interpreter for the oldest programs, VSL translation for the
  modern ones, containers for the rest.
- **Program → personality table → ring-0 VSL → the Colonel space.** Every
  program from every OS that ever existed runs: 8086/16-bit → emu, 32/64-bit
  → VSL translation, foreign kernels → EPT-style overlay.

### 2. Memory design: the single-level store + the hive

- **Single-level store (IBM i lineage):** memory and storage are ONE space.
  No files vs memory distinction — the AGI allocates and the memory persists
  invisibly. The Colonel's brain state (the hive, the trace, the weights)
  IS the store. Restart = the world reappears (Phantom OS orthogonal
  persistence).
- **The hive (the user's hand-drawn Vector/List/Hive design) = the agentic
  memory substrate:** stable pointers, O(1) mark-erase, cache-friendly
  iteration — the C11 realization of A-MEM's atomic notes + memory network.
- **MemGPT hierarchy in the kernel:** core memory (in-context) ↔ archival
  (hive) ↔ storage (single-level store). The AGI manages its own memory via
  syscalls, exactly like MemGPT agents do — but the memory is the OS's memory.
- **Memory virtualization for overlays:** each foreign-OS container gets a
  second-level translation (EPT-style: guest VA → guest PA → real PA) so the
  overlays are fully virtualized; the IOMMU (VT-d/AMD-Vi: DMA remapping,
  IOVAs, device isolation, interrupt remapping) covers the DMA plane — which
  is also the anti-cheat answer to PCIe-DMA cheats.

### 3. The magical Colonel (Willy Wonka OS)

Genera's heritage — free access to every part of the running OS, everything
interacts, changes made live — is the FEEL of WuBuOS:

- **Boot directly into the Colonel.** Not a shell on top of a generic kernel:
  the OS IS the AGI's environment, pre-loaded and compiled (the HolyC JIT +
  the live console already on metal this session).
- **Everything is live:** type code, it compiles and runs in ring 0 (HolyC);
  inspect any memory (single address space); change any part and it applies
  (Genera's model — no user/kernel wall).
- **Wired to the internet:** the Colonel connects (wubu_network + wubu_oci)
  and INSTANTLY downloads/materials whatever it needs — a Nix-style
  content-addressed store (deterministic, immutable, atomic) instead of
  package-manager archaeology. "Pre-loaded with a lot of things" = a seeded
  content store + instant fetch for everything else.
- **Whimsy is a feature:** the Win98/XP desktop, Bonzi Buddy, the φ/GAAD
  feng-shui layout, the musical/colorful UI — the OS is meant to be a joy to
  inhabit (TempleOS's playfulness + Genera's depth + Wonka's wonder).
- **The user's access is intentional and hedged:** Bonzi is the face; the
  browser/games/Steam live at levels 1-3; the Colonel lives at level 0.

### 4. Anti-cheat + the below-OS plane (from Part 1, now with the IOMMU)

Part 1 established attestation-rooted anti-cheat (measured boot + ring-0
visibility). Memory virtualization adds the hardware half: **IOMMU-enforced
DMA remapping in the measured firmware** (device isolation + interrupt
remapping) kills PCIe-DMA cheats; the EPT-style overlay translation means a
foreign OS cannot escape its container's memory. The anti-cheat module in the
Colonel space sees everything at every level.

## What this means for the build (metal-first, continued)

- **Level 0:** live console ✓, PCI ✓, APIC tick ✓, scheduler (cooperative ✓,
  preempt fix pending), HolyC JIT to metal (next), USB xHCI + HID (next),
  then the **personality registry** + **single-level store** + **the
  internet-wired Nix-style store**.
- **Level 1:** the WM/compositor + Bonzi on the metal framebuffer.
- **Levels 2/3:** VSL overlays + EPT-style memory virtualization + containers
  on the metal core.
- **The memory substrate:** the hive becomes the AGI's core memory manager;
  the single-level store becomes the persistent world.

