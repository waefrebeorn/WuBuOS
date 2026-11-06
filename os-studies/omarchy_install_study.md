# Omarchy Installer Source Study — the Arch Linux install/boot/package framework

**Study target:** the "omarchy installer" that WuBuOS should steal to produce a bootable, verifiable, package-managed OS install. Reference OS = Arch Linux. Scope = archinstall (installer-as-library), the Unified Kernel Image chain (UKI + kernel-install + systemd-stub + ukify), mkinitcpio (initramfs presets/hooks), pacman (transaction plan + keyring), systemd-boot (UEFI boot manager).
**Maps to WuBu goals:** `wubu_initramfs.c`, the UKI builder, `wubu_keyring.c`, `wubu_pkg_txn.c`, `wubu_aur.c`, `wubu_unit_parse.c`.

---

## What it is / architecture

### archinstall — the installer is a library, not a script
- archinstall "is a library which can be used to install Arch Linux. The library comes packaged with different pre-configured installers, such as the default Guided installation" [11]. The GitHub mission statement confirms it: "Archinstall primarily functions as a flexible library for managing services, packages, and other elements within an Arch Linux system," with the guided installer as the shippable front end [12].
- The guided installer (default `script: "guided"`) is fully driven by a single JSON configuration file, passed locally via `--config` or remotely via `--config-url`; a `--dry-run` simulates the run and writes the config to disk, so the whole install is a machine-readable, replayable document [11].
- The bootloader is a first-class JSON option: `bootloader_config` selects `Systemd-boot`, `grub`, or `limine`; `uki: true/false` enables unified kernel images (UEFI only, systemd-boot/limine only); `removable: true/false` installs to the default removable-media path `/EFI/BOOT/` instead of writing an NVRAM entry (UEFI only, grub/limine only) [11].
- Other JSON knobs cover the mirror set (`mirror_config`, with per-repo `sign_check`/`sign_option`), disk layout + LUKS encryption (`disk_config`, `disk_encryption`), kernel selection (`kernels` array), package list, and `parallel downloads` [11].
- Installer is an `archinstall.lib.installer.Installer` class instance; plugins are discovered by hooking entry points, e.g. a plugin may define `on_pacstrap(*packages)` which the hardcoded `Pacman().strap()` call iterates [11].

### The Unified Kernel Image chain — one UEFI PE ships stub + kernel + initramfs + cmdline
- A UKI "is the combination of a UEFI boot stub program like systemd-stub(7), a Linux kernel image, an initramfs, and further resources in a single UEFI PE file," bootable directly by UEFI firmware or auto-sourced by bootloaders with little/no config [13].
- mkinitcpio can assemble the UKI itself; otherwise it offloads to `systemd-ukify`, feeding ukify a config from `{/etc,/usr/lib}/kernel/uki.conf` [13]. Kernel parameters come from `.conf` files under `/etc/cmdline.d` which mkinitcpio concatenates, OR from `/etc/kernel/cmdline` [13].
- Presets drive output: `/etc/mkinitcpio.d/linux.preset` defines `PRESETS=('default' 'fallback')`, each with `default_image=` / `default_uki=` and per-preset options like `-S autodetect` (skipped on the fallback so it carries a full module set) [13].
- systemd-boot auto-sources UKIs: "Unified kernel images (UKIs) in `esp/EFI/Linux/` are automatically sourced by systemd-boot, and do not need an entry in `esp/loader/entries`" [26]. A firmware boot entry for the `.efi` can be created with `efibootmgr --create ... --loader '\EFI\Linux\arch-linux.efi' --unicode` [13].

