@echo off
setlocal

if not exist "build" mkdir "build"
if not exist "disk\EFI\BOOT" mkdir "disk\EFI\BOOT"
if not exist "disk\USER" mkdir "disk\USER"
REM Sonion automatic icon conversion

if not exist "assets\ICONS" mkdir "assets\ICONS"
if not exist "disk\SYSTEM\ICONS" mkdir "disk\SYSTEM\ICONS"
for %%N in (COMPUTER FILES SETTINGS) do (
    if exist "assets\ICONS\%%N.png" sonion_img.exe "assets\ICONS\%%N.png" -o "disk\SYSTEM\ICONS\%%N.BIN"
    if exist "assets\ICONS\%%N.jpg" sonion_img.exe "assets\ICONS\%%N.jpg" -o "disk\SYSTEM\ICONS\%%N.BIN"
)

if exist "assets\WALLPAPER.BIN" copy /Y "assets\WALLPAPER.BIN" "disk\SYSTEM\WALLPAPER.BIN" >nul

set UFLAGS=-O2 -ffast-math -ffreestanding -fno-stack-protector -mno-stack-arg-probe -mno-red-zone -fno-asynchronous-unwind-tables -U_WIN32 -U_WIN64 -Iuser -Ikernel\include -Ilib\include -Ikernel\include\libc
set USRCS=user\init.c user\ulibc\stdio.c user\ulibc\time.c user\ulibc\memory.c kernel\libc\string.c kernel\libc\stdlib.c kernel\libc\math.c kernel\libc\setjmp.c kernel\libc\locale_signal.c

echo === [1/4] Compiling Ring 3 User-Mode INIT.BIN ===
gcc %UFLAGS% %USRCS% -nostdlib "-Wl,--disable-dynamicbase,--image-base,0x0,-T,user\user.ld" -o build\init.elf
if errorlevel 1 goto :error
objcopy -O binary build\init.elf disk\USER\INIT.BIN
if errorlevel 1 goto :error

if exist "user\ui\main.c" (
    echo === [1B/4] Compiling Ring 3 Desktop UI [user\ui\ -> DESKTOP.SON] ===
    gcc -O2 -ffast-math -ffreestanding -fno-stack-protector -mno-stack-arg-probe -mno-red-zone -fno-asynchronous-unwind-tables -U_WIN32 -U_WIN64 -Iuser\ui -Isdk\include user\ui\*.c user\ui\drivers\*.c sdk\lib\crt0.o sdk\lib\img.o sdk\lib\stdio.o sdk\lib\time.o sdk\lib\memory.o kernel\libc\string.c kernel\libc\stdlib.c kernel\libc\math.c -nostdlib "-Wl,--disable-dynamicbase,--image-base,0x0,-T,sdk\lib\child.ld" -o build\desktop.elf
    if errorlevel 1 goto :error
    objcopy -O binary build\desktop.elf disk\USER\DESKTOP.SON
    if errorlevel 1 goto :error
)
set KSRCS=kernel\arch\gdt.c kernel\arch\paging.c kernel\arch\syscall.c kernel\drivers\acpi.c kernel\drivers\ahci.c kernel\drivers\ata.c kernel\drivers\blockdev.c kernel\drivers\nvme.c kernel\drivers\pci.c kernel\drivers\audio.c kernel\drivers\usb_msc.c kernel\fs\fat32.c kernel\libc\memory.c lib\img\img.c kernel\libc\setjmp.c kernel\libc\string.c kernel\libc\time.c kernel\console.c kernel\splash.c kernel\kernel.c kernel\sched.c kernel\kbd_events.c kernel\panic.c

python scripts\embed_logo.py
if errorlevel 1 goto :error

echo === [2/4] Compiling Thin Ring 0 Kernel (UEFI) ===
gcc -O3 -ffast-math -ffreestanding -fno-stack-protector -mno-stack-arg-probe -fshort-wchar -mno-red-zone -nostdlib "-Wl,--subsystem,10" -e efi_main -DSONION_KERNEL -Ikernel\include -Ilib\include -Ikernel\include\libc boot\uefi\main.c %KSRCS% -o disk\EFI\BOOT\BOOTX64.EFI
if errorlevel 1 goto :error
python scripts\sign_efi.py
if errorlevel 1 goto :error

echo === [4/4] Launching QEMU ===
set QEMU="C:\Program Files\qemu\qemu-system-x86_64.exe"
set OVMF_CODE="C:\Program Files\qemu\share\edk2-x86_64-code.fd"

if /i "%1"=="iso" (
    python scripts\make_iso.py
    if errorlevel 1 goto :error
    echo [READY] Share build\sonionOS.iso with your friend!
    goto :eof
)
%QEMU% -cpu max -m 512M -device AC97 -drive if=pflash,format=raw,readonly=on,file=%OVMF_CODE% -drive file=fat:rw:disk,format=raw -display sdl,grab-mod=lctrl-lalt -serial none
goto :eof

:error
echo [ERROR] Build failed!
pause
exit /b 1







































