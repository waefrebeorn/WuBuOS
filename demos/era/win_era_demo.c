/*
 * win_era_demo.c -- Windows NT / Win32 era demo (1993)
 *
 * A real 64-bit Windows console program cross-compiled with
 * x86_64-w64-mingw32-gcc. It writes a marker file (proving the Win32
 * file API + the underlying NT syscalls are reachable) and prints a banner.
 *
 * Runs INSIDE WuBuOS through the Windows personality:
 *   wubu_exec_win_pe() -> Proton/SteamOS container -> Wine64.
 *
 * This is the SAME launch path a graphics-intensive Win64 app (e.g. DaVinci
 * Resolve) would take; only the workload differs. Build:
 *   x86_64-w64-mingw32-gcc -O2 win_era_demo.c -o win_era_demo.exe
 */
#include <stdio.h>
#include <windows.h>

int main(void) {
    printf("WuBuOS Win32 era: hello from a 64-bit PE through Wine/Proton\n");

    /* Real Win32 file I/O -> NT syscall (NtCreateFile/NtWriteFile).
     * Write the marker to an absolute, host-discoverable path. Wine changes
     * the process CWD to the .exe's directory, so a relative name would land
     * next to the temp .exe (not where the launcher's parent can see it).
     * Prefer $WUBU_REPO_ROOT if set (points at the repo the launcher was run
     * from), else fall back to the current directory. */
    const char *root = getenv("WUBU_REPO_ROOT");
    char path[MAX_PATH];
    if (root && root[0]) {
        _snprintf(path, MAX_PATH, "%s/WUBU_ERA_WIN.OK", root);
        path[MAX_PATH - 1] = '\0';
    } else {
        _snprintf(path, MAX_PATH, "WUBU_ERA_WIN.OK");
        path[MAX_PATH - 1] = '\0';
    }

    HANDLE h = CreateFileA(path,
                            GENERIC_WRITE, 0, NULL,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        const char *msg = "Win32 (NT) personality exercised by WuBuOS\n";
        DWORD written = 0;
        WriteFile(h, msg, (DWORD)lstrlenA(msg), &written, NULL);
        CloseHandle(h);
    } else {
        fprintf(stderr, "CreateFile failed: %lu\n", GetLastError());
    }
    return 0;
}
