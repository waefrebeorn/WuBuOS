/*
 * myseed_jit_stub.c  --  50-line JIT proof-of-concept
 * 
 * Demonstrates mmap-based JIT: write machine code into
 * executable memory and call it as a C function pointer.
 * 
 * This is the foundational primitive for the My Seed OS's
 * HolyC/ZealC JIT runtime.
 * 
 * Build:  cc -o jit_stub myseed_jit_stub.c
 * Run:    ./jit_stub
 * 
 * Expected output:
 *   JIT: add(3, 4) = 7
 *   JIT: square(5) = 25
 *   JIT: fibonacci(10) = 55
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

/* Allocate RWX memory and copy machine code into it */
static void *jit_alloc_exec(size_t size) {
    void *mem = mmap(NULL, size, 
                     PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }
    return mem;
}

/* 
 * Emit x86-64 function: int add(int a, int b) { return a + b; }
 * 
 * ABI: a in edi, b in esi, return in eax
 * 
 *   01 d7        add  eax, edi    ; eax = a  (edi moved to eax by mov eax,edi)
 *   01 f0        add  eax, esi    ; eax += b
 *   c3           ret
 * 
 * Actually need:
 *   89 f8        mov  eax, edi    ; eax = a
 *   01 f0        add  eax, esi    ; eax += b
 *   c3           ret
 */
static void emit_add(unsigned char *buf) {
    buf[0] = 0x89; buf[1] = 0xf8;   /* mov eax, edi */
    buf[2] = 0x01; buf[3] = 0xf0;   /* add eax, esi */
    buf[4] = 0xc3;                   /* ret          */
}

/*
 * Emit x86-64 function: int square(int n) { return n * n; }
 * 
 * ABI: n in edi, return in eax
 * 
 *   89 f8        mov  eax, edi    ; eax = n
 *   0f af c7     imul eax, edi    ; eax = n * n
 *   c3           ret
 */
static void emit_square(unsigned char *buf) {
    buf[0] = 0x89; buf[1] = 0xf8;          /* mov eax, edi    */
    buf[2] = 0x0f; buf[3] = 0xaf; buf[4] = 0xc7;  /* imul eax, edi */
    buf[5] = 0xc3;                          /* ret             */
}

/*
 * Emit x86-64 function: int fibonacci(int n)
 *   if (n <= 1) return n;
 *   return fibonacci(n-1) + fibonacci(n-2);  // iterative version
 * 
 * Iterative: a=0, b=1; for i in 2..n: t=a+b, a=b, b=t; return b
 * 
 * ABI: n in edi, return in eax
 * Uses: eax=a, ecx=b, edx=temp, r8d=i
 * 
 *   89 f8           mov  eax, edi     ; if n <= 1, return n
 *   83 ff 01        cmp  edi, 1
 *   7e 17           jle  .ret
 *   31 c0           xor  eax, eax     ; a = 0
 *   b9 01 00 00 00  mov  ecx, 1       ; b = 1
 *   83 ff 02        cmp  edi, 2
 *   7e 0b           jle  .done
 *   41 b8 02 00 00 00  mov r8d, 2    ; i = 2
 * .loop:
 *   89 c8           mov  eax, ecx     ; t = b    (reuse eax as temp -- actually we need a+b)
 *   [simpler: edx=eax+ecx, eax=ecx, ecx=edx]
 *   89 d2           mov  edx, eax     ; wait, let me just emit the right bytes
 *   
 * Actually let me just hand-encode this properly:
 */
