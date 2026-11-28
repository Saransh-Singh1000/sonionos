#!/usr/bin/env bash

set -e

CC="${CC:-clang}"
LD="${LD:-ld.lld}"
OBJCOPY="${OBJCOPY:-llvm-objcopy}"

TARGET_WIN="--target=x86_64-w64-windows-gnu"
TARGET_ELF="--target=x86_64-unknown-elf"
for tool in "$CC" "$LD" "$OBJCOPY" nasm python3; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "[ERROR] $tool not found!"
        exit 1
    fi
done

QEMU="${QEMU:-qemu-system-x86_64}"
BIOS_FD="${BIOS_FD:-}"

if [ -z "$BIOS_FD" ]; then
    for candidate in \
        /usr/share/OVMF/OVMF_CODE.fd \
        /usr/share/edk2/ovmf/OVMF_CODE.fd \
        /usr/share/qemu/edk2-x86_64-code.fd
    do
        if [ -f "$candidate" ]; then
            BIOS_FD="$candidate"
            break
        fi
    done
fi

mkdir -p build disk/EFI/BOOT disk/USER
# Sonion automatic SVG -> PNG -> BIN icon conversion

mkdir -p build disk/SYSTEM/ICONS

for pair in "COMPUTER:monitor" "FILES:folder" "SETTINGS:settings"; do
    name="${pair%%:*}"
    source="${pair##*:}"

    echo "[ICON] Converting $source.svg -> $name.BIN"

    rsvg-convert -w 64 -h 64 \
        "assets/icons/$source.svg" \
        -o "build/$name.png"

    tools/sonion_img/sonion_img \
        "build/$name.png" \
        -o "disk/SYSTEM/ICONS/$name.BIN"
done


if [ -f "user/init.c" ]; then
UFLAGS=(
    -O2
    -ffast-math
    -ffreestanding
    -fno-stack-protector
    -mno-stack-arg-probe
    -mno-red-zone
    -fno-asynchronous-unwind-tables
    -U_WIN32
    -U_WIN64
    -Iuser
    -DSONION_KERNEL -Ikernel/include -Ilib/include
    -DSONION_KERNEL -Ikernel/include -Ilib/include/libc
)

USRCS=(
    user/init.c
    user/ulibc/stdio.c
    user/ulibc/time.c
    user/ulibc/memory.c
    kernel/libc/string.c
    kernel/libc/stdlib.c
    kernel/libc/math.c
    kernel/libc/setjmp.c
    kernel/libc/locale_signal.c
)

echo "=== [1/4] Compiling Ring 3 User-Mode INIT.BIN ==="
UOBJS=()
i=0
for src in "${USRCS[@]}"; do
    obj="build/user_${i}.o"
    "$CC" $TARGET_ELF "${UFLAGS[@]}" -c "$src" -o "$obj"
    UOBJS+=("$obj")
    ((i+=1))
done

"$LD" -m elf_x86_64 -T user/user.ld -o build/init.elf "${UOBJS[@]}"
"$OBJCOPY" -O binary build/init.elf disk/USER/INIT.BIN
else
    echo "=== [1/4] Skipping Ring 3 INIT.BIN (user/ is empty) ==="
fi

KSRCS=(
    kernel/kernel.c
    kernel/sched.c kernel/kbd_events.c
    kernel/splash.c
    kernel/panic.c
    kernel/console.c
    kernel/arch/gdt.c
    kernel/arch/paging.c
    kernel/arch/syscall.c
    kernel/libc/string.c
    kernel/libc/memory.c lib/img/img.c
    kernel/libc/setjmp.c
    kernel/libc/time.c
    kernel/drivers/acpi.c
    kernel/drivers/blockdev.c
    kernel/drivers/pci.c kernel/drivers/audio.c
    kernel/drivers/nvme.c
    kernel/drivers/ahci.c
    kernel/drivers/ata.c
    kernel/drivers/usb_msc.c
    kernel/fs/fat32.c
)

python3 scripts/embed_logo.py

