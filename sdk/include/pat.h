#ifndef SONION_PAT_H
#define SONION_PAT_H

#include <stdint.h>

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr(uint32_t msr, uint64_t val) {
    uint32_t lo = (uint32_t)val, hi = (uint32_t)(val >> 32);
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

static inline void enable_write_combining(uintptr_t fb_addr, uint64_t fb_bytes) {
    __asm__ volatile ("cli");

    uint64_t cr0, cr3;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));

    // Temporarily clear CR0.WP (bit 16) so UEFI read-only page tables can be updated safely
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0 & ~(1ULL << 16)) : "memory");

    uint64_t pat = rdmsr(0x277);
    pat = (pat & ~(0xFFULL << 8)) | (0x01ULL << 8);
    wrmsr(0x277, pat);

    uint64_t *pml4 = (uint64_t *)(uintptr_t)(cr3 & ~0xFFFULL);
    uint64_t start = fb_addr & ~0x1FFFFFULL;
    uint64_t end   = (fb_addr + fb_bytes + 0x1FFFFFULL) & ~0x1FFFFFULL;

    for (uint64_t addr = start; addr < end; addr += 0x200000ULL) {
        uint64_t pml4_i = (addr >> 39) & 0x1FF;
        uint64_t pdpt_i = (addr >> 30) & 0x1FF;
        uint64_t pd_i   = (addr >> 21) & 0x1FF;

        if (!(pml4[pml4_i] & 1)) continue;
        uint64_t *pdpt = (uint64_t *)(uintptr_t)(pml4[pml4_i] & ~0xFFFULL);
        if (!(pdpt[pdpt_i] & 1) || (pdpt[pdpt_i] & 0x80)) continue;

        uint64_t *pd = (uint64_t *)(uintptr_t)(pdpt[pdpt_i] & ~0xFFFULL);
        if ((pd[pd_i] & 1) && (pd[pd_i] & 0x80)) {
            pd[pd_i] |= (1ULL << 3);
        }
    }

    __asm__ volatile ("mov %0, %%cr3" : : "r"(cr3) : "memory");
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0) : "memory");
}

#endif
