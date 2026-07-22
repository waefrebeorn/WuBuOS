/*
 * vsl_syscall_numbers_mac.h  --  MacOS (XNU) Syscall Numbers
 *
 * macOS syscalls use a class-based encoding in RAX:
 *   Mach traps:    0x00000000 | syscall_num  (class 0)
 *   BSD syscalls:  0x02000000 | syscall_num  (class 2)
 *   Machine dep:   0x04000000 | syscall_num  (class 1)
 *
 * Self-contained header: just #define constants, NO includes, NO structs.
 */
#ifndef WUBUOS_VSL_SYSCALL_NUMBERS_MAC_H
#define WUBUOS_VSL_SYSCALL_NUMBERS_MAC_H

/* Syscall class encoding masks */
#define MAC_SYSCALL_CLASS_MASK    0xFF000000
#define MAC_SYSCALL_NUM_MASK      0x00FFFFFF
#define MAC_SYSCALL_CLASS_SHIFT   24

#define MAC_SYSCALL_CLASS_MACH    0   /* Mach traps */
#define MAC_SYSCALL_CLASS_BSD     2   /* BSD Unix syscalls */
#define MAC_SYSCALL_CLASS_MACH_DEP 1  /* Machine-dependent */

#define MAC_CLASS_MACH_TRAP(num)   (num)
#define MAC_CLASS_BSD_SYSCALL(num) (0x02000000 | (num))

/* ===================================================================
 * MACH TRAPS (class 0)
 * These are low-level Mach microkernel calls.
 * =================================================================== */
#define MAC_MACH_TRAP_BASE        0x00000000

#define MAC_MACH_KERNEL_DEBUG    0   /* kernel_debug */
#define MAC_MACH_MAP             1   /* mach_map */
#define MAC_MACH_MAKE_MEMORY_ENTRY 2 /* mach_make_memory_entry */
#define MAC_MACH_VM_ALLOC         3  /* vm_alloc */
#define MAC_MACH_VM_DEALLOC       4  /* vm_dealloc */
#define MAC_MACH_VM_PROTECT       5  /* vm_protect */
#define MAC_MACH_VM_INHERIT       6  /* vm_inherit */
#define MAC_MACH_VM_READ          7  /* vm_read */
#define MAC_MACH_VM_WRITE         8  /* vm_write */
#define MAC_MACH_VM_COPY          9  /* vm_copy */
#define MAC_MACH_VM_MAP           10 /* vm_map */
#define MAC_MACH_PORT_ALLOC       11 /* mach_port_allocate */
#define MAC_MACH_PORT_DEALLOC     12 /* mach_port_deallocate */
#define MAC_MACH_PORT_INSERT_RCV  13 /* mach_port_insert_receive */
#define MAC_MACH_PORT_DEL_RCV     14 /* mach_port_del_receive */
#define MAC_MACH_PORT_INSERT_SEND 15 /* mach_port_insert_send */
#define MAC_MACH_PORT_DEL_SEND    16 /* mach_port_del_send */
#define MAC_MACH_PORT_GET_STATUS  17 /* mach_port_get_status */
#define MAC_MACH_PORT_SET_RCV_LIM 18 /* mach_port_set_receive_limit */
#define MAC_MACH_PORT_SET_PORT_SET 19 /* mach_port_set_port_set */
#define MAC_MACH_PORT_EXTRACT_MEMBER 20 /* mach_port_extract_member */
#define MAC_MACH_PORT_INSERT_MEMBER 21 /* mach_port_insert_member */
#define MAC_MACH_PORT_DESTRUCTOR  22 /* mach_port_destructor */
#define MAC_MACH_PORT_CONSTRUCT   23 /* mach_port_construct */
#define MAC_MACH_PORT_GET_ATTR    24 /* mach_port_get_attributes */
#define MAC_MACH_PORT_SET_ATTR    25 /* mach_port_set_attributes */
#define MAC_MACH_TASK_SELF        28 /* task_self (get task port) */
#define MAC_MACH_HOST_SELF        29 /* host_self (get host port) */
#define MAC_MACH_THREAD_SELF      30 /* thread_self */
#define MAC_MACH_REPLY_PORT       31 /* mach_reply_port */
#define MAC_MACH_CLOCK_GET_TIME   32 /* clock_get_time */
#define MAC_MACH_CLOCK_SET_TIME   33 /* clock_set_time */
#define MAC_MACH_CLOCK_ALARM      34 /* clock_alarm */
#define MAC_MACH_TIMES            35 /* mach_times */
#define MAC_MACH_VM_STATISTICS    36 /* vm_statistics */
#define MAC_MACH_VM_PAGE_SIZE     37 /* vm_page_size */
#define MAC_MACH_TASK_INFO        38 /* task_info */
#define MAC_MACH_TASK_SET_INFO    39 /* task_set_info */
#define MAC_MACH_THREAD_INFO      40 /* thread_info */
#define MAC_MACH_SWITCH           41 /* mach_switch */
#define MAC_MACH_MSG              42 /* mach_msg (!!! critical) */
#define MAC_MACH_MSG_OVERWRITE    43 /* mach_msg_overwrite */
#define MAC_MACH_SEMAPHORE_SIGNAL 44 /* semaphore_signal */
#define MAC_MACH_SEMAPHORE_WAIT   45 /* semaphore_wait */
#define MAC_MACH_SEMAPHORE_TIMED_WAIT 46 /* semaphore_timedwait */
#define MAC_MACH_SEMAPHORE_SIGNAL_ALL 47 /* semaphore_signal_all */
#define MAC_MACH_SEMAPHORE_CREATE 48 /* semaphore_create */
#define MAC_MACH_SEMAPHORE_DESTROY 49 /* semaphore_destroy */
#define MAC_MACH_RPC_CALL         50 /* rpc_call */
#define MAC_MACH_TASK_CREATE      51 /* task_create */
#define MAC_MACH_TASK_TERMINATE   52 /* task_terminate */
#define MAC_MACH_TASK_THREADS     53 /* task_threads */
#define MAC_MACH_THREAD_CREATE    54 /* thread_create */
#define MAC_MACH_THREAD_TERMINATE 55 /* thread_terminate */
#define MAC_MACH_VM_ALLOCATE      56 /* vm_allocate */
#define MAC_MACH_VM_DEALLOCATE    57 /* vm_deallocate */
#define MAC_MACH_MKEXT            58 /* mkext */
#define MAC_MACH_VM_REGION        60 /* vm_region */
#define MAC_MACH_VM_PREFER        61 /* vm_prefer */
#define MAC_MACH_VM_WIRE          62 /* vm_wire */
#define MAC_MACH_VM_MAP_FILE      63 /* vm_map_file */
#define MAC_MACH_MKFUNCTABLE      64 /* mkfunctable */

