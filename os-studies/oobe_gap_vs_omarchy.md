# WuBuOS OOBE vs Omarchy — gap codewalk

> 2026-11-15. Comparison of WuBuOS's out-of-box-experience surface against
> Omarchy's installer + first-boot provisioning, read against the actual source
> (`src/runtime/wubu_onboarding.c`, `src/gui/shell/wubu_shell_greeter.c`,
> `src/hosted/hosted.c`, `install-wubuos.sh`, the boot chain in
> `docs/BOOT_CHAIN.md`). The Omarchy side is grounded by
> `os-studies/omarchy_install_study.md`, which is done.
>
> Headline: **Omarchy's OOBE is a boot-time OS installer + first-boot
> provisioning, with all the flair front-loaded as console graphics. WuBuOS has
> the low-level primitives (firmware, kernel, VBE, `wubu_console`, FAT32,
> AHCI/NVMe) and a tested first-boot GREETER decision layer — but no OS installer,
> no wizard, no disk imaging, no flair.** Almost everything the study's P1–P6
> spec names is still unbuilt.
>
> **Update 2026-11-15 — configurator core shipped:** the five-step wizard body
> now exists (`src/runtime/wubu_install.{c,h}`): language → keyboard → account
> (username + password, salted+iterated SHA-256, never the plaintext) → disk →
> masked summary, decoupled from any UI by injected I/O, with a serializable
> `/n/install` answers document that round-trips. Green in its own gate
> (`make test_wubu_install`, 25 checks). The stdio front end
> (`wubu_install_tty`, `make test_wubu_install_tty`) turns it into a real
> boot-console / DevOps text configurator that writes the confirmed plan to
> disk. The GUI greeter still renders a theme picker, and the disk write / user
> creation layers are still next.

---

## What WuBuOS actually has today (codewalk evidence)

| Piece | Code | What it does |
|---|---|---|
| Hosted desktop installer | `install-wubuos.sh` | Interactive POSIX-shell installer that asks prefix / autostart / window / MCP, writes a **replayable plan** to `~/.wubu/n/install/plan.json` (`--plan-only`), installs, then **verifies by actually running** what it installed. Installs the *hosted desktop app* on a running Linux — **not** an OS to a block device. |
| First-boot decision | `src/runtime/wubu_onboarding.{c,h}` | Sentinel-driven GREETER vs DESKTOP choice; persists a config signature → next boot is DESKTOP. Injection-testable path. 13/13 in its gate. |
| First-boot greeter | `src/gui/shell/wubu_shell_greeter.{c,h}` (308 lines) | A real modal dialog on the * WM stack: **theme picker** (+3 first-run offers: Getting Started / GUI Designer / Terminal), a Start button, footer hint. On Start → `wubu_onboarding_complete("theme=…")`. Has a test (`wubu_shell_greeter_test.c`) and a capture tool (`wubu_greeter_shot.c`). |
| Wiring | `src/hosted/hosted.c` (≈206–224) | On hosted boot, absent sentinel → `wubu_shell_greeter_run()` before connecting to Wayland. |
| Boot chain | `docs/BOOT_CHAIN.md` | WuBuFW (self-written UEFI, TPM/secureboot) → chainloader → **naked `KERNEL.ELF`** → kernel. Self-contained; no UKI, no initramfs, no boot-manager menu. |
| Package layer | `src/runtime/wubu_pkg.c`, `wubu_aur.c`, `wubu_txn/wubu_txn.c` | A package manager and an AUR front end exist; `wubu_txn` is the GrahaOS *transactional-speculation* subsystem (checkpoint/rollback), **not** pacman's five-step package transaction. |

---

## The gap table — Omarchy feature → WuBuOS status

### PART 1 — install / boot framework (archinstall + UKI + pacman)

| # | Omarchy feature (study §PART 1) | WuBuOS | Evidence |
|---|---|---|---|
| 1 | Installer-as-library, JSON config drive, replayable | **PARTIAL** | `install-wubuos.sh` writes a replayable `plan.json` on the `/n/install` surface, but installs a hosted app, not an OS. `wubu_install.c` does not exist. |
| 2 | Declarative bootloader / uki / removable (`bootloader_config`) | **MISSING** | No `wubu_bootloader.c`. Boot is one firmware chainloader. |
| 3 | UKI = one signed PE (stub+kernel+initramfs+cmdline) | **MISSING** | Boots naked `KERNEL.ELF`. `wubu_uki.c` absent; firmware does measurement/secureboot but no UKI assembly. |
| 4 | kernel-install numbered plugin chain | **MISSING** | `wubu_kernel_install.c` absent. |
| 5 | Initramfs presets (default/fallback) + ordered hooks | **MISSING** | No initramfs concept; kernel self-contains `cpio_extract.c`/`zip_extract.c`. `wubu_initramfs.c` absent. |
| 6 | Ordered build/runtime/post hook phases | **MISSING** | Absent. |
| 7 | pacman five-step transaction (file manifest, `.pacnew`) | **PARTIAL** | `wubu_pkg.c` + `wubu_aur.c` exist; `wubu_txn` is transactional *speculation*, not package transactions. No exact-removal manifest / `.pacnew`. |
| 8 | OpenPGP keyring `--init`/`--populate`, `SigLevel` | **MISSING** | `wubu_crypto.h`, `wubu_sha256.c` exist; no keyring/trust module. |
| 9 | Boot-manager auto-source `/EFI/Linux/*.efi` + NVRAM entry | **MISSING** | `wubu_boot_sd.c`/`wubu_boot_nvram.c` absent. |

