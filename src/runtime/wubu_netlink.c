/*
 * wubu_netlink.c  --  WuBuOS Netlink RTNETLINK Implementation
 *
 * Extracted from wubu_network.c (2026-07-06): all rtnetlink socket
 * operations and low-level network interface manipulation.
 *
 * C11 only. Encapsulates the netlink socket state.
 */

#define _POSIX_C_SOURCE 200809L
#include "wubu_netlink.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/if.h>
#include <linux/if_bridge.h>
#include <linux/if_vlan.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>

/* -- Netlink socket state ------------------------------------------- */

int g_nl_sock = -1;

/* -- Socket lifecycle ----------------------------------------------- */

int nl_socket_open(void) {
    if (g_nl_sock >= 0) return g_nl_sock;
    g_nl_sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (g_nl_sock < 0) {
        perror("[wubu_net] netlink socket");
        return -1;
    }
    struct sockaddr_nl sa = {0};
    sa.nl_family = AF_NETLINK;
    sa.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR;
    if (bind(g_nl_sock, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        perror("[wubu_net] netlink bind");
        close(g_nl_sock);
        g_nl_sock = -1;
        return -1;
    }
    return g_nl_sock;
}

void nl_socket_close(void) {
    if (g_nl_sock >= 0) {
        close(g_nl_sock);
        g_nl_sock = -1;
    }
}

/* -- Send netlink message and receive ACK --------------------------- */

int nl_send_recv(struct nlmsghdr *nh) {
    int sock = nl_socket_open();
    if (sock < 0) return -1;

    struct sockaddr_nl sa = {0};
    sa.nl_family = AF_NETLINK;

    struct iovec iov = { nh, nh->nlmsg_len };
    struct msghdr msg = { &sa, sizeof(sa), &iov, 1, NULL, 0, 0 };

    if (sendmsg(sock, &msg, 0) < 0) {
        perror("[wubu_net] netlink sendmsg");
        return -1;
    }

    /* Receive ACK */
    char buf[4096];
    iov.iov_base = buf;
    iov.iov_len = sizeof(buf);
    ssize_t len = recvmsg(sock, &msg, 0);
    if (len < 0) {
        perror("[wubu_net] netlink recvmsg");
        return -1;
    }

    /* Parse response for errors */
    struct nlmsghdr *rnh = (struct nlmsghdr *)buf;
    for (; NLMSG_OK(rnh, len); rnh = NLMSG_NEXT(rnh, len)) {
        if (rnh->nlmsg_type == NLMSG_ERROR) {
            struct nlmsgerr *err = (struct nlmsgerr *)NLMSG_DATA(rnh);
            if (err->error < 0) {
                errno = -err->error;
                return - 1;
                return -1;
            }
        }
        if (rnh->nlmsg_type == NLMSG_DONE) break;
    }
    return 0;
}

/* -- Add attribute to netlink message ------------------------------- */

void nl_addattr(struct nlmsghdr *nh, int maxlen, int type, const void *data, int alen) {
    int len = RTA_LENGTH(alen);
    struct rtattr *rta;
    if (NLMSG_ALIGN(nh->nlmsg_len) + len > maxlen) return;
    rta = (struct rtattr *)(((char *)nh) + NLMSG_ALIGN(nh->nlmsg_len));
    rta->rta_type = type;
    rta->rta_len = len;
    memcpy(RTA_DATA(rta), data, alen);
    nh->nlmsg_len = NLMSG_ALIGN(nh->nlmsg_len) + len;
}

/* -- Create link via netlink (bridge, macvlan, ipvlan, vxlan, dummy) -- */

int netlink_link_create(const char *name, const char *kind,
                        const char *parent, int vlan_id,
                        const char *vxlan_id, int vxlan_port,
                        const char *vxlan_remote, bool vxlan_encrypt) {
    char buf[4096];
    struct {
        struct nlmsghdr nh;
        struct ifinfomsg ifi;
        char attrs[4096 - sizeof(struct nlmsghdr) - sizeof(struct ifinfomsg)];
    } req = {0};

    req.nh.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
    req.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL | NLM_F_ACK;
    req.nh.nlmsg_type = RTM_NEWLINK;
    req.ifi.ifi_family = AF_UNSPEC;

    nl_addattr(&req.nh, sizeof(req), IFLA_IFNAME, name, strlen(name) + 1);
    nl_addattr(&req.nh, sizeof(req), IFLA_INFO_KIND, kind, strlen(kind) + 1);

    if (parent && parent[0]) {
        int parent_idx = if_nametoindex(parent);
        if (parent_idx > 0) {
            nl_addattr(&req.nh, sizeof(req), IFLA_LINK, &parent_idx, sizeof(parent_idx));
        }
    }

    /* Kind-specific attributes */
    if (strcmp(kind, "vlan") == 0 && vlan_id > 0) {
        struct ifla_vlan_flags vf = {0};
        vf.flags = 0;
        vf.mask = 0;
        nl_addattr(&req.nh, sizeof(req), IFLA_VLAN_FLAGS, &vf, sizeof(vf));
        nl_addattr(&req.nh, sizeof(req), IFLA_VLAN_ID, &vlan_id, sizeof(vlan_id));
    }

    if (strcmp(kind, "vxlan") == 0) {
        struct ifla_vxlan_port_range vpr = {0, 0};
        if (vxlan_id && vxlan_id[0]) {
            uint32_t vni = strtoul(vxlan_id, NULL, 10);
            nl_addattr(&req.nh, sizeof(req), IFLA_VXLAN_ID, &vni, sizeof(vni));
        }
        if (vxlan_port > 0) {
            nl_addattr(&req.nh, sizeof(req), IFLA_VXLAN_PORT, &vxlan_port, sizeof(vxlan_port));
            vpr.low = vxlan_port;
            vpr.high = vxlan_port;
            nl_addattr(&req.nh, sizeof(req), IFLA_VXLAN_PORT_RANGE, &vpr, sizeof(vpr));
        }
        if (vxlan_remote && vxlan_remote[0]) {
            struct in_addr addr;
            if (inet_pton(AF_INET, vxlan_remote, &addr) == 1) {
                nl_addattr(&req.nh, sizeof(req), IFLA_VXLAN_GROUP, &addr, sizeof(addr));
            }
        }
        /* Note: VXLAN encryption (WireGuard) requires kernel 5.11+ and is complex;
         * we store the config but actual encryption setup is best-effort */
        (void)vxlan_encrypt;
    }

    return nl_send_recv(&req.nh);
}

/* -- Delete link via netlink ---------------------------------------- */

int netlink_link_delete(const char *name) {
    char buf[4096];
    struct {
        struct nlmsghdr nh;
        struct ifinfomsg ifi;
        char attrs[4096 - sizeof(struct nlmsghdr) - sizeof(struct ifinfomsg)];
    } req = {0};

    req.nh.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
    req.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
    req.nh.nlmsg_type = RTM_DELLINK;
    req.ifi.ifi_family = AF_UNSPEC;

    nl_addattr(&req.nh, sizeof(req), IFLA_IFNAME, name, strlen(name) + 1);

    return nl_send_recv(&req.nh);
}

/* -- Set link up/down via netlink ----------------------------------- */

int netlink_link_set_up(const char *name, bool up) {
    char buf[4096];
    struct {
        struct nlmsghdr nh;
        struct ifinfomsg ifi;
        char attrs[4096 - sizeof(struct nlmsghdr) - sizeof(struct ifinfomsg)];
    } req = {0};

    req.nh.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
    req.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
    req.nh.nlmsg_type = RTM_NEWLINK;
    req.ifi.ifi_family = AF_UNSPEC;
    req.ifi.ifi_change = up ? IFF_UP : 0;
    req.ifi.ifi_flags = up ? IFF_UP : 0;

    nl_addattr(&req.nh, sizeof(req), IFLA_IFNAME, name, strlen(name) + 1);

    return nl_send_recv(&req.nh);
}

/* -- Add IP address via netlink ------------------------------------- */

int netlink_addr_add(const char *iface, const char *ip_with_cidr) {
    char buf[4096];
    struct {
        struct nlmsghdr nh;
        struct ifaddrmsg ifa;
        char attrs[4096 - sizeof(struct nlmsghdr) - sizeof(struct ifaddrmsg)];
    } req = {0};

    req.nh.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
    req.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL | NLM_F_ACK;
    req.nh.nlmsg_type = RTM_NEWADDR;
    req.ifa.ifa_family = AF_INET;
    req.ifa.ifa_index = if_nametoindex(iface);
    if (req.ifa.ifa_index == 0) return -1;

    /* Parse CIDR */
    char ip[64];
    int prefix_len = 24;
    char *slash = strchr(ip_with_cidr, '/');
    if (slash) {
        *slash = '\0';
        strncpy(ip, ip_with_cidr, sizeof(ip) - 1);
        prefix_len = atoi(slash + 1);
        *slash = '/';
    } else {
        strncpy(ip, ip_with_cidr, sizeof(ip) - 1);
    }

    struct in_addr addr;
    if (inet_pton(AF_INET, ip, &addr) != 1) return -1;

    nl_addattr(&req.nh, sizeof(req), IFA_LOCAL, &addr, sizeof(addr));
    nl_addattr(&req.nh, sizeof(req), IFA_ADDRESS, &addr, sizeof(addr));
    req.ifa.ifa_prefixlen = prefix_len;

    return nl_send_recv(&req.nh);
}

/* -- Composite: bring interface up with IP -------------------------- */

int net_iface_up(const char *iface, const char *ip_with_cidr) {
    if (netlink_link_set_up(iface, true) < 0) return -1;
    if (netlink_addr_add(iface, ip_with_cidr) < 0) return -1;
    return 0;
}

/* -- Utility: run shell command ------------------------------------- */

int net_cmd(const char *cmd) {
    return system(cmd);
}

/* -- Utility: current time in ms ------------------------------------ */

uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* -- Utility: generate unique ID ------------------------------------ */

void gen_id(char *out, size_t len, const char *prefix) {
    static int counter = 0;
    snprintf(out, len, "%s-%ld-%d", prefix, now_ms(), ++counter);
}