; WUBU_IR_CT: fixture-model optlevel=-O2 (hand-authored IR-shape model of a secret-indexed table)
; LEAK MODEL.  Same -O2 ladder shape but the secret swap is used as a GEP
; index into a table (the other classic CT failure the gate must bound).  The
; checker MUST report an index violation; --expect leak/index passes only if
; it does.  This model is independent of clang availability.
define i32 @ge_mul_ladder(ptr %r, ptr %p, ptr %n) {
entry:
  %swap_slot = alloca i8, align 1
  %table = alloca i8, align 1
  store i8 0, ptr %swap_slot, align 1
  br label %loop

loop:
  %i = phi i32 [ 256, %entry ], [ %inext, %loop ]
  %bit = call i32 @mbedtls_mpi_get_bit(ptr %n, i32 %i)
  %bit8 = trunc i32 %bit to i8
  %swapold = load i8, ptr %swap_slot, align 1
  %swapped = xor i8 %swapold, i8 %bit8
  store i8 %swapped, ptr %swap_slot, align 1
  %idx = zext i8 %swapped to i64
  %slot = getelementptr inbounds i8, ptr %table, i64 %idx
  store i8 1, ptr %slot, align 1
  %inext = sub i32 %i, 1
  %done = icmp eq i32 %inext, 0
  br i1 %done, label %exit, label %loop

exit:
  ret i32 0
}

declare i32 @mbedtls_mpi_get_bit(ptr, i32)
