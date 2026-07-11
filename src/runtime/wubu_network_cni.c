/* wubu_network_cni.c -- WuBuOS network: CNI plugin load/exec backend.
 * Extracted from wubu_network.c (separable leaf). Self-contained: uses the
 * shared resolvers find_network (wubu_network_internal.h) + net_cmd (wubu_netlink.h).
 * C11, minimal includes.
 */
#include "wubu_network.h"
#include "wubu_network_internal.h"
#include "wubu_netlink.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int wubu_cni_plugin_load(WubuNetworkManager *mgr, const char *plugin_path, const char *config_json) {
    if (!mgr || !plugin_path) return -1;
    if (mgr->cni_plugin_count >= WUBU_MAX_PLUGINS) return -1;
    strncpy(mgr->cni_plugin_dirs[mgr->cni_plugin_count], plugin_path, WUBU_MAX_PATH - 1);
    mgr->cni_plugin_count++;
    (void)config_json;
    return 0;
}

int wubu_cni_plugin_exec(WubuNetworkManager *mgr, const char *plugin_name,
                         const char *command, const char *stdin_data,
                         char *stdout_out, size_t stdout_size,
                         char *stderr_out, size_t stderr_size) {
    if (!mgr || !plugin_name || !command) return -1;

    /* Find the plugin binary */
    char plugin_path[WUBU_MAX_PATH];
    snprintf(plugin_path, sizeof(plugin_path), "%s/%s", mgr->cni_bin_dir, plugin_name);

    /* Build environment for CNI */
    char cni_command[64] = {0};
    strncpy(cni_command, command, sizeof(cni_command) - 1);
    char cni_containerid[64] = "test-container";
    char cni_netns[128] = "/var/run/netns/test";
    char cni_ifname[32] = "eth0";
    char cni_args[256] = "K8S_POD_NAMESPACE=test;K8S_POD_NAME=test-pod";
    char cni_path[512];
    snprintf(cni_path, sizeof(cni_path), "%s", mgr->cni_bin_dir);

    /* Use pipe + fork + exec to run the CNI plugin */
    int stdout_pipe[2] = {-1, -1}, stderr_pipe[2] = {-1, -1};
    if (stdout_out && stdout_size > 0) {
        if (pipe(stdout_pipe) < 0) return -1;
    }
    if (stderr_out && stderr_size > 0) {
        if (pipe(stderr_pipe) < 0) {
            if (stdout_pipe[0] >= 0) { close(stdout_pipe[0]); close(stdout_pipe[1]); }
            return -1;
        }
    }

    pid_t pid = fork();
    if (pid < 0) {
        if (stdout_pipe[0] >= 0) { close(stdout_pipe[0]); close(stdout_pipe[1]); }
        if (stderr_pipe[0] >= 0) { close(stderr_pipe[0]); close(stderr_pipe[1]); }
        return -1;
    }

    if (pid == 0) {
        /* Child process */
        setenv("CNI_COMMAND", cni_command, 1);
        setenv("CNI_CONTAINERID", cni_containerid, 1);
        setenv("CNI_NETNS", cni_netns, 1);
        setenv("CNI_IFNAME", cni_ifname, 1);
        setenv("CNI_ARGS", cni_args, 1);
        setenv("CNI_PATH", cni_path, 1);

        if (stdout_pipe[1] >= 0) {
            dup2(stdout_pipe[1], STDOUT_FILENO);
            close(stdout_pipe[0]); close(stdout_pipe[1]);
        }
        if (stderr_pipe[1] >= 0) {
            dup2(stderr_pipe[1], STDERR_FILENO);
            close(stderr_pipe[0]); close(stderr_pipe[1]);
        }

        /* Write stdin data to fd 0 if provided */
        if (stdin_data && *stdin_data) {
            int stdin_pipe[2];
            pipe(stdin_pipe);
            if (fork() == 0) {
                close(stdin_pipe[0]);
                write(stdin_pipe[1], stdin_data, strlen(stdin_data));
                close(stdin_pipe[1]);
                _exit(0);
            }
            close(stdin_pipe[1]);
            dup2(stdin_pipe[0], STDIN_FILENO);
            close(stdin_pipe[0]);
        }

        execl(plugin_path, plugin_name, NULL);
        _exit(127);
    }

    /* Parent: read output */
    if (stdout_pipe[1] >= 0) close(stdout_pipe[1]);
    if (stderr_pipe[1] >= 0) close(stderr_pipe[1]);

    if (stdout_out && stdout_size > 0 && stdout_pipe[0] >= 0) {
        ssize_t n = read(stdout_pipe[0], stdout_out, stdout_size - 1);
        if (n > 0) stdout_out[n] = '\0';
        else stdout_out[0] = '\0';
        close(stdout_pipe[0]);
    }
    if (stderr_out && stderr_size > 0 && stderr_pipe[0] >= 0) {
        ssize_t n = read(stderr_pipe[0], stderr_out, stderr_size - 1);
        if (n > 0) stderr_out[n] = '\0';
        else stderr_out[0] = '\0';
        close(stderr_pipe[0]);
    }

    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) return 0;
    return -1;
}
