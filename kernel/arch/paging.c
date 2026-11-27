#include "arch/paging.h"

#define PTE_PRESENT  (1ULL << 0)
#define PTE_WRITABLE (1ULL << 1)
#define PTE_USER     (1ULL << 2)
#define PTE_PS_2MB   (1ULL << 7)

static uint64_t pdpt_fb[512] __attribute__((aligned(4096)));
static uint64_t pd_fb[512]   __attribute__((aligned(4096)));
static uint64_t pml4[512]       __attribute__((aligned(4096)));
static uint64_t pdpt[512]       __attribute__((aligned(4096)));
static uint64_t pd_low[4][512] __attribute__((aligned(4096)));
static uint64_t pd_hi[512]      __attribute__((aligned(4096)));

void paging_init(BootInfo *info) {
    for (int i = 0; i < 512; i++) { pml4[i] = 0; pdpt[i] = 0; pd_hi[i] = 0; }
    uint64_t pdpt_flags = PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    pml4[0] = ((uint64_t)(uintptr_t)pdpt) | pdpt_flags;

    for (uint64_t g = 0; g < 4; g++) {
        pdpt[g] = ((uint64_t)(uintptr_t)pd_low[g]) | pdpt_flags;
        for (uint64_t m = 0; m < 512; m++) {
            uint64_t addr = (g << 30) | (m << 21);
            uint64_t flags = PTE_PRESENT | PTE_WRITABLE | PTE_PS_2MB;
            if (addr >= USER_BASE_ADDR && addr < USER_LIMIT_ADDR) flags |= PTE_USER;
            pd_low[g][m] = addr | flags;
        }
    }
    if (info && info->framebuffer) {
        uint64_t fb = (uint64_t)(uintptr_t)info->framebuffer;
        uint64_t gb = fb >> 30;
        if (gb >= 4 && gb < 512) {
            pdpt[gb] = ((uint64_t)(uintptr_t)pd_hi) | (PTE_PRESENT | PTE_WRITABLE);
            uint64_t base_gb = gb << 30;
            for (uint64_t m = 0; m < 512; m++) {
                pd_hi[m] = (base_gb | (m << 21)) | (PTE_PRESENT | PTE_WRITABLE | PTE_PS_2MB);
            }
        }
    }

    uint64_t cr3 = (uint64_t)(uintptr_t)pml4;
    uint64_t cr0;
    __asm__ volatile ("movq %%cr0, %0" : "=r"(cr0));
    cr0 = (cr0 & ~((1ULL << 2) | (1ULL << 3))) | (1ULL << 1) | (1ULL << 16);
    __asm__ volatile ("movq %0, %%cr0" : : "r"(cr0));
    uint64_t cr4;
    __asm__ volatile ("movq %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 7) | (1ULL << 9) | (1ULL << 10);
    __asm__ volatile ("movq %0, %%cr4" : : "r"(cr4));
    uint32_t elo, ehi; __asm__ volatile ("rdmsr" : "=a"(elo), "=d"(ehi) : "c"(0xC0000080U)); elo |= (1U << 11); __asm__ volatile ("wrmsr" : : "c"(0xC0000080U), "a"(elo), "d"(ehi));
    __asm__ volatile ("movq %0, %%cr3" : : "r"(cr3) : "memory");
}








