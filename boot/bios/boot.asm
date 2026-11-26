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

    ; Load 3 x 64 sectors (192 sectors = 96 KB) from LBA 1 into 0x10000..0x28000
    mov cx, 3
.read_loop:
    push cx
    mov si, dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc disk_err
    add word [dap + 6], 0x0800
    add dword [dap + 8], 64
    pop cx
    loop .read_loop

    xor ax, ax
    mov es, ax
    mov di, 0x9000
    mov ax, 0x4F01
    mov cx, 0x4143
    int 0x10

    mov ax, 0x4F02
    mov bx, 0x4143
    int 0x10

    in al, 0x92
    or al, 2
    out 0x92, al

    lgdt [gdt32_desc]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp dword 0x08:pm32_entry

disk_err:
    mov ax, 0x0E45
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
    jmp 0x10000

boot_drive: db 0
dap:
    db 0x10, 0
    dw 64, 0x0000, 0x1000
    dq 1

gdt32:
    dq 0
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
gdt32_desc:
    dw gdt32_desc - gdt32 - 1
    dd gdt32

times 510 - ($ - $$) db 0
dw 0xAA55
