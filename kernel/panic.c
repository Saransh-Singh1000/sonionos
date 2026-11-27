#include "panic.h"
#include "arch/syscall.h"
#include "arch/syscall.h"

typedef struct __attribute__((packed)) {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} IdtEntry;

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint64_t base;
} IdtPtr;

typedef struct __attribute__((packed)) {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
} ExceptionFrame;

static IdtEntry idt[256];
static BootInfo *p_info = 0;

static const uint8_t pfont[95][5] = {
    {0,0,0,0,0},{0,0,95,0,0},{0,7,0,7,0},{20,127,20,127,20},{36,42,127,42,18},
    {35,19,8,100,98},{54,73,85,34,80},{0,5,3,0,0},{0,28,34,65,0},{0,65,34,28,0},
    {20,8,62,8,20},{8,8,62,8,8},{0,80,48,0,0},{8,8,8,8,8},{0,96,96,0,0},
    {32,16,8,4,2},{62,81,73,69,62},{0,66,127,64,0},{66,97,81,73,70},{33,65,69,75,49},
    {24,20,18,127,16},{39,69,69,69,57},{60,74,73,73,48},{1,113,9,5,3},{54,73,73,73,54},
    {6,73,73,41,30},{0,54,54,0,0},{0,86,54,0,0},{8,20,34,65,0},{20,20,20,20,20},
    {0,65,34,20,8},{2,1,81,9,6},{50,73,121,65,62},{126,17,17,17,126},{127,73,73,73,54},
    {62,65,65,65,34},{127,65,65,34,28},{127,73,73,73,65},{127,9,9,9,1},{62,65,73,73,122},
    {127,8,8,8,127},{0,65,127,65,0},{32,64,65,63,1},{127,8,20,34,65},{127,64,64,64,64},
    {127,2,12,2,127},{127,4,8,16,127},{62,65,65,65,62},{127,9,9,9,6},{62,65,81,33,94},
    {127,9,25,41,70},{70,73,73,73,49},{1,1,127,1,1},{63,64,64,64,63},{31,32,64,32,31},
    {63,64,56,64,63},{99,20,8,20,99},{7,8,112,8,7},{97,81,73,69,67},{0,127,65,65,0},
    {2,4,8,16,32},{0,65,65,127,0},{4,2,1,2,4},{64,64,64,64,64},{0,1,2,4,0},
    {32,84,84,84,120},{127,72,68,68,56},{56,68,68,68,32},{56,68,68,72,127},{56,84,84,84,24},
    {8,126,9,1,2},{12,82,82,82,62},{127,8,4,4,120},{0,68,125,64,0},{32,64,68,61,0},
    {127,16,40,68,0},{0,65,127,64,0},{124,4,24,4,120},{124,8,4,4,120},{56,68,68,68,56},
    {124,20,20,20,8},{8,20,20,24,124},{124,8,4,4,8},{72,84,84,84,32},{4,63,68,64,32},
    {60,64,64,32,124},{28,32,64,32,28},{60,64,48,64,60},{68,40,16,40,68},{12,80,80,80,60},
    {68,100,84,76,68},{0,8,54,65,0},{0,0,127,0,0},{0,65,54,8,0},{16,8,8,16,8}
};

static const char *exc_names[32] = {
    "Divide By Zero (#DE)", "Debug (#DB)", "Non-Maskable Interrupt (NMI)", "Breakpoint (#BP)",
    "Overflow (#OF)", "Bound Range Exceeded (#BR)", "Invalid Opcode (#UD)", "Device Not Available (#NM)",
    "Double Fault (#DF)", "Coprocessor Segment Overrun", "Invalid TSS (#TS)", "Segment Not Present (#NP)",
    "Stack-Segment Fault (#SS)", "General Protection Fault (#GP)", "Page Fault (#PF)", "Reserved (15)",
    "x87 Floating-Point (#MF)", "Alignment Check (#AC)", "Machine Check (#MC)", "SIMD Floating-Point (#XM)",
    "Virtualization (#VE)", "Control Protection (#CP)", "Reserved (22)", "Reserved (23)",
    "Reserved (24)", "Reserved (25)", "Reserved (26)", "Reserved (27)",
    "Hypervisor Injection (#HV)", "VMM Communication (#VC)", "Security Exception (#SX)", "Reserved (31)"
};