### kernel-install — systemd's kernel lifecycle + plugin drivers
- kernel-install "is a flexible utility designed to streamline the installation and administration of Linux kernel images" with a plugin system for boot-loader entries, UKI generation, and Secure Boot signing; notably "kernel-install itself doesn't generate the initramfs, but utilizes plugins from programs like mkinitcpio or dracut instead" [22].
- Main config is `/etc/kernel/install.conf`, where `layout=bls` (traditional split kernel+initramfs) or `layout=uki` (unified) is chosen; default generators are `initrd_generator=mkinitcpio` and `uki_generator=ukify`, both overridable [22].
- Kernel command line is set in `/etc/kernel/cmdline`; it is either embedded in the UKI or added to bootloader config per layout. If absent, kernel-install falls back to `/usr/lib/kernel/cmdline` or `/proc/cmdline` [22].
- Plugins live in `/usr/lib/kernel/install.d/*.install` (e.g. `50-depmod`, `50-mkinitcpio`, `60-ukify`, `90-loaderentry`, `90-uki-copy`, `91-sbctl`); same-named files in `/etc/kernel/install.d/` override them, and symlinking one to `/dev/null` disables it [22]. Manual ops: `kernel-install add <ver> /usr/lib/modules/<ver>/vmlinuz` and `remove <ver>`; `add-all` handles every installed kernel [22].

### mkinitcpio — ordered, declarative initramfs assembly
- mkinitcpio is a Bash script that builds initramfs images; "Every time a kernel is installed or upgraded, a pacman hook automatically generates a _.preset_ file for the kernel in `/etc/mkinitcpio.d/`" — the preset is "a list of information required to create initial ramdisk images" [23].
- Config lives in `/etc/mkinitcpio.conf` (+ drop-ins `/etc/mkinitcpio.conf.d/*.conf`), with variables `MODULES`, `BINARIES`, `FILES`, `HOOKS`, `COMPRESSION`, `COMPRESSION_OPTIONS`, `MODULES_DECOMPRESS` [23]. `BINARIES` are dependency-parsed (libraries auto-added); `FILES` are added as-is; `MODULES` are loaded before any boot hook [23].
- Hooks "are small scripts describing what will be added to the initramfs image," executed in the order listed in the `HOOKS` array; a `systemd` hook in the array selects a systemd-based initramfs, its absence selects a busybox `ash` init with sequential runtime hooks [23].
- Build hooks (install hooks) source two functions, `build` and `help`, living in `/usr/lib/initcpio/install/` (custom: `/etc/initcpio/install/`); runtime hooks expose ordered phases `run_earlyhook`, `run_hook`, `run_latehook`, and `run_cleanuphook` (the last runs in reverse array order) in `/usr/lib/initcpio/hooks/` [23]. Post hooks run after image generation with kernel/image/UKI paths + `KERNELVERSION`/`KERNELDESTINATION` passed as args [23].
- The default setup builds two images: `default` (per config) and `fallback` (same but `autodetect` skipped, thus a full module range) [23]; `mkinitcpio -p linux` regenerates one preset, `mkinitcpio -P` regenerates all [23].

### pacman — the transactional, signed package manager
- pacman "is written in the C programming language and uses the bsdtar(1) tar format for packaging," synchronizing package lists with the master server and pulling all required dependencies [24].
- A successful transaction follows "five high-level steps plus pre/post transaction hooks": 1) initialize the transaction if there is no database lock; 2) choose which packages are added/removed; 3) prepare via sanity checks on sync databases/packages/dependencies; 4) commit — download (`_alpm_sync_load`), run `PreTransaction` hooks, remove replaced/conflicting packages, per-package `pre_install` → untar/dump files → `post_install`; 5) run `PostTransaction` hooks and release the transaction + database lock [24].
- Capsule transaction model: packages are archives containing compiled files + metadata (name, version, deps) + install directives; pacman handles dependencies, tracks every file for clean removal, and can run pre/post transaction hooks from `/usr/share/libalpm/hooks/` plus the `HookDir` (default `/etc/pacman.d/hooks`), with hook filenames suffixed `.hook` [24].
- Signature model (see next section) is controlled per `SigLevel` in `/etc/pacman.conf`; "The default configuration, `SigLevel = Required DatabaseOptional`, enables signature verification for all the packages on a global level," overridable per-repository [24].

### pacman keyring — OpenPGP web of trust
- "To determine if packages are authentic, _pacman_ uses OpenPGP keys in a web of trust model": at least three Master Signing Keys co-sign the Developers'/Package Maintainers' keys, which sign packages; each user gets a unique key via `pacman-key` [25].
- Setup is two idempotent operations: `pacman-key --init` initializes the keyring; `pacman-key --populate` loads the official keys [25]. The `archlinux-keyring` package — "a dependency of base, contains the latest keys," and unknown keys are fetched from a keyserver configured in `/etc/pacman.d/gnupg/gpg.conf` [25].
- `SigLevel` trust floor: `TrustedOnly` is the compiled-in default (identical to `SigLevel = Required DatabaseOptional TrustedOnly`); `TrustAll` exists only for debugging and "makes it very easy to trust keys that have not been verified" [25].

