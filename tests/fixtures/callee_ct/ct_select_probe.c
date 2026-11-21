/* gcc -O2 lowering probe for the EdDSA callee-CT gate.
 *
 * Constant-time selection MUST lower to a branch-free instruction (cmov or a
 * setcc/mask sequence), never to a conditional jump on the secret condition.
 * tools/eddsa_callee_ct_gate.sh compiles this with the LOCAL gcc at -O2,
 * disassembles it and requires tools/eddsa_callee_asm_check.py to see a cmov /
 * setcc and zero secret-governed jcc.  Never linked into a product build.
 */
int ct_select_cmov(unsigned cond, int x, int y)
{
    return cond ? x : y;
}
