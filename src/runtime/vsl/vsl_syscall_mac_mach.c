/*
 * vsl_syscall_mac_mach.c  --  VSL macOS Mach Trap Handlers
 *
 * Mach trap handlers, port table, mach_msg, and Mach dispatch table.
 * Called via vsl_mac_syscall_dispatch() in vsl_syscall_mac.c.
 *
 * C11, self-contained via vsl_syscall_mac_internal.h.
 */
#include "vsl/vsl_syscall_mac_internal.h"

/* ===================================================================
 * MACH TRAP HANDLERS (first batch)
 * =================================================================== */

int64_t mac_trap_task_self(uint64_t a, uint64_t b, uint64_t c,
                                   uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
    return (int64_t)MACH_TASK_SELF;
}

int64_t mac_trap_host_self(uint64_t a, uint64_t b, uint64_t c,
                                   uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
    return (int64_t)MACH_HOST_SELF;
}

int64_t mac_trap_thread_self(uint64_t a, uint64_t b, uint64_t c,
                                     uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
    return (int64_t)MACH_THREAD_SELF;
}

int64_t mac_trap_mach_reply_port(uint64_t a, uint64_t b, uint64_t c,
                                         uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
    return (int64_t)MACH_REPLY_PORT;
}

int64_t mac_trap_vm_allocate(uint64_t a, uint64_t b, uint64_t c,
                                     uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)e;(void)f;
    size_t size = (size_t)c;
    void *addr = NULL;
    if (b) addr = (void*)*(uint64_t*)(uintptr_t)b;
    int r = posix_memalign(&addr, 4096, size);
    if (r != 0) return mac_errno(r);
    memset(addr, 0, size);
    if (b) *(uint64_t*)(uintptr_t)b = (uint64_t)(uintptr_t)addr;
    return 0;
}

int64_t mac_trap_vm_deallocate(uint64_t a, uint64_t b, uint64_t c,
                                       uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)d;(void)e;(void)f;
    free((void*)(uintptr_t)b);
    return 0;
}

int64_t mac_trap_vm_protect(uint64_t a, uint64_t b, uint64_t c,
                                    uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)e;(void)f;
    int prot = 0;
    if ((int)d & 1) prot |= PROT_READ;
    if ((int)d & 2) prot |= PROT_WRITE;
    if ((int)d & 4) prot |= PROT_EXEC;
    if (mprotect((void*)(uintptr_t)b, (size_t)c, prot) < 0)
        return mac_errno(errno);
    return 0;
}

/* ===================================================================
 * Mach Port Table (simplified in-process)
 * =================================================================== */

mac_port_t g_mac_ports[MAC_MAX_PORTS];
int g_mac_n_ports = 4;  /* 0-3 reserved */

mach_port_name_t mac_port_alloc(int type, void *object) {
    if (g_mac_n_ports >= MAC_MAX_PORTS) return 0;
    mach_port_name_t name = (mach_port_name_t)g_mac_n_ports + 0x100;
    g_mac_ports[g_mac_n_ports].name = name;
    g_mac_ports[g_mac_n_ports].type = type;
    g_mac_ports[g_mac_n_ports].object = object;
    g_mac_n_ports++;
    return name;
}

mac_port_t *mac_port_lookup(mach_port_name_t name) {
    for (int i = 0; i < g_mac_n_ports; i++) {
        if (g_mac_ports[i].name == name) return &g_mac_ports[i];
    }
    return NULL;
}

/* Initialize reserved ports */
__attribute__((constructor))
static void mac_ports_init(void) {
    g_mac_ports[0].name = 0x103;  /* task_self */
    g_mac_ports[0].type = 1;
    g_mac_ports[1].name = 0x102;  /* host_self */
    g_mac_ports[1].type = 2;
    g_mac_ports[2].name = 0x104;  /* thread_self */
    g_mac_ports[2].type = 3;
    g_mac_ports[3].name = 0x105;  /* reply_port */
    g_mac_ports[3].type = 5;
    g_mac_n_ports = 4;
    (void)mac_port_alloc; /* suppress unused warning */
    (void)mac_port_lookup;
}

/* ===================================================================
 * MACH TRAP HANDLERS (continued)
 * =================================================================== */