/* ===================================================================
 * BSD SYSCALLS (class 2)
 * Unix-compatible syscalls from the BSD layer of XNU.
 * Number = 0x02000000 | syscall_num
 * =================================================================== */
#define MAC_BSD_SYSCALL_BASE      0x02000000

#define MAC_SYS_EXIT              1
#define MAC_SYS_FORK              2
#define MAC_SYS_READ              3
#define MAC_SYS_WRITE             4
#define MAC_SYS_OPEN              5
#define MAC_SYS_CLOSE             6
#define MAC_SYS_WAIT4             7
#define MAC_SYS_LINK              9
#define MAC_SYS_UNLINK            10
#define MAC_SYS_EXECVE            11
#define MAC_SYS_CHDIR             12
#define MAC_SYS_MKDIR             15
#define MAC_SYS_RMDIR             16
#define MAC_SYS_ACCESS            33
#define MAC_SYS_GETPID            20
#define MAC_SYS_GETPPID           39
#define MAC_SYS_GETUID            24
#define MAC_SYS_GETGID            25
#define MAC_SYS_GETEUID           43
#define MAC_SYS_GETEGID           44
#define MAC_SYS_KILL              37
#define MAC_SYS_DUP               41
#define MAC_SYS_PIPE              42
#define MAC_SYS_GETGID_LIST       46
#define MAC_SYS_GETGROUPS         47  /* getgroups */
#define MAC_SYS_SETGROUPS         48  /* setgroups */
#define MAC_SYS_FCNTL             92
#define MAC_SYS_FSYNC             95
#define MAC_SYS_FTRUNCATE         99
#define MAC_SYS_LSEEK             199
#define MAC_SYS_TRUNCATE          200
#define MAC_SYS_STAT              188
#define MAC_SYS_FSTAT             189
#define MAC_SYS_LSTAT             190
#define MAC_SYS_GETCWD            175
#define MAC_SYS_IOCTL             54
#define MAC_SYS_RENAME            128
#define MAC_SYS_SYMLINK           57
#define MAC_SYS_READLINK          58
#define MAC_SYS_GETTIMEOFDAY      116
#define MAC_SYS_SETTIMEOFDAY      79
#define MAC_SYS_SELECT            93
#define MAC_SYS_READV             120
#define MAC_SYS_WRITEV            121
#define MAC_SYS_MMAP              197
#define MAC_SYS_MUNMAP            73
#define MAC_SYS_MADVISE           75
#define MAC_SYS_MPROTECT          74
#define MAC_SYS_MINHERIT          250
#define MAC_SYS_SHARED_REGION     251
#define MAC_SYS_SOCKET            97
#define MAC_SYS_CONNECT           98
#define MAC_SYS_ACCEPT            30
#define MAC_SYS_BIND              104
#define MAC_SYS_LISTEN            105
#define MAC_SYS_SETSOCKOPT        106
#define MAC_SYS_GETSOCKOPT        118
#define MAC_SYS_GETSOCKNAME       119
#define MAC_SYS_GETPEERNAME       120
#define MAC_SYS_SENDTO            96
#define MAC_SYS_RECVFROM          102
#define MAC_SYS_SHUTDOWN          134
#define MAC_SYS_SOCKETPAIR        135
#define MAC_SYS_SIGACTION         46
#define MAC_SYS_SIGPROCMASK       48
#define MAC_SYS_SIGRETURN         50
#define MAC_SYS_PTHREAD_SET_SELF  63
#define MAC_SYS_PSYNCH_mutexwait  301
#define MAC_SYS_PSYNCH_mutexdrop  302
#define MAC_SYS_PSYNCH_cvbroad    303
#define MAC_SYS_PSYNCH_cvsignal   304
#define MAC_SYS_PSYNCH_cvwait     305
#define MAC_SYS_SYSCTL            202  /* sysctl() */
#define MAC_SYS_GETENTROPY        322  /* getentropy() */
#define MAC_SYS_ISSETUGID         327
#define MAC_SYS_PID_SUSPEND       379
#define MAC_SYS_PID_RESUME        380
#define MAC_SYS_GETATTRLIST       261
#define MAC_SYS_SETATTRLIST       262
#define MAC_SYS_EXCHANGEDATA      263
#define MAC_SYS_SEARCHFS          264
#define MAC_SYS_FSGETPATH         343
#define MAC_SYS_MACH_VM_PURGABLE  269
#define MAC_SYS_CS_OPS            296  /* codesign operations */
#define MAC_SYS_PROC_INFO         308
#define MAC_SYS_COALITION         311
#define MAC_SYS_NECP_MATCH_POLICY 322
#define MAC_SYS_KDEBUG_TYPE_FILTER 333
#define MAC_SYS_KDEBUG_TRACE_STRING 334
#define MAC_SYS_TERMINATE_PROCESS 359
#define MAC_SYS_MEMORYSTATUS      365
#define MAC_SYS_WORK_INTERVAL     408
#define MAC_SYS_KERN_CTL          146
#define MAC_SYS_KERN_CTRL         147
#define MAC_SYS_KDEBUG_TRACE      180

