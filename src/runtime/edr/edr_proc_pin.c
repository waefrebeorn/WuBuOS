/* edr_proc_pin.c  --  WuBuOS EDR Process Pin
 *
 * NETLINK_CONNECTOR + cn_proc — the Linux equivalent of Windows
 * PsSetCreateProcessNotifyRoutineEx.
 *
 * Subscribes to PROC_CN_MCAST_LISTEN to receive real-time
 * PROC_EVENT_FORK, PROC_EVENT_EXEC, and PROC_EVENT_EXIT messages.
 * Pushes EdrEvent structs into the lock-free queue.
 *
 * This is the single most important EDR telemetry source on Linux —
 * without it, the process model never learns about new processes.
 */

#include "edr_internal.h"
#include <linux/connector.h>
#include <linux/cn_proc.h>

/* ================================================================
 * Netlink helpers for the connector socket
 * ================================================================ */

static int cn_send_proc_event(int fd, int event_type) {
    struct {
        struct nlmsghdr nl_hdr;
        struct cn_msg cn_msg;
        enum proc_cn_mcast_op mc_op;
    } msg;

    memset(&msg, 0, sizeof(msg));

    msg.nl_hdr.nlmsg_len = sizeof(msg);
    msg.nl_hdr.nlmsg_type = NLMSG_DONE;
    msg.nl_hdr.nlmsg_flags = 0;
    msg.nl_hdr.nlmsg_seq = 0;
    msg.nl_hdr.nlmsg_pid = getpid();

    msg.cn_msg.id.idx = CN_IDX_PROC;
    msg.cn_msg.id.val = CN_VAL_PROC;
    msg.cn_msg.len = sizeof(enum proc_cn_mcast_op);

    msg.mc_op = event_type;

    struct sockaddr_nl sa = {
        .nl_family = AF_NETLINK,
        .nl_groups = 0
    };

    struct iovec iov = { &msg, sizeof(msg) };
    struct msghdr mh = { &sa, sizeof(sa), &iov, 1, NULL, 0, 0 };

    if (sendmsg(fd, &mh, 0) < 0)
        return -1;
    return 0;
}

/* ================================================================
 * Public API
 * ================================================================ */

int edr_proc_pin_start(void) {
    /* Open NETLINK_CONNECTOR socket */
    g_proc_connector_fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_CONNECTOR);
    if (g_proc_connector_fd < 0) {
        perror("[edr_proc] NETLINK_CONNECTOR socket");
        return -1;
    }

    struct sockaddr_nl sa = {
        .nl_family = AF_NETLINK,
        .nl_groups = CN_IDX_PROC,
        .nl_pid    = getpid()
    };

    if (bind(g_proc_connector_fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        perror("[edr_proc] bind NETLINK_CONNECTOR");
        close(g_proc_connector_fd);
        g_proc_connector_fd = -1;
        return -1;
    }

    /* Subscribe to process events */
    if (cn_send_proc_event(g_proc_connector_fd, PROC_CN_MCAST_LISTEN) < 0) {
        perror("[edr_proc] PROC_CN_MCAST_LISTEN");
        close(g_proc_connector_fd);
        g_proc_connector_fd = -1;
        return -1;
    }

    printf("[edr_proc] NETLINK_CONNECTOR process pin active\n");
    return 0;
}

