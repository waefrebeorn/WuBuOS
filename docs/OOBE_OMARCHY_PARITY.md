# OOBE Parity: WuBuOS first-boot wizard vs omarchy deferred provisioning

**Date:** 2026-11-15. Comparison of the WuBuOS first-boot OOBE
(`src/gui/shell/wubu_shell_greeter.c`, 3-step wizard) against omarchy's first-boot
OOBE — what omarchy calls *deferred provisioning* (from `os-studies/omarchy_install_study.md`
Part 2, and the omarchy ISO `configurator`: five questions at install, personal setup moved
to FIRST BOOT via `omarchy-provision-user`).

## The honest comparison

| omarchy first-boot OOBE | WuBuOS | status |
|---|---|---|
| Keyboard / username / password deferred to first boot, NOT install | Same sentinel pattern (`wubu_onboarding.c`); but only theme/network are captured | PARTIAL |
| **Password hashed with SHA-512 crypt (`$6$`, `openssl passwd -6`)** | **NEW `wubu_pwcrypt.c` — implemented, verified byte-exact vs openssl + canonical vectors** | DONE |
| Language selection (i18n, per-language keyboard default) | none | MISSING |
| Keyboard layout selection | none | MISSING |
| Username + password, masked | none | MISSING |
| Hostname | none | MISSING |
| Account record persisted at first boot | sentinel persists only a config signature | MISSING |

What WuBuOS already had that omarchy lacks: a real GUI wizard (omarchy is console/gum),
theme choice, live network-interface discovery, first-run offers (Getting Started / GUI
Designer / Terminal), and progress pagination.

## What the codewalk added (this pass)

1. **`src/runtime/wubu_pwcrypt.{c,h}`** — a self-contained C11 SHA-512 crypt (`$6$`)
   primitively. No GNU crypt(3), no glibc, no heap. **Verified byte-exact:**
   - 5 live vectors against `openssl passwd -6` on the dev box
   - the canonical `$6$saltstring` / "Hello world!" vector from the reference source
   - hermetic gate `test_pwcrypt` (embedded vectors, runs without external tools)
2. The algorithm is a **transcription** of the reference (phadej/crypt-sha512, the glibc
   construction) — not a paraphrase — because three parts of the spec are easy to get
   wrong from prose alone and all three failed against the oracle before being corrected:
   - digest A folds a key-length bit-select (B-vs-key) after B
   - P/S are derived from digests (DP, DS), repeated+truncated, not from raw repeats
   - there is a 21-group b64 permutation and **no** separate final digest

## Remaining work (next session)

- Greeter: add a **user-account step** (username, masked password, confirm) that calls
  `wubu_pw_crypt_sha512` for a shadow-compatible record.
- Greeter: **keyboard layout step** and **hostname step** (bring the wizard to 5+ steps,
  omarchy's `language→keyboard→account→disk→summary` spirit).
- Persist username/hostname/keyboard/password-hash in the onboarding signature so the
  recorded OOBE is a real account record, not just a theme choice.

The cryptography — the hard, security-sensitive slice — is done and gated. The GUI steps
are well-scoped wiring on top of a proven primitive.
