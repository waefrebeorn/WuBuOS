/*
 * wubu_exec.c  --  WuBuOS Universal Executable Dispatcher Implementation
 *
 * One exec to rule them all.
 */

#include "wubu_exec.h"
#include "wubu_exec_internal.h"
#include "wubu_host_exec.h"
#include "../compiler/holyc.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

/* -- Format Names ------------------------------------------------- */

/* -- Format Detection --------------------------------------------- */

/* -- VSL (Virtualization Substrate Layer) -------------------------
 *
 * VSL is now HOST DELEGATION - fork+exec on the host Linux kernel.
 * This replaces the old in-process syscall translation layer.
 * Architecture: "VSL is NOT a Linux syscall emulation layer  -- 
 * rename to wubu_host_linux.c (platform delegation to host libc)."
 */

static bool g_vsl_initialized = false;
static void *g_vsl_shared = NULL;

int wubu_vsl_init(void) {
    if (g_vsl_initialized) return 0;

    /* 1. Verify host environment has required capabilities */
    /* Check for basic fork/exec support */
    pid_t test_pid = fork();
    if (test_pid < 0) {
        return -1;  /* Host doesn't support fork */
    }
    if (test_pid == 0) {
        _exit(0);  /* Child exits immediately */
    }
    int status;
    waitpid(test_pid, &status, 0);

    /* 2. Verify we can exec basic binaries */
    if (access("/bin/sh", X_OK) != 0 && access("/usr/bin/sh", X_OK) != 0) {
        return -1;  /* No shell available */
    }

    /* 3. Set up shared memory region for WuBuOS ↔ host communication */
    {
        int shm_fd = memfd_create("wubu_vsl_shm", 0);
        if (shm_fd >= 0) {
            if (ftruncate(shm_fd, 4096) == 0) {
                g_vsl_shared = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                                    MAP_SHARED, shm_fd, 0);
                if (g_vsl_shared != MAP_FAILED) {
                    memset(g_vsl_shared, 0, 4096);
                }
            }
            close(shm_fd);
        }
        /* Non-fatal if memfd unavailable (e.g., restricted kernel) */
    }

    g_vsl_initialized = true;
    return 0;
}

void wubu_vsl_shutdown(void) {
    g_vsl_initialized = false;
}

bool wubu_vsl_active(void) {
    return g_vsl_initialized;
}

