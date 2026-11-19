# Research: verify macOS code for free, without owning a Mac

Checked 2026-11-19. Sources in SOURCES.md. The constraint set: no new
hardware, no debt, no human-in-the-loop services; agent-drivable or
nothing; free first.

## Verdict matrix

| option | cost | real macOS? | agent-drivable? | verdict |
|---|---|---|---|---|
| GitHub Actions macOS (public repo) | $0, unlimited | yes: Intel + M1 | yes: push / workflow_dispatch / gh CLI | PRIMARY |
| 2012 iMac + OCLP, SSH loop | $0 (owned) | yes (x86_64, legacy Metal) | yes: ssh + cron/agent | FALLBACK |
| 2012 iMac self-hosted runner | $0 (owned) | yes, needs macOS 11+ (OCLP) | yes: GitHub assigns jobs | FALLBACK (after OCLP) |
| osxcross on Linux | $0 | compile only, no run | yes: local toolchain | COMPILE CHECKS |
| Codemagic free tier | $0, 500 M2 min/mo | yes | yes: API/CLI | SPARE (small quota) |
| Blacksmith / Avrea free minutes | $0, ~150 min/mo | yes (M4) | yes: GitHub App | SPARE (small quota) |
| GetMac free tier | $0, 100 min/mo, no card | yes (M4 VM) | partial | SPARE |
| MacStadium FOSS program | $0 Mac mini | yes (bare metal) | yes AFTER approval; application + waitlist = human gate | BACKUP, apply once |
| NoMac | $0.80/hr, from $10 prepaid | yes (M4-M6) | yes: MCP/CLI/API/SSH, built for agents | PAID-BUT-AUTOMATED last resort |
| Cirrus CI | was free | — | — | DEAD: jobs stopped 2026-06-01; runners closed to new customers 2026-04-07 |
| AWS EC2 Mac / MacStadium paid / MacRun | $109+/mo or 24h-min dedicated hosts | yes | yes | REFUSED: debt-adjacent, unnecessary |
| darling (Mach-O process exec on Linux) | $0 | no (translation) | — | DEAD END for full process exec (skill-verified) |
| macOS VM on the Linux box (QEMU) | $0 | technically possible | — | REFUSED: Apple EULA allows macOS VMs only on Apple hardware; the iMac is the legal host |

## The design

Primary loop: research/01. The iMac: research/02. Compile checks without
any Mac: research/03. Small free quotas and the MacStadium application:
research/04. Why the compositor can actually build on macOS (epoll-shim):
research/05.

The GitHub package loop finalizes: once the macOS verify workflow is
green on both architectures, the release workflow builds the artifacts
and `gh release create` publishes them. Everything between edit and
artifact is CLI; no step waits on a person.