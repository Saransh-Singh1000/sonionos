#include "usyscall.h"

static void print_str(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    sys_write(s, n);
}

static void print_u64(uint64_t v) {
    char buf[24];
    int i = 0;
    if (v == 0) { buf[i++] = '0'; }
    while (v > 0) { buf[i++] = (char)('0' + (v % 10)); v /= 10; }
    while (i > 0) { char c = buf[--i]; sys_write(&c, 1); }
}

__attribute__((section(".text._start")))
void _start(void) {
    print_str("=== sonionOS 1-Second Max Syscall Throughput (Ring 3, CPL = ");
    print_u64(get_current_cpl());
    print_str(") ===\n\n");

    /* 1. Standard 1-by-1 SYSCALLs in 1 exact RTC second */
    print_str("Aligning to RTC second edge...\n");
    int64_t t0 = syscall3(SYS_TIME, 0, 0, 0);
    while (syscall3(SYS_TIME, 0, 0, 0) == t0) {}

    int64_t t_start = syscall3(SYS_TIME, 0, 0, 0);
    uint64_t c0 = u_rdtsc();
    uint64_t count = 0;

    while (syscall3(SYS_TIME, 0, 0, 0) == t_start) {
        for (int i = 0; i < 1000; i++) {
            syscall3(SYS_TELL, 0, 0, 0);
        }
        count += 1000;
    }
    uint64_t c1 = u_rdtsc();
    uint64_t total_cycles = c1 - c0;

    print_str("\n[1] Standard 1-by-1 SYSCALLs / sec : ");
    print_u64(count);
    print_str(" syscalls/sec\n");
    print_str("    Avg Cycles per SYSCALL         : ");
    print_u64(count ? (total_cycles / count) : 0);
    print_str(" cycles\n");
    print_str("    Avg Latency per SYSCALL        : ");
    print_u64(count ? (1000000000ULL / count) : 0);
    print_str(" ns\n\n");

    /* 2. Batched SYS_BATCH (io_uring-style, 256 ops/batch) in 1 exact RTC second */
    uint64_t ops[256][4];
    for (int i = 0; i < 256; i++) {
        ops[i][0] = SYS_TELL;
        ops[i][1] = 0;
        ops[i][2] = 0;
        ops[i][3] = 0;
    }

    t0 = syscall3(SYS_TIME, 0, 0, 0);
    while (syscall3(SYS_TIME, 0, 0, 0) == t0) {}

    t_start = syscall3(SYS_TIME, 0, 0, 0);
    uint64_t batch_ops = 0;
    while (syscall3(SYS_TIME, 0, 0, 0) == t_start) {
        sys_batch(ops, 256);
        batch_ops += 256;
    }

    print_str("[2] Batched SYS_BATCH Ops / sec    : ");
    print_u64(batch_ops);
    print_str(" ops/sec\n");
    print_str("    Avg Latency per Batched Op     : ");
    print_u64(batch_ops ? (1000000000ULL / batch_ops) : 0);
    print_str(" ns\n\n");

    sys_exit(0);
}