static void vram_char(char c, int x, int y, uint32_t col, int scale) {
    if (!p_info || c < 32 || c > 126) return;
    for (int i = 0; i < 5; i++) {
        uint8_t line = pfont[c - 32][i];
        for (int j = 0; j < 7; j++) {
            if (line & (1 << j)) {
                for (int dy = 0; dy < scale; dy++) {
                    int py = y + j * scale + dy;
                    if (py < 0 || py >= (int)p_info->height) continue;
                    for (int dx = 0; dx < scale; dx++) {
                        int px = x + i * scale + dx;
                        if (px >= 0 && px < (int)p_info->width)
                            p_info->framebuffer[py * p_info->pitch + px] = col;
                    }
                }
            }
        }
    }
}

static void vram_str(const char *s, int x, int y, uint32_t col, int scale) {
    while (*s) { vram_char(*s++, x, y, col, scale); x += 6 * scale; }
}

static void vram_hex(const char *label, uint64_t val, int x, int y) {
    char buf[32];
    int p = 0;
    while (label[p]) { buf[p] = label[p]; p++; }
    buf[p++] = '0';
    buf[p++] = 'x';
    const char *hex = "0123456789ABCDEF";
    for (int s = 60; s >= 0; s -= 4) {
        buf[p++] = hex[(val >> s) & 0xF];
    }
    buf[p] = 0;
    vram_str(buf, x, y, 0x00F8FAFC, 2);
}

static void halt_with_reboot(void) {
    InputState in;
    while (1) {
        if (p_info && p_info->poll_input) {
            p_info->poll_input(&in);
            if (in.scan_code == 0x17 && p_info->reboot) p_info->reboot();
        }
        __asm__ volatile ("pause");
    }
}

static void paint_panic_bg(void) {
    if (!p_info || !p_info->framebuffer) return;
    for (uint32_t y = 0; y < p_info->height; y++) {
        uint32_t col = (y < 64) ? 0x00991B1B : 0x000F172A;
        for (uint32_t x = 0; x < p_info->width; x++) {
            p_info->framebuffer[y * p_info->pitch + x] = col;
        }
    }
    vram_str("SONIONOS KERNEL PANIC - SYSTEM HALTED", 28, 22, 0x00FFFFFF, 2);
    vram_str("Press ESC to reboot machine...", 28, (int)p_info->height - 40, 0x0094A3B8, 2);
}

void kernel_panic(const char *reason, const char *file, int line) {
    __asm__ volatile ("cli");
    paint_panic_bg();
    vram_str("REASON:", 28, 90, 0x00F87171, 2);
    vram_str(reason ? reason : "Unknown Kernel Panic", 125, 90, 0x00FFFFFF, 2);
    vram_str("SOURCE:", 28, 125, 0x00F87171, 2);
    vram_str(file ? file : "unknown", 125, 125, 0x00E2E8F0, 2);
    vram_hex("LINE:   ", (uint64_t)line, 28, 160);
    halt_with_reboot();
}

__attribute__((used)) __attribute__((used)) void panic_exception_handler(ExceptionFrame *f) {
    if ((f->cs & 3) == 3) { uint64_t cr2 = 0; __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2)); ring3_fault_recover(f->int_no, f->rip, cr2); }
    __asm__ volatile ("cli");
    uint64_t cr2 = 0;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));

    paint_panic_bg();
    const char *name = (f->int_no < 32) ? exc_names[f->int_no] : "Hardware Exception";
    vram_str("EXCEPTION:", 28, 85, 0x00F87171, 2);
    vram_str(name, 160, 85, 0x00FFFFFF, 2);

    vram_hex("RIP: ", f->rip,      28, 130);
    vram_hex("ERR: ", f->err_code, 360, 130);
    vram_hex("CR2: ", cr2,         28, 160);
    vram_hex("RSP: ", f->rsp,      360, 160);

    vram_hex("RAX: ", f->rax,      28, 205);
    vram_hex("RBX: ", f->rbx,      360, 205);
    vram_hex("RCX: ", f->rcx,      28, 235);
    vram_hex("RDX: ", f->rdx,      360, 235);
    vram_hex("RSI: ", f->rsi,      28, 265);
    vram_hex("RDI: ", f->rdi,      360, 265);
    vram_hex("RBP: ", f->rbp,      28, 295);
    vram_hex("RFL: ", f->rflags,   360, 295);

    halt_with_reboot();
}