### systemd-boot — minimal UEFI boot manager
- systemd-boot is "an easy-to-configure UEFI boot manager" providing a textual menu and an editor for the kernel command line; it can only start EFI executables from the ESP or an XBOOTLDR partition [26]. `bootctl install` "will copy the _systemd-boot_ UEFI boot manager to the ESP, create a UEFI boot entry for it and set it as the first in the UEFI boot order" [26].
- It ships with systemd (a `base` dependency), reads FAT12/16/32 via the firmware, and auto-detects `/EFI/Microsoft/.../Bootmgfw.efi`, `/EFI/BOOT/bootx64.efi`, and "specially prepared kernel files found in /EFI/Linux/" with no manual loader config [26]. XBOOTLDR partitions (GUID `bc13c2ff-59e6-4262-a352-b275fd6f7172`) keep kernels/initramfs off the ESP [26].

---

## Stealable techniques (fact + /n mapping + delta-by-design)

1. **Installer-as-library + JSON-as-truth.** archinstall is a library with a preconfigured guided front end and a full config passed via `--config` [11][12]. => **/n mapping:** `/n/install/` surface where every answered step is a file under the Styx namespace, the "plan" being a single JSON doc. **Delta:** the JSON config doubles as the audit log (like `/var/log/archinstall`) but is live-edited via 9P writes instead of a TUI.
2. **Bootloader/UKI/removable as declarative options.** `bootloader_config { bootloader, uki, removable }` with `removable` = install to `/EFI/BOOT/` for portability [11]. => **/n surface:** `/n/install/boot.hive` — three booleans + bootloader IDs emitted by the wizard. **Delta:** `removable=on` is our default ("safe default" per archinstall for the removable path [11]) because WuBuOS boots from arbitrary media.
3. **UKI = single signed PE (stub+kernel+initramfs+cmdline).** [13] is the whole boot story — one file, one signature, Secure-Boot friendly. => **/n surface:** `/n/boot/uki/<variant>.efi`, generated by our `wubu_uki.c`. **Delta:** we add a `cmdline.d/`-style splat in `/n/boot/cmdline.d/*.conf` merged at build time, mirroring mkinitcpio's `/etc/cmdline.d` [13].
4. **kernel-install plugin chain (numbered `install.d` drop-ins).** Initramfs generator, UKI generator, loader entry, UKI copy, sbctl signing are all separate ordered plugins, disabled by symlinking to `/dev/null` [22]. => **/n surface:** `/n/boot/kernel_install.d/<nn>-*.install` as 9P files; enable/disable = create/unlink the file. **Delta:** plugin = a C11 registration struct, not a shell script, so signing and depmod are in-tree built-ins with no assembly interpreter in initramfs.
5. **Presets drive initramfs variants.** `default` vs `fallback` (full module set, no autodetect) generated by pacman hooks on every kernel change [23]. => **/n surface:** `/n/boot/mkinitcpio/<kernel>.preset` and `/n/boot/initramfs/{default,fallback}`. **Delta:** `fallback` becomes our recovery/angel path; `-S autodetect` maps to a module-dep closure walk over the module tree (`wubu_initramfs.c`).
6. **Ordered build/runtime/post hook phases.** `build`+`help` install hooks; `run_earlyhook/run_hook/run_latehook/run_cleanuphook` (cleanup runs in reverse) [23]. => **/n surface:** `/n/boot/hooks/{install,runtime,post}/<name>` phase dirs. **Delta:** we keep the four runtime phases as kernel-visible event points, replacing bash sourcing with C11 function tables — the ordering contract (array order, reverse cleanup) is the real steal.
7. **Five-step transaction with lock + prepare + per-file commit.** pacman's plan: DB-lock, choose, prepare/sanity-check, commit (download → PreTransaction → remove → each package: pre_install → untar → post_install), PostTransaction → release [24]. => **/n surface:** `/n/pkg/txn/<id>.plan` — lock, add/remove set, commit, hooks staged as files. **Delta:** our `wubu_pkg_txn.c` keeps the file manifest so removal is exact and `.pacnew`-style config preservation is native.
8. **OpenPGP web of trust + idempotent keyring init/populate.** Master keys co-sign packager keys; `pacman-key --init`/`--populate`; keyring shipped as a package (`archlinux-keyring`) [25]. => **/n surface:** `/n/keyring/*.gpg` + `/n/keyring/trust.db`, populated by `wubu_keyring.c`. **Delta:** rebuild the keyring as a self-hosted C11 object store on `/n` (no GnuPG), keeping the two-step init/populate contract and the 1-package keyring upgrade path.
9. **systemd-boot auto-sourcing of `/EFI/Linux/*.efi`.** UKIs in `esp/EFI/Linux/` need no loader entry [26]. => **/n surface:** `/n/boot/EFI/Linux/` — dropping a UKI file *is* the boot entry. **Delta:** our boot manager scans `/n/boot/EFI/Linux` directly for `.efi` and also honors the `efibootmgr --create` NVRAM path for non-removable installs [13].

