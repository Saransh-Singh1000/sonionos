#include "arch/gdt.h"

typedef struct __attribute__((packed)) {
    uint32_t rsv0;
    uint64_t rsp0, rsp1, rsp2, rsv1;
    uint64_t ist[7];
    uint64_t rsv2;
    uint16_t rsv3, iomap_base;
} Tss64;

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint64_t base;
} Gdtr64;

static uint64_t gdt[7] __attribute__((aligned(16)));
static Tss64    tss      __attribute__((aligned(16)));
static uint8_t  ist1_stack[8192] __attribute__((aligned(16)));
static uint8_t  kstack[16384] __attribute__((aligned(16)));

void tss_set_rsp0(uint64_t rsp0) { tss.rsp0 = rsp0; }

void gdt_init(void) {
    for (int i = 0; i < 7; i++) gdt[i] = 0;
    uint8_t *tp = (uint8_t *)&tss;
    for (unsigned i = 0; i < sizeof(tss); i++) tp[i] = 0;

    tss.rsp0       = (uint64_t)(uintptr_t)(kstack + sizeof(kstack));
    tss.ist[0]     = (uint64_t)(uintptr_t)(ist1_stack + sizeof(ist1_stack));
    tss.iomap_base = sizeof(Tss64);

    gdt[1] = 0x00AF9A000000FFFFULL;
    gdt[2] = 0x00CF92000000FFFFULL;
    gdt[3] = 0x00CFF2000000FFFFULL;
    gdt[4] = 0x00AFFA000000FFFFULL;

    uint64_t base  = (uint64_t)(uintptr_t)&tss;
    uint64_t limit = sizeof(Tss64) - 1;
    gdt[5] = (limit & 0xFFFFULL) | ((base & 0xFFFFFFULL) << 16) |
               (0x89ULL << 40) | (((limit >> 16) & 0xFULL) << 48) | (((base >> 24) & 0xFFULL) << 56);
    gdt[6] = (base >> 32) & 0xFFFFFFFFULL;

    Gdtr64 gdtr = { sizeof(gdt) - 1, (uint64_t)(uintptr_t)gdt };
    __asm__ volatile (
        "lgdt %0\n\t"
        "pushq $0x08\n\t"
        "leaq 1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n\t"
        "1:\n\t"
        "movw $0x10, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        "movw %%ax, %%ss\n\t"
        : : "m"(gdtr) : "rax", "memory"
    );
    uint16_t tss_sel = GDT_TSS_SEL;
    __asm__ volatile ("ltr %0" : : "r"(tss_sel));
}

