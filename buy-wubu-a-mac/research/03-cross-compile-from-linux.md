# 03 — Compile checks from Linux, no Mac anywhere

osxcross (github.com/tpoechtrager/osxcross): macOS cross-toolchain for
Linux/BSD. Clang is a cross-compiler by default; osxcross adds the
cctools/ld64 port + the macOS SDK. Targets x86_64 and arm64 (arm64 needs
the macOS 11+ SDK).

## What it buys

A local, free, instant compile-check loop for the Apple branches:
`o64-clang -std=c11 -target x86_64-apple-darwin20 --sysroot=<SDK> ...`
catches syntax/type/API mistakes in the `_NSGetExecutablePath` branches
and the future Quartz C without any network or runner. It does NOT
produce a runnable verification: no macOS kernel, no frameworks at
runtime. Compile-green here is necessary, not sufficient; the CI loop
(01) is the sufficiency.

## Limits, verified honestly

- CLI/C code cross-compiles well. Modern macOS GUI applications are
  where cross-compilation breaks: framework/objc linking fights (a
  Nov-2025 wails discussion documents zig AND osxcross failing on
  Cocoa/WebKit frameworks). Our Quartz backend should therefore stay
  in the C APIs (CoreGraphics/ApplicationServices C surface) as far as
  possible; ObjC pieces get compiled on the real macOS loop.
- The SDK comes from Xcode (gen_sdk_package.sh against an Xcode dmg;
  Xcode downloads need a free Apple ID, not the paid program).
- LICENSING: Apple's SDK/Xcode license terms restrict use to
  Apple-branded hardware. osxcross on a Linux box is a gray area.
  Position: use it for local compile CHECKS only, keep the authoritative
  build-and-verify on GitHub's macOS runners (Apple-licensed hardware)
  and the owned iMac. Nothing shipped is built by the gray path.
- pts-osxcross is a prebuilt binary variant (SDK 10.10, x86 only) —
  too old for our targets; use upstream osxcross with a current SDK.

## Loop shape

    edit Apple branch
      -> o64-clang compile check (local, instant, free)
      -> git push
      -> GitHub macOS runner: real compile + run gates (authoritative)
      -> green -> package loop

The local check exists to keep the CI loop's failure rate low, not to
replace it.