> [unverified] The exact pacman `_alpm_sync_load` internal symbol and the `layout=bls` spec reference were sourced only via the ArchWiki pages cited above; the underlying alpm API semantics were not independently confirmed against libalpm source.

---

## /n control-plane mapping

| Concern | Arch source | /n surface | WuBu module |
|---|---|---|---|
| Installer config (replayable JSON) | archinstall `--config` [11] | `/n/install/config.json` | wizard; `wubu_install.c` |
| Bootloader choice / uki / removable | archinstall `bootloader_config` [11] | `/n/install/boot.hive` | `wubu_bootloader.c` |
| UKI build (PE: stub+kernel+initramfs+cmdline) | [13] | `/n/boot/uki/<variant>.efi` | `wubu_uki.c` |
| Kernel cmdline merge | `/etc/cmdline.d`, `/etc/kernel/cmdline` [13][22] | `/n/boot/cmdline.d/*.conf` | `wubu_unit_parse.c` |
| Kernel lifecycle (install/remove/plugins) | kernel-install `install.d` [22] | `/n/boot/kernel_install.d/<nn>-*.install` | `wubu_kernel_install.c` |
| Initramfs presets (default/fallback) | mkinitcpio presets [23] | `/n/boot/mkinitcpio/<kernel>.preset` | `wubu_initramfs.c` |
| Initramfs hooks (build/runtime/post) | mkinitcpio HOOKS array + phases [23] | `/n/boot/hooks/{install,runtime,post}/<name>` | `wubu_initramfs.c` |
| Package transaction plan | pacman 5-step [24] | `/n/pkg/txn/<id>.plan` | `wubu_pkg_txn.c` |
| Hooks (Pre/PostTransaction) | pacman hooks [24] | `/n/pkg/hooks/{PreTransaction,PostTransaction}/*.hook` | `wubu_pkg_txn.c` |
| Signature policy | pacman.conf SigLevel [24][25] | `/n/keyring/siglevel.hive` | `wubu_keyring.c` |
| Keyring init/populate | `pacman-key --init/--populate` [25] | `/n/keyring/{trust.db,*.gpg}` | `wubu_keyring.c` |
| User/community packages | AUR (community repo, not part of this study) [unverified in this doc] | `/n/pkg/aur/*` | `wubu_aur.c` |
| Boot manager (menu + auto-scan EFI/Linux) | systemd-boot [26] | `/n/boot/EFI/Linux/` | `wubu_boot_sd.c` |
| NVRAM boot entry | `efibootmgr --create` [13] | `/n/boot/nvram/entry` | `wubu_boot_nvram.c` |

---

## P-level build spec (go heavy first)

