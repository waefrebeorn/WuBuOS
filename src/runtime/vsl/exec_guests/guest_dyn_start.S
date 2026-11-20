/* _start for the real ET_DYN fixture.  Raw syscalls only (no libc); both
 * syscalls are trapped by VSL's seccomp filter.  The write buffer is the
 * pointer relocated by R_X86_64_RELATIVE, so a missing/incorrect relocation
 * produces wrong output or a fault. */
    .text
    .globl _start
    .hidden guest_ptr
    .type _start, @function
_start:
    lea guest_ptr(%rip), %rbx       /* address of the pointer slot */
    mov (%rbx), %rsi                /* relocated pointer -> string */
    mov $1, %eax                    /* SYS_write */
    mov $1, %edi                    /* fd 1 */
    mov $11, %edx                   /* "VSL-DYN-SO\n" */
    syscall
    mov $60, %eax                   /* SYS_exit */
    xor %edi, %edi
    syscall
    .size _start, .-_start
    .section .note.GNU-stack,"",@progbits