int64_t wubu_vsl_run(const char *cmd) {
    if (!g_vsl_initialized) {
        if (wubu_vsl_init() != 0) return -1;
    }

    if (!cmd || !*cmd) return -1;

    /* Fork and execute the command on the host */
    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }

    if (pid == 0) {
        /* Child: execute via shell */
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        /* If /bin/sh fails, try /usr/bin/sh */
        execl("/usr/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }

    /* Parent: wait for completion */
    int status = 0;
    pid_t ret = waitpid(pid, &status, 0);
    if (ret != pid) {
        return -1;
    }

    if (WIFEXITED(status)) {
        return (int64_t)WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        return (int64_t)(-WTERMSIG(status));
    }

    return -1;
}

/* -- Native Execution --------------------------------------------- */
/*
 * WuBuOS native exec: mmap payload with PROT_EXEC and call entry_offset.
 * This is the "bare metal" path — the payload is native machine code
 * (e.g., JIT-compiled or AOT-compiled WuBuOS bytecode → x86-64).
 *
 * Architecture validation: only execute if host arch matches container arch.
 * entry_offset is relative to the start of the payload.
 */
int64_t wubu_exec_native(const void *payload, size_t payload_size,
                         uint64_t entry_offset, uint8_t arch) {
    if (!payload || payload_size == 0) return WUBU_EXEC_ERR_MEM;

    /* Validate architecture: host must match container */
#if defined(__x86_64__)
    uint8_t host_arch = WUBU_ARCH_X86_64;
#elif defined(__aarch64__)
    uint8_t host_arch = WUBU_ARCH_AARCH64;
#elif defined(__riscv) && (__riscv_xlen == 64)
    uint8_t host_arch = WUBU_ARCH_RISCV64;
#else
    uint8_t host_arch = 0xFF;  /* unknown arch */
#endif

    if (arch != host_arch) {
        fprintf(stderr, "[wubu_exec] arch mismatch: container=%d host=%d\n",
                arch, host_arch);
        return WUBU_EXEC_ERR_FMT;
    }

    /* Validate entry_offset is within payload */
    if (entry_offset >= payload_size) {
        fprintf(stderr, "[wubu_exec] entry_offset %lu >= payload_size %zu\n",
                (unsigned long)entry_offset, payload_size);
        return WUBU_EXEC_ERR_HDR;
    }

    /* mmap with PROT_READ | PROT_EXEC (no WRITE — code is immutable) */
    void *code = mmap(NULL, payload_size,
                      PROT_READ | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (code == MAP_FAILED) {
        perror("[wubu_exec] mmap native code");
        return WUBU_EXEC_ERR_MEM;
    }

    /* Copy payload into executable memory */
    memcpy(code, payload, payload_size);

    /* Calculate entry point */
    void *entry = (uint8_t *)code + entry_offset;

    /* Call the entry point as a function: int main(void) -> int return */
    /* Native WuBuOS executables use the standard x86-64 SysV ABI:
     *   main() returns int via RAX. */
    int (*native_main)(void);
    memcpy(&native_main, &entry, sizeof(native_main));

    fprintf(stderr, "[wubu_exec] native: calling %p (offset %lu in %zu bytes)\n",
            entry, (unsigned long)entry_offset, payload_size);

    int ret = native_main();

    /* Cleanup */
    munmap(code, payload_size);
    return (int64_t)ret;
}

/* -- Format-Specific Executors ------------------------------------ */

int64_t wubu_exec_linux_elf(const void *elf_data, size_t elf_size) {
    if (!elf_data || elf_size < 4) return -1;

    /* Validate ELF magic */
    const uint8_t *p = (const uint8_t *)elf_data;
    if (p[0] != 0x7F || p[1] != 'E' || p[2] != 'L' || p[3] != 'F')
        return -1;

    /* Write ELF to temp file for container execution */
    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/wubu-elf-%d", getpid());
    FILE *f = fopen(tmp_path, "wb");
    if (!f) return -1;
    fwrite(elf_data, 1, elf_size, f);
    fclose(f);
    chmod(tmp_path, 0755);

    /* Create native Linux container and exec the ELF */
    char *root = getenv("WUBU_ARCH_ROOT");
    if (!root) root = "/";  /* fallback to host root */

    WubuCt *ct = wubu_ct_native("linux-elf", root);
    if (!ct) {
        unlink(tmp_path);
        return -1;
    }

    /* Set up command to run the ELF */
    char *argv[] = {tmp_path, NULL};
    wubu_ct_set_cmd(ct, 1, argv);
    ct->net_enabled = true;

    int rc = wubu_ct_start(ct);
    if (rc != 0) {
        wubu_ct_destroy(ct);
        unlink(tmp_path);
        return -1;
    }

    /* Wait for completion */
    int exit_code = wubu_ct_wait(ct);
    wubu_ct_destroy(ct);
    unlink(tmp_path);

    return exit_code;
}

int64_t wubu_exec_win_pe(const void *pe_data, size_t pe_size) {
    if (!pe_data || pe_size < 2) return -1;

    /* Validate PE magic (MZ) */
    const uint8_t *p = (const uint8_t *)pe_data;
    if (p[0] != 'M' || p[1] != 'Z')
        return -1;

    /* Write PE to temp file for container execution */
    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/wubu-pe-%d.exe", getpid());
    FILE *f = fopen(tmp_path, "wb");
    if (!f) return -1;
    fwrite(pe_data, 1, pe_size, f);
    fclose(f);

    /* Create Proton/SteamOS container and exec via Wine */
    char *root = getenv("WUBU_ARCH_ROOT");
    if (!root) root = "/";

    WubuCt *ct = wubu_ct_steamos("win-pe", root);
    if (!ct) {
        unlink(tmp_path);
        return -1;
    }

    /* Set up command: wine /path/to/exe */
    char *argv[] = {"/usr/bin/wine", tmp_path, NULL};
    wubu_ct_set_cmd(ct, 2, argv);
    ct->net_enabled = true;
    ct->gpu_passthrough = true;  /* Proton needs GPU */

    int rc = wubu_ct_start(ct);
    if (rc != 0) {
        wubu_ct_destroy(ct);
        unlink(tmp_path);
        return -1;
    }

    /* Wait for completion */
    int exit_code = wubu_ct_wait(ct);
    wubu_ct_destroy(ct);
    unlink(tmp_path);

    return exit_code;
}

int64_t wubu_exec_holyc(const char *source, size_t source_size) {
    if (!source) return -1;
    /* Use our HolyC compiler to JIT compile and execute */
    return hc_eval(source);
}

int64_t wubu_exec_c(const char *source, size_t source_size) {
    if (!source || source_size == 0) return -1;

    /* Write C source to temp file */
    char src_path[256];
    char bin_path[256];
    snprintf(src_path, sizeof(src_path), "/tmp/wubu-c-src-%d.c", getpid());
    snprintf(bin_path, sizeof(bin_path), "/tmp/wubu-c-bin-%d", getpid());

    FILE *f = fopen(src_path, "w");
    if (!f) return -1;
    fwrite(source, 1, source_size, f);
    fclose(f);

    /* Compile via gcc -O2 -o bin src.c
     /* Compile the C source to a binary */
         /* Use -no-pie for simpler entry-point mapping. */
         char cmd[1024];
         snprintf(cmd, sizeof(cmd),
                  "gcc -O2 -no-pie -o %s %s 2>/dev/null",
                  bin_path, src_path);

         /* Fork and exec gcc instead of system() */
         pid_t gcc_pid = fork();
         if (gcc_pid < 0) {
             unlink(src_path);
             return WUBU_EXEC_ERR_JIT;
         }
         if (gcc_pid == 0) {
             execl("/bin/sh", "sh", "-c", cmd, (char*)NULL);
             _exit(127);
         }
         int compile_rc;
         waitpid(gcc_pid, &compile_rc, 0);
         unlink(src_path);  /* clean up source regardless */

         if (!WIFEXITED(compile_rc) || WEXITSTATUS(compile_rc) != 0) {
             fprintf(stderr, "[wubu_exec] C compilation failed (gcc exit=%d)\n",
                     WEXITSTATUS(compile_rc));
             unlink(bin_path);
             return WUBU_EXEC_ERR_JIT;
         }

    /* Execute the compiled binary */
    chmod(bin_path, 0755);
    int64_t result = wubu_vsl_run(bin_path);
    unlink(bin_path);
    return result;
}

int64_t wubu_exec_shell(const char *script, size_t script_size) {
    (void)script_size;
    /* Write script to temp file and execute via bash */
    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/wubu-shell-%d.sh", getpid());
    FILE *f = fopen(tmp_path, "wb");
    if (!f) return -1;
    fwrite(script, 1, script_size, f);
    fclose(f);
    chmod(tmp_path, 0755);

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "bash %s", tmp_path);
    int64_t result = wubu_vsl_run(cmd);
    unlink(tmp_path);
    return result;
}

int64_t wubu_exec_python(const char *script, size_t script_size) {
    (void)script_size;
    /* Write script to temp file and execute via python3 */
    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/wubu-python-%d.py", getpid());
    FILE *f = fopen(tmp_path, "wb");
    if (!f) return -1;
    fwrite(script, 1, script_size, f);
    fclose(f);

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "python3 %s", tmp_path);
    int64_t result = wubu_vsl_run(cmd);
    unlink(tmp_path);
    return result;
}

