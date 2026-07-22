/*
 * vsl_mach_ipc.h  --  Mach IPC Types & Constants for macOS Layer
 *
 * Defines the Mach IPC message format and port abstractions
 * needed by the VSL macOS syscall dispatch module. Mirrors
 * the macOS XNU kernel's mach/message.h and mach/port.h.
 *
 * C11, self-contained, no includes required.
 */
#ifndef WUBUOS_VSL_MACH_IPC_H
#define WUBUOS_VSL_MACH_IPC_H

#include <stdint.h>
#include <stdbool.h>

/* ===================================================================
 * Mach Port Types
 * =================================================================== */

typedef uint32_t mach_port_name_t;
typedef mach_port_name_t mach_port_t;
typedef uint32_t mach_port_right_t;

#define MACH_PORT_NULL            0x00000000u
#define MACH_PORT_DEAD            (~(mach_port_name_t)0)

/* Port rights */
#define MACH_PORT_RIGHT_SEND      0
#define MACH_PORT_RIGHT_RECEIVE   1
#define MACH_PORT_RIGHT_SEND_ONCE 2
#define MACH_PORT_RIGHT_PORT_SET  3
#define MACH_PORT_RIGHT_DEAD_NAME 4
#define MACH_PORT_RIGHT_LABEL_H   5
#define MACH_PORT_RIGHT_NUMBER    6

/* ===================================================================
 * mach_msg Types
 * =================================================================== */

typedef uint32_t mach_msg_bits_t;
typedef uint32_t mach_msg_size_t;
typedef uint32_t mach_msg_id_t;
typedef uint32_t mach_msg_timeout_t;
typedef uint32_t mach_msg_option_t;
typedef int      kern_return_t;

/* mach_msg_header */
typedef struct {
    mach_msg_bits_t       msgh_bits;
    mach_msg_size_t       msgh_size;
    mach_port_name_t      msgh_remote_port;
    mach_port_name_t      msgh_local_port;
    mach_port_name_t      msgh_voucher_port;
    mach_msg_id_t         msgh_id;
} mach_msg_header_t;

/* mach_msg_body (trailer after header) */
typedef struct {
    unsigned int msgh_descriptor_count;
} mach_msg_body_t;

/* Simple message (header + inline data) */
#define MAX_TRAILER_SIZE 64
typedef struct {
    mach_msg_header_t header;
    uint8_t           body[MAX_TRAILER_SIZE];
} mach_msg_max_t;

/* ===================================================================
 * mach_msg Return Codes
 * =================================================================== */
#define MACH_MSG_SUCCESS          0x00000000u
#define MACH_SEND_INVALID_DATA    0x10000001u
#define MACH_SEND_INVALID_DEST    0x10000002u
#define MACH_SEND_TIMED_OUT       0x10000004u
#define MACH_SEND_INTERRUPTED     0x10000007u
#define MACH_SEND_MSG_TOO_SMALL   0x10000008u
#define MACH_SEND_INVALID_REPLY   0x10000009u
#define MACH_RCV_INVALID_NAME     0x1000000Au
#define MACH_RCV_TIMED_OUT        0x10000003u
#define MACH_RCV_INTERRUPTED      0x1000000Fu
#define MACH_RCV_PORT_DIED        0x10000012u
#define MACH_RCV_INVALID_SET      0x10000014u
#define MACH_RCV_TOO_LARGE        0x10000015u
#define MACH_RCV_INVALID_TRAILER  0x10000017u

/* ===================================================================
 * mach_msg Option Bits
 * =================================================================== */
#define MACH_MSG_OPTION_NONE      0x00000000u
#define MACH_SEND_MSG             0x00000001u
#define MACH_RCV_MSG              0x00000002u
#define MACH_RCV_LARGE            0x00000004u
#define MACH_SEND_TIMEOUT         0x00000010u
#define MACH_SEND_INTERRUPT       0x00000020u
#define MACH_SEND_NOTIFY          0x00000040u
#define MACH_SEND_CANCEL          0x00000080u
#define MACH_RCV_TIMEOUT          0x00000100u
#define MACH_RCV_INTERRUPT        0x00000200u
#define MACH_RCV_OVERWRITE        0x00000400u
#define MACH_RCV_NOTIFY           0x00000800u

/* mach_msg_bits helpers */
#define MACH_MSGH_BITS(remote, local) \
    (((remote) & 0x1F) | (((local) & 0x1F) << 8))
#define MACH_MSGH_BITS_REMOTE(bits)   ((bits) & 0x1F)
#define MACH_MSGH_BITS_LOCAL(bits)    (((bits) >> 8) & 0x1F)
#define MACH_MSGH_BITS_OTHER(bits)    (((bits) >> 16) & 0x1F)

/* ===================================================================
 * Special Port Names (common XNU values)
 * =================================================================== */
#define MACH_PORT_SPECIAL_BITS    0x00000400u
#define MACH_PORT_SPECIAL_MASK    0x00000FFFu
#define MACH_TASK_SELF            (MACH_PORT_SPECIAL_BITS | 3u)
#define MACH_HOST_SELF            (MACH_PORT_SPECIAL_BITS | 2u)
#define MACH_THREAD_SELF          (MACH_PORT_SPECIAL_BITS | 4u)
#define MACH_REPLY_PORT           (MACH_PORT_SPECIAL_BITS | 5u)

#endif /* WUBUOS_VSL_MACH_IPC_H */