if [ -f "user/ui/main.c" ]; then
    echo "=== [1B/4] Compiling Ring 3 Desktop UI (user/ui/ -> DESKTOP.SON) ==="
    "$CC" $TARGET_ELF -O2 -ffast-math -ffreestanding -fno-stack-protector -mno-stack-arg-probe -mno-red-zone -fno-asynchronous-unwind-tables -U_WIN32 -U_WIN64 -Iuser/ui -Isdk/include user/ui/*.c sdk/lib/crt0.o sdk/lib/img.o sdk/lib/stdio.o sdk/lib/time.o sdk/lib/memory.o kernel/libc/string.c kernel/libc/stdlib.c kernel/libc/math.c -nostdlib -Wl,--image-base,0x0,-T,sdk/lib/child.ld -o build/desktop.elf
    "$OBJCOPY" -O binary build/desktop.elf disk/USER/DESKTOP.SON
fi

echo "=== [2/4] Compiling Thin Ring 0 Kernel (UEFI) ==="

"$CC" $TARGET_WIN -fuse-ld=lld -O3 -ffast-math \
    -ffreestanding \
    -fno-stack-protector \
    -mno-stack-arg-probe \
    -fshort-wchar \
    -mno-red-zone \
    -nostdlib \
    -Wl,--subsystem,10 \
    -e efi_main \
    -DSONION_KERNEL -Ikernel/include -Ilib/include \
    -DSONION_KERNEL -Ikernel/include -Ilib/include/libc \
    boot/uefi/main.c \
    "${KSRCS[@]}" \
    -o disk/EFI/BOOT/BOOTX64.EFI

python3 scripts/sign_efi.py

echo "=== [3/4] Compiling Thin Ring 0 Kernel (BIOS) ==="

if command -v nasm >/dev/null 2>&1; then
    nasm -f bin boot/bios/boot.asm -o build/bios_boot.bin
    nasm -f elf64 boot/bios/entry64.asm -o build/entry64.o

    for src in boot/bios/bios_hal.c "${KSRCS[@]}"; do
        name=$(basename "$src" .c)
        "$CC" $TARGET_ELF -O3 -ffast-math \
            -ffreestanding \
            -fno-stack-protector \
            -fno-asynchronous-unwind-tables \
            -fno-unwind-tables \
            -mno-red-zone \
            -mno-avx \
            -fno-tree-loop-distribute-patterns \
            -DSONION_KERNEL -Ikernel/include -Ilib/include \
            -Ikernel/include/libc \
            -c "$src" \
            -o "build/$name.o"
    done

    "$LD" -m elf_x86_64 \
        --script=boot/bios/bios.ld \
        -o build/bios_kernel.elf \
        build/entry64.o \
        build/bios_hal.o \
        build/kernel.o \
        build/sched.o build/kbd_events.o \ \ build/splash.o \
        build/panic.o \
        build/console.o \
        build/gdt.o \
        build/paging.o \
        build/syscall.o \
        build/string.o \
        build/memory.o build/img.o \
        build/setjmp.o \
        build/time.o \
        build/acpi.o \
        build/blockdev.o \
        build/pci.o build/audio.o \
        build/nvme.o \
        build/ahci.o \
        build/ata.o \
        build/usb_msc.o \
        build/fat32.o \

    python3 scripts/build_flat.py
    cat build/bios_boot.bin build/bios_kernel.bin > build/sonionos_bios.img
    truncate -s 2097152 build/sonionos_bios.img
    echo "[OK] Built Ring 3 User Space + Thin Ring 0 Kernels!"
else
    echo "[SKIP] NASM not found!"
fi

echo "=== [4/4] Launching QEMU ==="

if [ "${1:-}" = "iso" ]; then
    python3 scripts/make_iso.py
    echo "[READY] Built build/sonionOS.iso!"
elif [ "${1:-}" = "bios" ]; then
    "$QEMU" \
        -cpu max \
        -m 512M -audiodev sdl,id=snd0 -device AC97,audiodev=snd0 \
        -drive file=build/sonionos_bios.img,format=raw,if=ide,index=0 \
        -drive file=fat:rw:disk,format=raw,if=ide,index=1 \
        -display sdl,grab-mod=lctrl-lalt \
        -serial none
else
    if [ -z "$BIOS_FD" ]; then
        echo "[ERROR] OVMF firmware not found! Set BIOS_FD to your OVMF_CODE.fd path."
        exit 1
    fi
    "$QEMU" \
        -cpu max \
        -m 512M -audiodev sdl,id=snd0 -device AC97,audiodev=snd0 \
        -drive if=pflash,format=raw,readonly=on,file="$BIOS_FD" \
        -drive file=fat:rw:disk,format=raw \
        -display sdl,grab-mod=lctrl-lalt \
        -serial none
fi
