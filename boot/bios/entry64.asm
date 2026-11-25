[bits 32]
section .text.trampoline
global _start32
extern bios_entry

_start32:
    ; Zero out 9 MB of .bss RAM (0x20000 -> 0x920000) so backbuf & globals are clean
    mov edi, 0x20000
    xor eax, eax
    mov ecx, 2359296
    rep stosd

    ; Build 64-Bit Identity Page Tables at 0x1000 (maps first 4 GB)
    mov edi, 0x1000
    xor eax, eax
    mov ecx, 8192
    rep stosd

    mov dword [0x1000], 0x2003
    mov dword [0x2000], 0x3003
    mov dword [0x2008], 0x4003
    mov dword [0x2010], 0x5003
    mov dword [0x2018], 0x6003

    mov edi, 0x3000
    mov eax, 0x00000083
    mov ecx, 2048
.map_loop:
    mov [edi], eax
    add eax, 0x200000
    add edi, 8
    loop .map_loop

    ; Enable FPU & SSE
    mov eax, cr0
    and ax, 0xFFFB
    or ax, 0x2
    mov cr0, eax

    mov eax, cr4
    or eax, (1 << 5) | (1 << 9) | (1 << 10)
    mov cr4, eax

    ; Enable Long Mode (EFER.LME) & Paging
    mov eax, 0x1000
    mov cr3, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    lgdt [gdt64_desc]
    jmp dword 0x08:lm64_ready

[bits 64]
lm64_ready:
    mov ax, 0x10
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Place 16-byte aligned stack safely at 0x9F0000 (above .bss!)
    mov rsp, 0x9F0000
    mov rcx, 0x9000
    mov rdi, 0x9000
    mov rax, bios_entry
    call rax

.halt:
    hlt
    jmp .halt

align 8
gdt64:
    dq 0
    dq 0x00209A0000000000
    dq 0x0000920000000000
gdt64_desc:
    dw gdt64_desc - gdt64 - 1
    dd gdt64