void edr_proc_pin_poll(void) {
    if (g_proc_connector_fd < 0) return;

    struct sockaddr_nl sa;
    socklen_t sa_len = sizeof(sa);

    /* Non-blocking peek — how much data is available? */
    char peek_buf[8];
    ssize_t peek = recvfrom(g_proc_connector_fd, peek_buf, sizeof(peek_buf),
                              MSG_PEEK | MSG_DONTWAIT, (struct sockaddr *)&sa, &sa_len);
    if (peek <= 0) return;

    /* Allocate full receive buffer (cn_proc messages are small, but be generous) */
    char buf[4096];
    ssize_t n = recvfrom(g_proc_connector_fd, buf, sizeof(buf),
                           MSG_DONTWAIT, (struct sockaddr *)&sa, &sa_len);
    if (n <= 0) return;

    struct nlmsghdr *nl_hdr = (struct nlmsghdr *)buf;
    for (; NLMSG_OK(nl_hdr, (size_t)n); nl_hdr = NLMSG_NEXT(nl_hdr, n)) {
        if (nl_hdr->nlmsg_type == NLMSG_ERROR) continue;
        if (nl_hdr->nlmsg_type == NLMSG_DONE) continue;

        struct cn_msg *cn_msg = (struct cn_msg *)NLMSG_DATA(nl_hdr);
        if (cn_msg->id.idx != CN_IDX_PROC) continue;

        struct proc_event *ev = (struct proc_event *)cn_msg->data;
        EdrEvent *edr_ev = NULL;

        switch (ev->what) {
        case PROC_EVENT_FORK: {
            /* Fork: new child = edr_process; parent = what's forking */
            uint32_t child_pid  = ev->event_data.fork.child_pid;
            uint32_t child_tgid = ev->event_data.fork.child_tgid;
            uint32_t parent_pid = ev->event_data.fork.parent_pid;
            uint32_t parent_tgid = ev->event_data.fork.parent_tgid;

            /* Push PROCESS_CREATE for the child */
            edr_ev = calloc(1, sizeof(EdrEvent) + sizeof(uint64_t)*2);
            if (!edr_ev) break;
            edr_ev->header.version = 1;
            edr_ev->header.type = EDR_EV_PROCESS_CREATE;
            edr_ev->header.pid = child_tgid;
            edr_ev->header.tid = child_pid;
            edr_ev->header.extra_pid = parent_tgid; /* creator_pid */
            edr_ev->header.timestamp = (uint64_t)time(NULL) * 1000000000ULL;
            edr_ev->header.size = sizeof(EdrEvent) + sizeof(uint64_t)*2;
            edr_ev->header.u64a = child_pid;  /* thread id */
            edr_ev->header.u64b = parent_pid;
            edr_queue_push(edr_ev);

            /* Also push THREAD_CREATE for the child thread */
            EdrEvent *thr_ev = calloc(1, sizeof(EdrEvent));
            if (thr_ev) {
                thr_ev->header.version = 1;
                thr_ev->header.type = EDR_EV_THREAD_CREATE;
                thr_ev->header.pid = child_tgid;
                thr_ev->header.tid = child_pid;
                thr_ev->header.extra_pid = parent_tgid;
                thr_ev->header.timestamp = (uint64_t)time(NULL) * 1000000000ULL;
                thr_ev->header.size = sizeof(EdrEvent);
                edr_queue_push(thr_ev);
            }
            break;
        }
        case PROC_EVENT_EXEC: {
            /* Exec: process replaced itself */
            uint32_t pid  = ev->event_data.exec.process_pid;
            uint32_t tgid = ev->event_data.exec.process_tgid;
            edr_ev = calloc(1, sizeof(EdrEvent));
            if (!edr_ev) break;
            edr_ev->header.version = 1;
            edr_ev->header.type = EDR_EV_PROCESS_CREATE; /* treat exec as new process */
            edr_ev->header.pid = tgid;
            edr_ev->header.tid = pid;
            edr_ev->header.extra_pid = 0;
            edr_ev->header.timestamp = (uint64_t)time(NULL) * 1000000000ULL;
            edr_ev->header.size = sizeof(EdrEvent);
            edr_queue_push(edr_ev);
            break;
        }
        case PROC_EVENT_EXIT: {
            uint32_t pid  = ev->event_data.exit.process_pid;
            uint32_t tgid = ev->event_data.exit.process_tgid;
            uint32_t exit_code = ev->event_data.exit.exit_code;
            edr_ev = calloc(1, sizeof(EdrEvent));
            if (!edr_ev) break;
            edr_ev->header.version = 1;
            edr_ev->header.type = EDR_EV_PROCESS_EXIT;
            edr_ev->header.pid = tgid;
            edr_ev->header.tid = pid;
            edr_ev->header.u32 = exit_code;
            edr_ev->header.timestamp = (uint64_t)time(NULL) * 1000000000ULL;
            edr_ev->header.size = sizeof(EdrEvent);
            edr_queue_push(edr_ev);
            break;
        }
        default:
            break;
        }
    }
}

void edr_proc_pin_stop(void) {
    if (g_proc_connector_fd >= 0) {
        cn_send_proc_event(g_proc_connector_fd, PROC_CN_MCAST_IGNORE);
        close(g_proc_connector_fd);
        g_proc_connector_fd = -1;
    }
}