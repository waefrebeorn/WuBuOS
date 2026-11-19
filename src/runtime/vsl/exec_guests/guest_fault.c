/* Guest: NULL write -> SIGSEGV. Engine must report ERR_FAULT and survive. */
void _start(void) {
    *(volatile long *)0 = 1;
    for (;;) {}
}