/* -- WASM Execution ----------------------------------------------- */

/* -- Mach-O Execution --------------------------------------------- */

/* -- Container Exec ----------------------------------------------- */

/* -- Universal Exec ----------------------------------------------- */

int64_t wubu_exec(const void *data, size_t size, const char *filename) {
    if (!data || size == 0) return -1;

    bool is_wubu = false;
    WUBU_PAYLOAD_TYPE type = wubu_detect_format(data, size, &is_wubu);

    if (filename) {
        printf("[wubu_exec] %s: %s\n", filename, wubu_payload_name(type));
    }

    if (is_wubu) {
        WUBU_HEADER hdr;
        const void *payload;
        size_t payload_size;

        if (wubu_container_parse(data, size, &hdr, &payload, &payload_size) != 0)
            return -1;

        return wubu_exec_container(&hdr, payload, payload_size);
    }

    /* Raw file  --  wrap in implicit .wubu and dispatch */
    switch (type) {
        case WUBU_PAYLOAD_LINUX_ELF:
            return wubu_exec_linux_elf(data, size);
        case WUBU_PAYLOAD_WIN_PE:
            return wubu_exec_win_pe(data, size);
        case WUBU_PAYLOAD_HOLYC_SRC:
            return wubu_exec_holyc((const char *)data, size);
        case WUBU_PAYLOAD_C_SRC:
            return wubu_exec_c((const char *)data, size);
        case WUBU_PAYLOAD_SHELL_SCRIPT:
            return wubu_exec_shell((const char *)data, size);
        case WUBU_PAYLOAD_PYTHON:
            return wubu_exec_python((const char *)data, size);
        case WUBU_PAYLOAD_WASM:
            return wubu_exec_wasm(data, size);
        case WUBU_PAYLOAD_MAC_MACHO:
            return wubu_exec_macho(data, size);
        default:
            return -1;
    }
}