- **P1** — `/n/pkg/txn/*.plan` transaction core in C11: lock, choose set, prepare/sanity-check, commit, release; exact file manifest + `.pacnew` preservation [24]. (wubu_pkg_txn.c)
- **P2** — OpenPGP-equivalent keyring on `/n`: init + populate contract, signed package/metadata verification, shipped keyring object store [25]. (wubu_keyring.c)
- **P3** — UKI builder emitting one signed PE = systemd-stub + kernel + initramfs + merged cmdline; `cmdline.d/*.conf` splice; Secure-Boot-ready [13][22]. (wubu_uki.c)
- **P4** — Initramfs presets + ordered build/runtime/post hooks with array-order + reverse-cleanup semantics; default/fallback variants; module-dep closure for `autodetect` [23]. (wubu_initramfs.c)
- **P5** — Installer-as-library: JSON config drive, `boot_hive` (bootloader/uki/removable), boot manager auto-sourcing `/EFI/Linux/*.efi`, and the NVRAM/efibootmgr fallback [11][26][13]. (wubu_install.c, wubu_boot_sd.c)
- **P6** — kernel-install-style numbered plugin drop-ins (`install.d` override/disable-by-unlink) binding generator/loader-entry/signing as in-tree C11 plugins [22]. (wubu_kernel_install.c)

## PART 2 — the Omarchy Configurator UX: graphics + onboarding (the actual installer front end)

This is the piece the earlier PART 1 (install framework) did not cover: the **visual
and onboarding personality** of the installer — what the user means by "cool graphics
and helpful onboarding." Grounded in the cloned `omacom/omarchy-iso` source
(`configs/airootfs/root/configurator`, a 1,446-line **single bash script**), verified
2026-11-06 from `/home/wubu/Documents/omarchy-study/omarchy-iso`.

### What the configurator actually does (source facts)

1. **It is one bash script, gum-gated.** The whole wizard is `configurator`, built on
   charmbracelet gum for every widget. `gum draws on /dev/tty, so measure that rather
   than inherited COLUMNS` [27:59]. `gum style --foreground 2 --padding ...` draws the
   logo in green; `gum spin --spinner pulse --title ... -- sleep N` shows a pulsing
   progress spinner between steps [27:106,200]. A `notice()` clears the logo, spins the
   pulse spinner, and sleeps ~2s as the "installation show" beat [27:199-202].
2. **An animated ColorShift boot logo.** `greeter()` draws the ASCII logo (from
   `$OMARCHY_PATH/logo.txt`) vertically centered with a dark "Press Return to Start
   Install" hint and the tagline — NO animation on this frame [27:108-142]. The *next*
   step animates it: `ttfx -i "$LOGO_PATH" --frame-rate 60 --xterm-colors` runs the
   terminal text animation, a **ColorShift** from a green base (indexed color 2)
   drifting through a cyan accent (6) and settling back on green [27:163-173]. The 16
   INDEXED colors map to the Tokyo Night palette because the framebuffer console
   "crushes the palette to a muddy lavender" if truecolor is used — `--xterm-colors`
   keeps the stops indexed so the ttfx settle color stays in step with the gum logo
   underneath [27:144-152].
3. **A console-width "smush" guard.** The console grows after boot (the framebuffer
   settles late, "especially in VMs"); the logo is wider than 80 columns, so the first
   draw against the transient narrow console would wrap ("smushed") it. The configurator
   waits for the width to settle at/beyond the logo width before the first draw and
   re-centers on growth [27:57-85,103].
4. **A small, human question set.** Five-ish steps: LANGUAGE (every language in
   `locales.json`, with per-language defaults = its keyboard layout + its country's
   capital for timezone fallback; languages the console font can't draw are listed in
   English) [27: STEP 0 LANGUAGE block], KEYBOARD [27:292,306], the account form
   (username + password; password hashed with `openssl passwd -6 -stdin` = SHA-512
   crypt `$6$`) [27:350-354], DISK [27:396], summary. The summary screen echoes every
   choice with a MASKED password (`printf "%${#password}s" | tr ' ' '*'`) and i18n
   labels via `$(t summary.username)` [27:381-382].
5. **Deferred provisioning = onboarding at first boot, not install.** `Deferred-
   provisioning installs defer the user to first boot: no credentials` [27:580]. The
   install writes no user; personal setup (keyboard layout, username, password) runs
   the first time the machine boots, on the SAME centered logo+tagline+hint frame that
   opened the install — "install and first boot bookend the setup identically"
   [27:110-116]. The user runs `omarchy-provision-user` [29 README].
