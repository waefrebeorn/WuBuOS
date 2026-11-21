; WUBU_IR_CT: fixture-model optlevel=-O2 (hand-authored IR-shape model of clang -O2 ge_mul_ladder)
; CLEAN MODEL.  Not compiler output: a minimal LLVM IR model of the -O2 shape
; of the shipped ladder.  Fixed 256-trip counter loop, secret bit read from
; mbedtls_mpi_get_bit, swap ^= bit, constant-time data selection (select, not
; br) on all four coordinates in the real function.  No secret branch and no
; secret-derived GEP index.  Used by the analyzer self-test only.
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
  %sel = select i8 %swapped, i8 1, i8 0
  %inext = sub i32 %i, 1
  %done = icmp eq i32 %inext, 0
  br i1 %done, label %exit, label %loop

exit:
  ret i32 0
}

declare i32 @mbedtls_mpi_get_bit(ptr, i32)
