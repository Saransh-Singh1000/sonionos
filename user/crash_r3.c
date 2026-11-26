#include "usyscall.h"

__attribute__((section(".text._start")))
void _start(void) {
    const char msg[] = "[CRASH.BIN] Attempting illegal write to Ring 0 kernel RAM (0x00100000)...\n";
    sys_write(msg, sizeof(msg) - 1);

    /* 0x00100000 is Ring 0 Supervisor-Only in paging.c -> triggers CPU #PF! */
    volatile uint64_t *kernel_mem = (volatile uint64_t *)0x00100000ULL;
    *kernel_mem = 0xDEADBEEFCAFEBABEULL;

    sys_exit(0);
}