int64_t mac_trap_mach_msg(uint64_t a, uint64_t b, uint64_t c,
                                  uint64_t d, uint64_t e, uint64_t f) {
    void *msg_addr = (void*)(uintptr_t)a;
    uint32_t option = (uint32_t)b;
    mach_msg_size_t send_size = (mach_msg_size_t)c;
    mach_msg_size_t receive_limit = (mach_msg_size_t)d;
    mach_port_name_t receive_name = (mach_port_name_t)(uint64_t)e;
    mach_msg_timeout_t timeout = (mach_msg_timeout_t)(uint64_t)f;
    
    (void)send_size;
    (void)receive_limit;
    (void)receive_name;
    (void)timeout;
    
    if (!msg_addr) return MACH_SEND_INVALID_DATA;
    
    mach_msg_header_t header;
    memcpy(&header, msg_addr, sizeof(header));
    
    uint32_t msg_option = option & 0xFF;
    bool is_send = (msg_option & MACH_SEND_MSG) != 0;
    bool is_receive = (msg_option & MACH_RCV_MSG) != 0;
    
    if (is_send) {
        mach_port_name_t remote = header.msgh_remote_port;
        mach_port_name_t local = header.msgh_local_port;
        mach_msg_id_t msgh_id = header.msgh_id;
        (void)remote;
        (void)local;
        
        if (msgh_id == 0x48C || msgh_id == 0x1E) {
            return MACH_MSG_SUCCESS;
        }
        
        if (is_receive) {
            if (msg_addr) {
                header.msgh_size = sizeof(header);
                header.msgh_bits = 0;
                header.msgh_remote_port = 0;
                header.msgh_local_port = local;
                memcpy(msg_addr, &header, sizeof(header));
            }
            return MACH_MSG_SUCCESS;
        }
        return MACH_MSG_SUCCESS;
    }
    
    if (is_receive) {
        return MACH_RCV_TIMED_OUT;
    }
    
    return MACH_MSG_SUCCESS;
}

int64_t mac_trap_semaphore_create(uint64_t a, uint64_t b, uint64_t c,
                                          uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)d;(void)e;(void)f;
    if (g_n_mac_sems >= 64) return mac_errno(ENOSPC);
    mac_sem_t *ms = &g_mac_sems[g_n_mac_sems++];
    snprintf(ms->name, sizeof(ms->name), "/wubu-mac-sem-%d-%d", getpid(), g_n_mac_sems);
    sem_unlink(ms->name);
    ms->sem = sem_open(ms->name, O_CREAT | O_EXCL, 0644, (unsigned)c);
    if (ms->sem == SEM_FAILED) return mac_errno(errno);
    return (int64_t)(uintptr_t)ms;
}

int64_t mac_trap_semaphore_destroy(uint64_t a, uint64_t b, uint64_t c,
                                           uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)c;(void)d;(void)e;(void)f;
    mac_sem_t *ms = (mac_sem_t*)(uintptr_t)b;
    if (ms) { if (ms->sem) { sem_close(ms->sem); sem_unlink(ms->name); } }
    return 0;
}

int64_t mac_trap_semaphore_wait(uint64_t a, uint64_t b, uint64_t c,
                                        uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)c;(void)d;(void)e;(void)f;
    mac_sem_t *ms = (mac_sem_t*)(uintptr_t)b;
    if (!ms || !ms->sem) return mac_errno(EINVAL);
    if (sem_wait(ms->sem) < 0) return mac_errno(errno);
    return 0;
}

int64_t mac_trap_semaphore_signal(uint64_t a, uint64_t b, uint64_t c,
                                          uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)c;(void)d;(void)e;(void)f;
    mac_sem_t *ms = (mac_sem_t*)(uintptr_t)b;
    if (!ms || !ms->sem) return mac_errno(EINVAL);
    if (sem_post(ms->sem) < 0) return mac_errno(errno);
    return 0;
}

int64_t mac_trap_vm_statistics(uint64_t a, uint64_t b, uint64_t c,
                                       uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)d;(void)e;(void)f;
    if (c) {
        uint32_t *stats = (uint32_t*)(uintptr_t)b;
        stats[0] = 0;   stats[1] = 0;
        stats[2] = 0;   stats[3] = 0;
        stats[4] = 0;
    }
    return 0;
}

/* -- Mach trap dispatch table ------------------------------------- */
const mac_syscall_fn mac_trap_table[96] = {
    [MAC_MACH_TASK_SELF]         = mac_trap_task_self,
    [MAC_MACH_HOST_SELF]         = mac_trap_host_self,
    [MAC_MACH_THREAD_SELF]       = mac_trap_thread_self,
    [MAC_MACH_REPLY_PORT]        = mac_trap_mach_reply_port,
    [MAC_MACH_VM_ALLOCATE]       = mac_trap_vm_allocate,
    [MAC_MACH_VM_DEALLOCATE]     = mac_trap_vm_deallocate,
    [MAC_MACH_VM_PROTECT]        = mac_trap_vm_protect,
    [MAC_MACH_MSG]               = mac_trap_mach_msg,
    [MAC_MACH_SEMAPHORE_CREATE]  = mac_trap_semaphore_create,
    [MAC_MACH_SEMAPHORE_DESTROY] = mac_trap_semaphore_destroy,
    [MAC_MACH_SEMAPHORE_WAIT]    = mac_trap_semaphore_wait,
    [MAC_MACH_SEMAPHORE_SIGNAL]  = mac_trap_semaphore_signal,
    [MAC_MACH_VM_STATISTICS]     = mac_trap_vm_statistics,
};