### PART 2 — configurator UX + onboarding (the "flair")

| # | Omarchy feature (study §PART 2) | WuBuOS | Evidence |
|---|---|---|---|
| 10 | Animated ColorShift boot logo (indexed colors, settle envelope) | **MISSING** | Greeter is a static themed dialog; no logo splash, no animation. |
| 11 | Console width-settle guard (avoid smush) | **MISSING** | `wubu_console.c` exists but has no install-greeter size guard. |
| 12 | Greeter frame bookends install AND first boot | **PARTIAL** | First-boot greeter is real and wired; the install does **not** close on the same frame. |
| 13 | 5-step question set: language→keyboard→account→disk→summary (masked pw, i18n) | **PARTIAL — body shipped** | The wizard ENGINE (`wubu_install.c`) collects lang→keyboard→account→disk→timezone→masked summary with a salted-iterated password hash, driven by injected I/O, and round-trips a `/n/install` answers document — green in `test_wubu_install`. Still missing: per-language keyboard/timezone defaults and any i18n, and the GUI greeter still renders a theme picker instead of this set. |
| 14 | Deferred provisioning — create the user at FIRST boot on the same frame (`omarchy-provision-user`) | **PARTIAL** | The sentinel orchestration works and the greeter runs at first boot — **but it creates a theme only**. The configurator now holds the provisioning answers (username + hashed password); no `wubu_firstboot.c` yet applies them to create the user/hostname/keyboard. |
| 15 | Full-disk vs free-space, full-encryption default, Ctrl+C → no-encryption | **MISSING** | `wubu_disk_install.c` absent. |
| 16 | Prebuilt root image `zstdcat|dd` + grow-to-partition | **MISSING** | `fat32_format.c` primitive exists; `wubu_image_write.c` absent; no wizard drives it. |
| 17 | `cidata`-labeled volume → skip the wizard (unattended) | **MISSING** | No unattended pre-config path. The sentinel is post-config, not an injection. |
| 18 | jq-escaped `answers.json` capture for the orchestrator | **PARTIAL** | `plan.json` is the replay/audit doc; the greeter persists only `theme=<name>`. |
| 19 | gum pulse-spinner "installation show" / animated progress | **MISSING** | `install-wubuos.sh` is silent text; no progress flair. |

---

## What WuBuOS has that Omarchy's configurator does not

- **Verifies by running.** `install-wubuos.sh` boots/launches what it installed
  as its acceptance test — the study's own gold standard ("an install that does
  not boot what it put on disk is not an install"). Omarchy's configurator
  relies on chroot setup + a display check.
- **A real, tested GUI first-boot dialog already wired in.** The greeter is a
  true WM modal with key/mouse input, handler tests, and a capture tool — not a
  bash/gum TUI. The only missing question is that it configures theme, not the
  machine.

---

## Root cause and ordering to steal

The heaviest lifts are **not** any of the primitives — those exist — they are:

1. **A boot-time OS configurator that formats + writes an OS to a target disk**
   (the study's P1 `wubu_install.c`, driven by the existing primitives
   `fat32_format`, AHCI/NVMe probe, `wubu_console`).
2. **The five-step question set** (language/keyboard/account/disk/summary with
   masked password + i18n). Current greeter = theme picker; this is the single
   biggest user-facing gap.
3. **Deferred provisioning that actually creates the user** on first boot on
   the same frame (`wubu_firstboot.c`). The orchestration already works; the
   body is missing.
4. **The disk model** (full-disk / free-space / encryption / root-image write +
   grow); **cidata unattended**; then **the flair** (ColorShift logo, width
   guard, pulse spinner, install+first-boot bookend).

Per the study's own note, the "cool graphics" are *front-loaded console flair
over a tiny honest question set* — so the ordering to steal is: real body
(configurator + provisioning) first, flair second. The flair is what makes it
feel like Omarchy; the question set + disk write + user creation are what make
it an OOBE at all.