static void emit_fibonacci(unsigned char *buf) {
    /* 
     * int fib(int n) {  // n in edi
     *   int a = 0, b = 1;  // eax=a, ecx=b
     *   if (n <= 1) return n;
     *   for (int i = 2; i <= n; i++) { int t = a + b; a = b; b = t; }  
     *   return b;
     * }
     */
    int i = 0;
    /* if n <= 1, return n */
    buf[i++] = 0x89; buf[i++] = 0xf8;        /* mov eax, edi       */
    buf[i++] = 0x83; buf[i++] = 0xff; buf[i++] = 0x01;  /* cmp edi, 1 */
    buf[i++] = 0x7e; buf[i++] = 0x1a;        /* jle .ret (+26)     */
    
    /* a = 0 (eax), b = 1 (ecx) */
    buf[i++] = 0x31; buf[i++] = 0xc0;        /* xor eax, eax       */
    buf[i++] = 0xb9; buf[i++] = 0x01; buf[i++] = 0x00; buf[i++] = 0x00; buf[i++] = 0x00;  /* mov ecx, 1 */
    
    /* i = 2 (r8d) */
    buf[i++] = 0x41; buf[i++] = 0xb8; buf[i++] = 0x02; buf[i++] = 0x00; buf[i++] = 0x00; buf[i++] = 0x00;  /* mov r8d, 2 */
    
    /* .loop: */
    int loop_start = i;
    /* if i > n, break */
    buf[i++] = 0x44; buf[i++] = 0x39; buf[i++] = 0xc7;  /* cmp edi, r8d */
    buf[i++] = 0x7c; buf[i++] = 0x0b;        /* jl .done (+11)    */
    
    /* t = a + b (edx) */
    buf[i++] = 0x89; buf[i++] = 0xc8;        /* mov eax, eax (nop -- actually mov edx,eax) */
    /* Fix: mov edx, eax */
    i -= 2;
    buf[i++] = 0x89; buf[i++] = 0xc2;        /* mov edx, eax       */
    buf[i++] = 0x01; buf[i++] = 0xca;        /* add edx, ecx       */
    
    /* a = b */
    buf[i++] = 0x89; buf[i++] = 0xc8;        /* mov eax, ecx       */
    
    /* b = t */
    buf[i++] = 0x89; buf[i++] = 0xd1;        /* mov ecx, edx       */
    
    /* i++ */
    buf[i++] = 0x41; buf[i++] = 0xff; buf[i++] = 0xc0;  /* inc r8d */
    
    /* jmp .loop */
    int loop_end = i;
    buf[i++] = 0xeb;                          /* jmp .loop          */
    buf[i++] = (unsigned char)(loop_start - loop_end - 2);  /* relative offset */
    
    /* .done: return b (ecx → eax) */
    buf[i++] = 0x89; buf[i++] = 0xc8;        /* mov eax, ecx       */
    
    /* .ret: */
    buf[i++] = 0xc3;                          /* ret                */
}

int main(void) {
    typedef int (*jit_fn_2i)(int, int);
    typedef int (*jit_fn_1i)(int);
    
    /* Test 1: add(a, b) */
    void *add_mem = jit_alloc_exec(4096);
    if (!add_mem) return 1;
    emit_add((unsigned char *)add_mem);
    jit_fn_2i add_fn = (jit_fn_2i)add_mem;
    printf("JIT: add(3, 4) = %d\n", add_fn(3, 4));
    printf("JIT: add(-1, 10) = %d\n", add_fn(-1, 10));
    
    /* Test 2: square(n) */
    void *sq_mem = jit_alloc_exec(4096);
    if (!sq_mem) return 1;
    emit_square((unsigned char *)sq_mem);
    jit_fn_1i sq_fn = (jit_fn_1i)sq_mem;
    printf("JIT: square(5) = %d\n", sq_fn(5));
    printf("JIT: square(12) = %d\n", sq_fn(12));
    
    /* Test 3: fibonacci(n) */
    void *fib_mem = jit_alloc_exec(4096);
    if (!fib_mem) return 1;
    emit_fibonacci((unsigned char *)fib_mem);
    jit_fn_1i fib_fn = (jit_fn_1i)fib_mem;
    printf("JIT: fibonacci(0) = %d\n", fib_fn(0));
    printf("JIT: fibonacci(1) = %d\n", fib_fn(1));
    printf("JIT: fibonacci(10) = %d\n", fib_fn(10));
    printf("JIT: fibonacci(20) = %d\n", fib_fn(20));
    
    return 0;
}