/* Maximum supported syscall number */
#define MAC_SYS_MAX               512

/* Additional syscalls for macOS layer */
#define MAC_SYS_ISSETUGID         327
#define MAC_SYS_CS_OPS            296  /* code signing operations */
#define MAC_SYS_PROC_INFO         308  /* process info */
#define MAC_SYS_FSGETPATH         343  /* get path from fd */
#define MAC_SYS_MEMORYSTATUS      365  /* memory pressure status */
#define MAC_SYS_TERMINATE_PROCESS 359  /* terminate with payload */
#define MAC_SYS_GETATTRLIST       261  /* get attribute list */
#define MAC_SYS_SETATTRLIST       262  /* set attribute list */
#define MAC_SYS_EXCHANGEDATA      263  /* exchange data */
#define MAC_SYS_SEARCHFS          264  /* search file system */
#define MAC_SYS_KQUEUE            362  /* kqueue */
#define MAC_SYS_KEVENT_ID         363  /* kevent_id */
#define MAC_SYS_KEVENT64          364  /* kevent64 */
#define MAC_SYS_WORKQ_OPEN        338  /* workq_open */
#define MAC_SYS_WORKQ_KERN_RETURN 339  /* workq_kernreturn */
#define MAC_SYS_PSYNCH_MUTEXWAIT  301  /* psynch_mutexwait */
#define MAC_SYS_PSYNCH_MUTEXDROP  302  /* psynch_mutexdrop */
#define MAC_SYS_PSYNCH_CVBROAD    303  /* psynch_cvbroad */
#define MAC_SYS_PSYNCH_CVSIGNAL   304  /* psynch_cvsignal */
#define MAC_SYS_PSYNCH_CVWAIT     305  /* psynch_cvwait */
#define MAC_SYS_SHM_OPEN          127  /* shm_open (POSIX) */
#define MAC_SYS_SHM_UNLINK        128  /* shm_unlink (NOTE: different from symlink=57) */
#define MAC_SYS_SEM_OPEN          126  /* sem_open */
#define MAC_SYS_SEM_UNLINK        130  /* sem_unlink */
#define MAC_SYS_SEM_WAIT          115  /* sem_wait */
#define MAC_SYS_SEM_TRYWAIT       116  /* sem_trywait */
#define MAC_SYS_SEM_POST          117  /* sem_post */
#define MAC_SYS_SEM_GETVALUE      118  /* sem_getvalue */
#define MAC_SYS_PID_SUSPEND       379  /* pid_suspend */
#define MAC_SYS_PID_RESUME        380  /* pid_resume */

#endif /* WUBUOS_VSL_SYSCALL_NUMBERS_MAC_H */
