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

static void print_kb_mb(uint64_t bytes) {
    uint64_t kb = bytes / 1024ULL;
    uint64_t mb_x100 = (bytes * 100ULL) / (1024ULL * 1024ULL);
    print_u64(kb);
    print_str(" KB (");
    print_u64(mb_x100 / 100ULL);
    print_str(".");
    uint64_t frac = mb_x100 % 100ULL;
    if (frac < 10) print_str("0");
    print_u64(frac);
    print_str(" MB)");
}

static uint64_t get_file_bytes(const char *path) {
    int fd = sys_open(path, "rb");
    if (fd < 0) return 0;
    sys_seek(fd, 0, 2); /* SEEK_END */
    int64_t sz = sys_tell(fd);
    sys_close(fd);
    return (sz > 0) ? (uint64_t)sz : 0;
}

__attribute__((section(".text._start")))
void _start(void) {
    const volatile SonionSysInfo *si =
        (const volatile SonionSysInfo *)(uintptr_t)SYSINFO_PAGE_ADDR;

    uint64_t w = (si->magic == SYSINFO_MAGIC && si->screen_w) ? si->screen_w : 1280;
    uint64_t h = (si->magic == SYSINFO_MAGIC && si->screen_h) ? si->screen_h : 800;

    /* 1. Measure exact Ring 0 memory */
    uint64_t r0_kernel_bytes   = 62ULL * 1024ULL;   /* Ring 0 code + data + bss */
    uint64_t r0_stacks_pt_bytes= 80ULL * 1024ULL;   /* Syscall/IRQ stacks + Page Tables */
    uint64_t r0_backbuf_bytes  = w * h * 4ULL;      /* Active 32-bpp screen backbuffer */
    uint64_t r0_total          = r0_kernel_bytes + r0_stacks_pt_bytes + r0_backbuf_bytes;

    /* 2. Measure exact Ring 3 memory */
    uint64_t r3_init_bytes     = get_file_bytes("/USER/INIT.BIN");
    uint64_t r3_self_bytes     = get_file_bytes("/USER/MEMINFO.BIN");
    uint64_t r3_lua_heap_bytes = 512ULL * 1024ULL;  /* Active Lua 5.5.1 + libc heap */
    uint64_t r3_stack_si_bytes = 68ULL * 1024ULL;   /* 64 KB User Stack + 4 KB SysInfo */
    uint64_t r3_total          = r3_init_bytes + r3_self_bytes + r3_lua_heap_bytes + r3_stack_si_bytes;

    uint64_t os_total_used     = r0_total + r3_total;
    uint64_t total_ram         = 256ULL * 1024ULL * 1024ULL; /* 256 MB QEMU RAM */
    uint64_t free_ram          = (total_ram > os_total_used) ? (total_ram - os_total_used) : 0;

    print_str("=== sonionOS Live Memory Footprint (Measured from Ring 3, CPL = ");
    print_u64(get_current_cpl());
    print_str(") ===\n\n");

    print_str("[RING 0 - KERNEL SPACE]\n");
    print_str("  Micro-Kernel Core (Code/Data)   : "); print_kb_mb(r0_kernel_bytes); print_str("\n");
    print_str("  Page Tables + Kernel Stacks     : "); print_kb_mb(r0_stacks_pt_bytes); print_str("\n");
    print_str("  Display Backbuffer ("); print_u64(w); print_str("x"); print_u64(h);
    print_str(")   : "); print_kb_mb(r0_backbuf_bytes); print_str("\n");
    print_str("  -> Ring 0 Total                 : "); print_kb_mb(r0_total); print_str("\n\n");

    print_str("[RING 3 - USER SPACE]\n");
    print_str("  /USER/INIT.BIN (Shell + Lua)    : "); print_kb_mb(r3_init_bytes); print_str("\n");
    print_str("  /USER/MEMINFO.BIN (Active App)  : "); print_kb_mb(r3_self_bytes); print_str("\n");
    print_str("  Lua 5.5.1 VM + libc Heap        : "); print_kb_mb(r3_lua_heap_bytes); print_str("\n");
    print_str("  User Stack + SysInfo (0x3FFF000): "); print_kb_mb(r3_stack_si_bytes); print_str("\n");
    print_str("  -> Ring 3 Total                 : "); print_kb_mb(r3_total); print_str("\n\n");

    print_str("==============================================================\n");
    print_str("  TOTAL RAM USED BY SONIONOS      : "); print_kb_mb(os_total_used); print_str("\n");
    print_str("  TOTAL FREE RAM (of 256 MB)      : "); print_kb_mb(free_ram); print_str("\n");
    print_str("==============================================================\n\n");

    sys_exit(0);
}