6. **Disk model.** Full-disk (wipes the selected drive, **full encryption by default**)
   vs free-space (dual-boot in unallocated space); `Ctrl+C` at the format confirmation
   switches to an encryption-less install [28]. Partitioning is factored into a sourced
   `disk-partitioning.sh` helper [27:29-36]. The ISO writes a **pre-built zstd-compressed
   btrfs partition image** onto the target with `zstdcat | dd`, gives it a fresh
   filesystem id, grows it to the partition, installs the per-machine packages from a
   bundled mirror, runs target setup in the chroot, creates the user, and runs
   `omarchy-provision-user` [29 README].
7. **Unattended / zero-wizard installs.** Attach a drive labeled `cidata` with
   `user-data`/`meta-data` and the installer copies the config off it and **skips the
   configurator entirely**; with no such drive the wizard runs as usual [29 README].
   Answers are `jq`-escaped into `configurator-answers.json` so the orchestrator can
   consume them [27:594-596].

> The whole appeal ("Be up and running in as little as seven seconds", "answer five
> questions... it hands back a finished desktop") [31] is therefore: a tiny question
> set + deferred personalization + an animated, great-looking console frame — the
> install is boring-fast and the FLAIR is front-loaded into the boot/install graphics.

### Stealable techniques → /n mapping (delta-by-design)

| Omarchy installer fact | /n surface (WuBuOS) | WuBu module / delta |
|---|---|---|
| ColorShift animated framebuffer logo, INDEXED colors (framebuffer consoles crush truecolor) [27:144-173] | `/n/install/greeter.logo` + colour state | `wubu_console` / `wubu_boot_greeter.c`; delta = a C11 frame renderer, ttfx ColourShift re-expressed with integer ARGB + a settle envelope |
| Width-settle guard (logo wider than transient console) [27:57-103] | `/n/install/greeter.size` (measured dims) | `wubu_console` re-render on resize |
| Greeter = logo + tagline + one dim hint button, no animation; bookends first boot [27:108-142] | `/n/install/welcome.hive` (tagline/hint strings) | `wubu_install.c`; same frame rendered at install AND first boot |
| Gum widgets (`style`, `spin pulse`) + i18n `t()` summary with masked password [27:199,381] | `/n/install/answers.json` (masked view served here) | `wubu_console` input prompts; password = `openssl passwd -6`-equivalent SHA-512 crypt |
| 5-step question set: language→keyboard→account→disk→summary [27] | `/n/install/questions.n` step files | `wubu_install.c` harness mirrors the step order |
| Deferred provisioning — user created at FIRST boot on the same frame [27:580,110] | `/n/install/firstboot` marker + `/n/firstboot/onboarding` | `wubu_firstboot.c`; write no user at install, run onboarding on boot if marker present |
| Full-disk vs free-space + encryption default + Ctrl+C skip [28] | `/n/install/disk.hive` (mode, target, encrypt) | `wubu_disk_install.c` over `fat32_format(blk, sectors, "WUBUOS")` + AHCI/NVMe probe |
| Prebuilt root image `zstdcat | dd` + grow-to-partition [29] | `/n/install/image.hive` + `/n/install/target` | `wubu_image_write.c` streaming a compact os-image to the target block device |
| cidata unattended config skips the wizard [29] | `/n/install/unattended` (9P read of a labeled volume) | `wubu_install.c` config-file path (JSON->hive) |
| First-boot onboarding: keyboard/username/password on first power-on [30] | `/n/firstboot/` subtree | `wubu_firstboot.c` reuses the greeter frame + account form |

### P-level build spec for a WuBuOS installer (go heavy first)

WuBuOS today boots kernel.elf and has the *primitives* (`fat32_format`, `wubu_storage_probe`,
AHCI/NVMe, `vbe_draw_text`/`dosgui_*`, `wubu_console`) but **no configurator** — this is the
parity gap. Build:

- **P1 — Configurator core (C11, in-process, first):** a boot-time wizard in `wubu_console`
  that renders the greeter frame (ASCII wordmark + tagline + one dim hint), then asks
  language→keyboard→username→(masked) password→hostname→disk target, shows an i18n summary
  with a masked password, and on confirm **formats + writes** the OS via a new
  `wubu_install.c` that currently uses `fat32_format(blk, n_sectors, "WUBUOS")` and streams
  kernel.elf + boot to the chosen AHCI/NVMe device. Regression gate: a harness that answers
  the steps and asserts the disk image (boot sector + kernel) is written, byte-shape-verified.
- **P2 — Graphics + onboarding flair:** the ColorShift boot-logo animation re-expressed in C
  (indexed palette, settle envelope, width-settle guard), wired into `wubu_boot_greeter.c`;
  the `gum spin pulse`-equivalents as `wubu_console` progress ticks; the deferred-
  provisioning first-boot frame (same greeter) + `/n/firstboot/onboarding` that creates the
  user/hostname/keyboard on first power-on.
- **P3 — Full-disk vs free-space + encryption + unattended:** disk-hive (mode/target/encrypt)
  with Ctrl+C-style no-encryption escape, and a `cidata`-style unattended path that reads
  `/n/install/unattended` and skips the wizard.

> Parity target note: the "cool graphics" are front-loaded console flair (animated logo,
> pulse spinner, dim buttons) over a *tiny* honest question set — not a heavyweight UI. The
> heaviest lift is the same-frame-deferred onboarding, because it makes install fast and
> makes first boot feel intentional. That is the ordering to steal.

[11] https://man.archlinux.org/man/archinstall.1.en — archinstall(1) man page — library + preconfigured guided installer, bootloader/uki/removable JSON options
[12] https://github.com/archlinux/archinstall — archinstall GitHub repo — mission statement, guided installer default, script library
[13] https://wiki.archlinux.org/title/Unified_kernel_image — ArchWiki Unified kernel image — UKI = systemd-stub + kernel + initramfs + cmdline in one UEFI PE
[22] https://wiki.archlinux.org/title/Kernel-install — ArchWiki kernel-install — plugin system, /etc/kernel/install.conf layout=bls|uki
[23] https://wiki.archlinux.org/title/Mkinitcpio — ArchWiki Mkinitcpio — presets, MODULES/HOOKS/BINARIES/FILES, hook ordering + runtime functions
[24] https://wiki.archlinux.org/title/Pacman — ArchWiki Pacman — 5-step transaction plan, hooks, SigLevel, C + bsdtar
[25] https://wiki.archlinux.org/title/Pacman/Package_signing — ArchWiki pacman/Package signing — OpenPGP web of trust, pacman-key --init/--populate, archlinux-keyring
[26] https://wiki.archlinux.org/title/Systemd-boot — ArchWiki systemd-boot — UEFI boot manager, bootctl install, auto-sources esp/EFI/Linux UKIs
[27] `configs/airootfs/root/configurator` in omacom/omarchy-iso (cloned at `/home/wubu/Documents/omarchy-study/omarchy-iso`, depth-1) — the 1,446-line Installer Configurator: gum-gated UI (:59,106,199-203), ttfx ColorShift animated logo + indexed Tokyo Night palette + `--frame-rate 60 --xterm-colors` (:144-173), console-width smush guard (:57-103), greeter frame w/ tagline+dim hint (:108-142), i18n `locales.json` + message catalog (:237-296), account form + `openssl passwd -6` SHA-512 crypt (:350-354), masked-password summary (:381-382), disk steps (:396), sourced `disk-partitioning.sh` (:29-36), deferred provisioning (:577-583), `jq`-escaped answers json (:594-596)
[28] https://omarchy.org/manual/getting-started/ — full-disk vs free-space install, full encryption default, Ctrl+C at format confirm → no-encryption
[29] https://github.com/omacom/omarchy-iso — README: writes pre-built zstd-compressed btrfs root image via `zstdcat | dd`, fresh fsid, grow-to-partition, per-machine packages, chroot setup, create user, `omarchy-provision-user`; cidata unattended skip
[30] https://omarchy.org/manual/getting-started/ — "all the personal setup — keyboard layout, username, password — is deferred until the machine boots for the first time"
[31] https://omarchy.org/ — "Be up and running in as little as seven seconds", "answer five questions", "hands back a finished desktop", install show
[32] https://distro-finder.com/d/omarchy — boots into a text-mode system installer (Archinstall lineage)
[33] https://en.wikipedia.org/wiki/Omarchy — DHH's opinionated distro for onboarding Windows/Mac users; Arch + Hyprland + Quickshell; a configuration post-install → its own install image + repo
