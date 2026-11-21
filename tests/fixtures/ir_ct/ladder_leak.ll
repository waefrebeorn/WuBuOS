; WUBU_IR_CT: fixture-model optlevel=-O2 (hand-authored IR-shape model of the `if (swap)` regression)
; LEAK MODEL.  Same -O2 ladder shape as ladder_clean.ll but with the injected
; `if (swap) ge_cswap(&r0, &r1, 1)` regression lowered to a secret-dependent
; `br i1 %leakcond`.  The checker MUST report a branch violation on this file;
; --expect leak passes only if it does.
define i32 @ge_mul_ladder(ptr %r, ptr %p, ptr %n) {
entry:
  %swap_slot = alloca i8, align 1
  store i8 0, ptr %swap_slot, align 1
  br label %loop

loop:
  %i = phi i32 [ 256, %entry ], [ %inext, %loop ]
  %bit = call i32 @mbedtls_mpi_get_bit(ptr %n, i32 %i)
  %bit8 = trunc i32 %bit to i8
  %swapold = load i8, ptr %swap_slot, align 1
  %swapped = xor i8 %swapold, i8 %bit8
  store i8 %swapped, ptr %swap_slot, align 1
  %leakcond = icmp ne i8 %swapped, 0
  br i1 %leakcond, label %do_cswap, label %skip_cswap

do_cswap:
  %ignored = call i32 @ge_cswap(ptr %r, ptr %p, i32 1)
  br label %loop

skip_cswap:
  br label %loop

exit:
  ret i32 0
}

declare i32 @mbedtls_mpi_get_bit(ptr, i32)
declare i32 @ge_cswap(ptr, ptr, i32)
