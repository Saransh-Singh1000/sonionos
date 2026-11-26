@echo off
setlocal

if not exist "build" mkdir "build"
if not exist "disk\EFI\BOOT" mkdir "disk\EFI\BOOT"

set SRCS=kernel\kernel.c kernel\panic.c kernel\console.c kernel\shell.c kernel\libc\string.c kernel\libc\memory.c kernel\libc\stdlib.c kernel\libc\setjmp.c kernel\libc\math.c kernel\libc\stdio.c kernel\libc\time.c kernel\libc\locale_signal.c kernel\drivers\blockdev.c kernel\drivers\pci.c kernel\drivers\nvme.c kernel\drivers\ahci.c kernel\drivers\ata.c kernel\drivers\usb_msc.c kernel\fs\fat32.c

echo === [1/3] Compiling Shared Kernel and UEFI Target ===
gcc -O3 -ffast-math -ffreestanding -fno-stack-protector -mno-stack-arg-probe -fshort-wchar -mno-red-zone -nostdlib "-Wl,--subsystem,10" -e efi_main -Ikernel\include -Ikernel\include\libc boot\uefi\main.c %SRCS% -o disk\EFI\BOOT\BOOTX64.EFI
if errorlevel 1 goto :error

echo === [2/3] Compiling Shared Kernel and Legacy BIOS Target ===
where nasm >nul 2>nul
if %errorlevel%==0 (
    nasm -f bin boot\bios\boot.asm -o build\bios_boot.bin
    if errorlevel 1 goto :error
    nasm -f win64 boot\bios\entry64.asm -o build\entry64.o
    if errorlevel 1 goto :error

    for %%f in (boot\bios\bios_hal.c %SRCS%) do (
        gcc -O3 -ffast-math -ffreestanding -fno-stack-protector -mno-stack-arg-probe -fno-asynchronous-unwind-tables -fno-unwind-tables -mno-red-zone -mno-avx -fno-tree-loop-distribute-patterns -Ikernel\include -Ikernel\include\libc -c %%f -o build\%%~nf.o
        if errorlevel 1 goto :error
    )

    ld --disable-dynamicbase --image-base 0x10000 -T boot\bios\bios.ld -o build\bios_kernel.elf build\entry64.o build\bios_hal.o build\kernel.o build\panic.o build\console.o build\shell.o build\string.o build\memory.o build\stdlib.o build\setjmp.o build\math.o build\stdio.o build\time.o build\locale_signal.o build\blockdev.o build\pci.o build\nvme.o build\ahci.o build\ata.o build\usb_msc.o build\fat32.o
    if errorlevel 1 goto :error

    python scripts\build_flat.py
    if errorlevel 1 goto :error

    copy /b build\bios_boot.bin + build\bios_kernel.bin build\sonionos_bios.img >nul
    fsutil file seteof build\sonionos_bios.img 2097152 >nul
    echo [OK] Built both UEFI and BIOS targets!
) else (
    echo [SKIP] NASM not found!
)

echo === [3/3] Launching QEMU ===
set QEMU="C:\Program Files\qemu\qemu-system-x86_64.exe"
set BIOS_FD="C:\Program Files\qemu\share\edk2-x86_64-code.fd"

if /i "%1"=="bios" (
    %QEMU% -drive file=build\sonionos_bios.img,format=raw,if=ide -serial stdio
) else (
    %QEMU% -drive if=pflash,format=raw,readonly=on,file=%BIOS_FD% -drive file=fat:rw:disk,format=raw -device qemu-xhci -device usb-mouse -serial stdio
)
goto :eof

:error
echo [ERROR] Build failed!
pause
exit /b 1






