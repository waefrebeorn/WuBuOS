# Sources (checked 2026-11-19)

GitHub-hosted runners (free/unlimited public repos; macOS labels):
- https://docs.github.com/en/actions/reference/runners/github-hosted-runners
- https://docs.github.com/en/actions/writing-workflows/choosing-where-your-workflow-runs/choosing-the-runner-for-a-job

Self-hosted runner requirements (macOS 11.0+):
- https://docs.github.com/en/actions/reference/runners/self-hosted-runners

Cirrus CI shutdown (jobs stopped 2026-06-01; runners closed 2026-04-07):
- https://dev.to/kalemi/cirrus-ci-shutting-down-moving-the-macos-jobs-first-2k0i
- https://dev.to/kalemi/cirrus-ci-alternative-for-macos-where-those-jobs-go-2c34
- https://cirrus-ci.org/pricing/ (historical free-OSS terms)

macOS CI pricing cross-comparison (Oct 2026 rates):
- https://macswarm.com/compare/macos-ci-runners-pricing/
- https://nomac.app/blog/best-macos-runners-github-actions

MacStadium FOSS program (free Mac mini, criteria, waitlist):
- https://macstadium.com/company/opensource
- https://macstadium.com/blog/macstadium-supports-the-open-source-community

osxcross (cross-toolchain; SDK packaging; targets):
- https://github.com/tpoechtrager/osxcross
- https://github.com/pts/pts-osxcross (prebuilt, SDK 10.10, x86 only)
- https://github.com/wailsapp/wails/discussions/4267 (GUI cross-compile
  limits, Nov 2025: zig + osxcross both fail on modern Cocoa frameworks)

OpenCore Legacy Patcher (iMac13,x support; Sequoia; AVX2/RAM caveats):
- https://dortania.github.io/OpenCore-Legacy-Patcher/MODELS.html
- https://oclp.dev/sequoia/
- https://dortania.github.io/OpenCore-Legacy-Patcher/FAQ.html

Wayland on Darwin (epoll-shim, MacPorts recipe, wayland-os.c analysis):
- https://github.com/MaxLeiter/jailbreak/blob/main/x11/docs/wayland-plan.md
- https://github.com/jiixyj/epoll-shim
- https://ports.macports.org/port/epoll-shim/details/
- https://www.freshports.org/graphics/wayland (epoll-shim dependency)
- https://github.com/CoreBedtime/iland (macOS wayland support lib; SIP hack)

Internal (this tree):
- wubuos docs/compendium/04-roadmap/hardcoding-census.md (philosophy,
  portability fixes, compositor split map)
- wubucompositor README.md (portability map)
- skill wubuos-macos-leg-proof (metal2vulkan compute leg; darling dead end)
- skill wubucompositor (crash-verified protocol rules)