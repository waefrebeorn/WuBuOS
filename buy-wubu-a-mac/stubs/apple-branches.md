# Live Apple branches (copies, 2026-11-19)

The originals stay wired in their repos; these are the collected
copies for the dossier. Both are compile-pending on real macOS —
the free CI loop (research/01) is how they get verified.

## wubucompositor/src/wcc_spawn.c — guest path derivation

```c
#if defined(__APPLE__)
#include <mach-o/dyld.h>   /* _NSGetExecutablePath — macOS has no /proc */
#endif

const char *wcc_default_start_cmd(void) {
    static char path[PATH_MAX];
    static int resolved = 0;
    if (!resolved) {
        resolved = 1;
        int have_exe = 0;
#if defined(__APPLE__)
        /* macOS: no /proc. _NSGetExecutablePath may return a relative
         * path; realpath it before walking up. */
        uint32_t sz = (uint32_t)sizeof(path);
        if (_NSGetExecutablePath(path, &sz) == 0) {
            char real[PATH_MAX];
            if (realpath(path, real)) {
                snprintf(path, sizeof(path), "%s", real);
            }
            have_exe = 1;
        }
#else
        ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
        if (n > 0) { path[n] = '\0'; have_exe = 1; }
#endif
        if (have_exe) {
            char *slash = strrchr(path, '/');
            if (slash) {
                *slash = '\0';  /* now the executable's directory */
                size_t len = strlen(path);
                const char *rel = "/../wubuos/src/hosted/wubu";
                if (len + strlen(rel) < sizeof(path)) {
                    strcat(path, rel);
                    return path;
                }
            }
        }
        snprintf(path, sizeof(path), "%s", WUBU_GUEST_FALLBACK);
    }
    return path;
}
```

Gate: `make test-spawn` (headless) — ALL PASS on Linux. The Apple
branch compiles for the first time on the macOS runner.

## wubuos/src/hosted/hosted.c — buddy launcher derivation

```c
#if defined(__APPLE__)
#include <mach-o/dyld.h>   /* _NSGetExecutablePath (no /proc on macOS) */
#endif

        char self[4096];
#if defined(__APPLE__)
        /* macOS has no /proc — the WSL-replacement philosophy needs the
         * same derivation on both hosts (see wcc_spawn.c). */
        uint32_t sz = (uint32_t)sizeof(self);
        if (_NSGetExecutablePath(self, &sz) != 0) return;
        char real[4096];
        if (realpath(self, real)) snprintf(self, sizeof(self), "%s", real);
#else
        ssize_t n = readlink("/proc/self/exe", self, sizeof(self) - 1);
        if (n <= 0) return;
        self[n] = 0;
#endif
        /* then: walk up from the executable to the repo root, look for
         * build/wubu-buddy (unchanged, host-agnostic). */
```

Gate: hosted build green on Linux. Same macOS verification path.

## Also portable already (no branch needed)

- Compositor keymap transport: POSIX shm_open + immediate unlink
  (replaced Linux-only memfd_create). Works on both hosts as-is.
- The guest's Wayland client core: portable headers only.