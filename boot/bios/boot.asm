[bits 16]
[org 0x7C00]

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    mov [boot_drive], dl

    ; 1. Load 100 sectors (50 KB) from LBA 1 into 0x0800:0x0000 (0x8000)
    mov si, dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc disk_err

    ; 2. Query VESA Mode 0x144 (1024x768x32bpp) Info into 0x9000
    xor ax, ax
    mov es, ax
    mov di, 0x9000
    mov ax, 0x4F01
    mov cx, 0x4143
    int 0x10

    ; 3. Switch Screen to VESA 1024x768x32bpp Linear Framebuffer
    mov ax, 0x4F02
    mov bx, 0x4143
    int 0x10

    ; 4. Enable A20 Line & Enter 32-Bit Protected Mode
    in al, 0x92
    or al, 2
    out 0x92, al

    lgdt [gdt32_desc]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp dword 0x08:pm32_entry

disk_err:
    mov ax, 0x0E45          ; Print 'E' on error
    int 0x10
    cli
    hlt

[bits 32]
pm32_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000
    jmp 0x10000              ; Jump to entry64.asm at 0x8000

boot_drive: db 0
dap:
    db 0x10, 0
    dw 100, 0x0000, 0x1000
    dq 1

gdt32:
    dq 0
    dq 0x00CF9A000000FFFF     ; 0x08: 32-bit Code
    dq 0x00CF92000000FFFF     ; 0x10: 32-bit Data
gdt32_desc:
    dw gdt32_desc - gdt32 - 1
    dd gdt32

times 510 - ($ - $$) db 0
dw 0xAA55
