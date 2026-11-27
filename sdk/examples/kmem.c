#include <stdio.h>
#include "usyscall.h"

int main(void) {
    uint64_t m[7] = {0};
    sys_kmem(m);

    uint64_t heap_tot_kb  = m[0] / 1024ULL;
    uint64_t heap_used_kb = m[1] / 1024ULL;
    uint64_t heap_free_kb = m[2] / 1024ULL;
    uint64_t backbuf_kb   = m[5] / 1024ULL;
    uint64_t kstacks_kb   = m[6] / 1024ULL;
    uint64_t total_os_kb  = heap_used_kb + backbuf_kb + kstacks_kb;

    printf("=== sonionOS Ring 0 Kernel Memory Footprint ===\n");
    printf("Kernel Heap Pool   : %llu KB total (%llu KB used / %llu KB free)\n",
           (unsigned long long)heap_tot_kb,
           (unsigned long long)heap_used_kb,
           (unsigned long long)heap_free_kb);
    printf("Kernel Heap Blocks : %llu allocated, %llu free\n",
           (unsigned long long)m[3], (unsigned long long)m[4]);
    printf("Compositor Backbuf : %llu KB (VRAM double-buffer)\n",
           (unsigned long long)backbuf_kb);
    printf("Ring 0 Stacks      : %llu KB (Kernel + IST1 + 8 Task Stacks)\n",
           (unsigned long long)kstacks_kb);
    printf("----------------------------------------------\n");
    printf("Active Ring 0 OS Footprint: %llu KB (~%llu MB)\n",
           (unsigned long long)total_os_kb,
           (unsigned long long)(total_os_kb / 1024ULL));
    return 0;
}
