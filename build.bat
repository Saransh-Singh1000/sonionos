@echo off
setlocal

if not exist "build" mkdir "build"
if not exist "disk\EFI\BOOT" mkdir "disk\EFI\BOOT"
if not exist "disk\USER" mkdir "disk\USER"
if exist "assets\WALLPAPER.BIN" copy /Y "assets\WALLPAPER.BIN" "disk\WALLPAPER.BIN" >nul

set UFLAGS=-O2 -ffast-math -ffreestanding -fno-stack-protector -mno-stack-arg-probe -mno-red-zone -fno-asynchronous-unwind-tables -U_WIN32 -U_WIN64 -Iuser -Ikernel\include -Ikernel\include\libc -Iuser\lua
set USRCS=user\init.c user\ulibc\stdio.c user\ulibc\time.c user\ulibc\memory.c kernel\libc\string.c kernel\libc\stdlib.c kernel\libc\math.c kernel\libc\setjmp.c kernel\libc\locale_signal.c user\lua_runner.c

echo === [1/4] Compiling Ring 3 User-Mode INIT.BIN ===
gcc %UFLAGS% %USRCS% user\lua\*.c -nostdlib "-Wl,--disable-dynamicbase,--image-base,0x0,-T,user\user.ld" -o build\init.elf
if errorlevel 1 goto :error
objcopy -O binary build\init.elf disk\USER\INIT.BIN
if errorlevel 1 goto :error

set KSRCS=kernel\arch\gdt.c kernel\arch\paging.c kernel\arch\syscall.c kernel\drivers\acpi.c kernel\drivers\ahci.c kernel\drivers\ata.c kernel\drivers\blockdev.c kernel\drivers\nvme.c kernel\drivers\pci.c kernel\drivers\usb_msc.c kernel\fs\fat32.c kernel\libc\memory.c kernel\libc\setjmp.c kernel\libc\string.c kernel\libc\time.c kernel\console.c kernel\desktop.c kernel\gfx.c kernel\wallpaper.c kernel\kernel.c kernel\sched.c kernel\panic.c

echo === [2/4] Compiling Thin Ring 0 Kernel (UEFI) ===
gcc -O3 -ffast-math -ffreestanding -fno-stack-protector -mno-stack-arg-probe -fshort-wchar -mno-red-zone -nostdlib "-Wl,--subsystem,10" -e efi_main -Ikernel\include -Ikernel\include\libc boot\uefi\main.c %KSRCS% -o disk\EFI\BOOT\BOOTX64.EFI
if errorlevel 1 goto :error

echo === [3/4] Compiling Thin Ring 0 Kernel (BIOS) ===
where nasm >nul 2>nul
if %errorlevel%==0 (
    nasm -f bin boot\bios\boot.asm -o build\bios_boot.bin
    if errorlevel 1 goto :error
    nasm -f win64 boot\bios\entry64.asm -o build\entry64.o
    if errorlevel 1 goto :error

    for %%f in (boot\bios\bios_hal.c %KSRCS%) do (
        gcc -O3 -ffast-math -ffreestanding -fno-stack-protector -mno-stack-arg-probe -fno-asynchronous-unwind-tables -fno-unwind-tables -mno-red-zone -mno-avx -fno-tree-loop-distribute-patterns -Ikernel\include -Ikernel\include\libc -c %%f -o build\%%~nf.o
        if errorlevel 1 goto :error
    )

    ld --disable-dynamicbase --image-base 0x10000 -T boot\bios\bios.ld -o build\bios_kernel.elf build\entry64.o build\bios_hal.o build\acpi.o build\gdt.o build\paging.o build\syscall.o build\ahci.o build\ata.o build\blockdev.o build\nvme.o build\pci.o build\usb_msc.o build\fat32.o build\memory.o build\setjmp.o build\string.o build\time.o build\console.o build\desktop.o build\gfx.o build\kernel.o build\sched.o build\panic.o
    if errorlevel 1 goto :error

    python scripts\build_flat.py
    if errorlevel 1 goto :error

    copy /b build\bios_boot.bin + build\bios_kernel.bin build\sonionos_bios.img >nul
    fsutil file seteof build\sonionos_bios.img 2097152 >nul
    echo [OK] Built Ring 3 User Space + Thin Ring 0 Kernels!
) else (
    echo [SKIP] NASM not found!
)

echo === [4/4] Launching QEMU ===
set QEMU="C:\Program Files\qemu\qemu-system-x86_64.exe"
set BIOS_FD="C:\Program Files\qemu\share\edk2-x86_64-code.fd"

if /i "%1"=="iso" (
    python scripts\make_iso.py
    if errorlevel 1 goto :error
    echo [READY] Share build\sonionOS.iso with your friend!
    goto :eof
)
if /i "%1"=="bios" (
    %QEMU% -cpu max -m 256M -drive file=build\sonionos_bios.img,format=raw,if=ide,index=0 -drive file=fat:rw:disk,format=raw,if=ide,index=1 -display sdl,grab-mod=lctrl-lalt -serial none
) else (
    %QEMU% -cpu max -m 256M -drive if=pflash,format=raw,readonly=on,file=%BIOS_FD% -drive file=fat:rw:disk,format=raw -display sdl,grab-mod=lctrl-lalt -serial none
)
goto :eof

:error
echo [ERROR] Build failed!
pause
exit /b 1






