__attribute__((naked, used)) void exc_common_stub(void) {
    __asm__ volatile (
        "push %rax\n\t"
        "push %rbx\n\t"
        "push %rcx\n\t"
        "push %rdx\n\t"
        "push %rsi\n\t"
        "push %rdi\n\t"
        "push %rbp\n\t"
        "push %r8\n\t"
        "push %r9\n\t"
        "push %r10\n\t"
        "push %r11\n\t"
        "push %r12\n\t"
        "push %r13\n\t"
        "push %r14\n\t"
        "push %r15\n\t"
        "mov %rsp, %rcx\n\t"
        "sub $32, %rsp\n\t"
        "call panic_exception_handler\n\t"
        "hlt\n\t"
    );
}

#define ISR_NOERR(n) \
    __attribute__((naked)) static void isr_##n(void) { \
        __asm__ volatile ("push $0\n\t push $" #n "\n\t jmp exc_common_stub\n\t"); \
    }

#define ISR_ERR(n) \
    __attribute__((naked)) static void isr_##n(void) { \
        __asm__ volatile ("push $" #n "\n\t jmp exc_common_stub\n\t"); \
    }

ISR_NOERR(0)  ISR_NOERR(1)  ISR_NOERR(2)  ISR_NOERR(3)
ISR_NOERR(4)  ISR_NOERR(5)  ISR_NOERR(6)  ISR_NOERR(7)
ISR_ERR(8)    ISR_NOERR(9)  ISR_ERR(10)   ISR_ERR(11)
ISR_ERR(12)   ISR_ERR(13)   ISR_ERR(14)   ISR_NOERR(15)
ISR_NOERR(16) ISR_ERR(17)   ISR_NOERR(18) ISR_NOERR(19)
ISR_NOERR(20) ISR_ERR(21)   ISR_NOERR(22) ISR_NOERR(23)
ISR_NOERR(24) ISR_NOERR(25) ISR_NOERR(26) ISR_NOERR(27)
ISR_NOERR(28) ISR_ERR(29)   ISR_ERR(30)   ISR_NOERR(31)

static void set_idt_gate(int n, void (*handler)(void), uint16_t cs) {
    uint64_t addr = (uint64_t)(uintptr_t)handler;
    idt[n].offset_low  = (uint16_t)(addr & 0xFFFF);
    idt[n].selector    = cs;
    idt[n].ist         = (n == 2 || n == 8 || n == 12 || n == 13 || n == 14 || n == 18) ? 1 : 0;
    idt[n].type_attr   = 0x8E;
    idt[n].offset_mid  = (uint16_t)((addr >> 16) & 0xFFFF);
    idt[n].offset_high = (uint32_t)((addr >> 32) & 0xFFFFFFFF);
    idt[n].zero        = 0;
}

void panic_init(BootInfo *info) {
    p_info = info;
    uint16_t cs = 0;
    __asm__ volatile ("mov %%cs, %0" : "=r"(cs));

    __asm__ volatile ("outb %0, $0x21" : : "a"((uint8_t)0xFF));
    __asm__ volatile ("outb %0, $0xA1" : : "a"((uint8_t)0xFF));

    void (*handlers[32])(void) = {
        isr_0, isr_1, isr_2, isr_3, isr_4, isr_5, isr_6, isr_7,
        isr_8, isr_9, isr_10, isr_11, isr_12, isr_13, isr_14, isr_15,
        isr_16, isr_17, isr_18, isr_19, isr_20, isr_21, isr_22, isr_23,
        isr_24, isr_25, isr_26, isr_27, isr_28, isr_29, isr_30, isr_31
    };

        __asm__ volatile ("jmp 1f\n\t isr_ignore:\n\t iretq\n\t 1:\n\t");
    extern void isr_ignore(void);
    for (int i = 32; i < 256; i++) {
        set_idt_gate(i, isr_ignore, cs);
    }

    for (int i = 0; i < 32; i++) {
        set_idt_gate(i, handlers[i], cs);
    }

    uint64_t cr4; __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4)); cr4 |= (1ULL << 6) | (1ULL << 10); __asm__ volatile ("mov %0, %%cr4" : : "r"(cr4));
    IdtPtr idtr;
    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint64_t)(uintptr_t)&idt[0];
    __asm__ volatile ("lidt %0" : : "m"(idtr));
}






