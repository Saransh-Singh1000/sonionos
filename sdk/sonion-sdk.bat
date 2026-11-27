@echo off
setlocal
set SDK_DIR=%~dp0
if "%~1"=="" (
    echo === sonionOS User-Mode C / C++ SDK ===
    echo Usage: sonion-sdk.bat ^<app.c ^| app.cpp^> -o ^<output.son^>
    exit /b 1
)

set SRC=%~1
set OUT=APP.SON
if /i "%~2"=="-o" if not "%~3"=="" set OUT=%~3

if /i "%~x1"==".cpp" (
    echo [SONION-SDK] Compiling C++ source: %SRC%
    g++ -O2 -std=c++17 -fno-exceptions -fno-rtti -ffreestanding -fno-stack-protector -mno-stack-arg-probe -mno-red-zone -fno-asynchronous-unwind-tables -U_WIN32 -U_WIN64 -I"%SDK_DIR%include" -c "%SRC%" -o "%TEMP%\sonion_app.o"
) else (
    echo [SONION-SDK] Compiling C source: %SRC%
    gcc -O2 -ffreestanding -fno-stack-protector -mno-stack-arg-probe -mno-red-zone -fno-asynchronous-unwind-tables -U_WIN32 -U_WIN64 -I"%SDK_DIR%include" -c "%SRC%" -o "%TEMP%\sonion_app.o"
)
if errorlevel 1 exit /b 1

ld --disable-dynamicbase --image-base 0x0 -T "%SDK_DIR%lib\child.ld" -o "%TEMP%\sonion_app.elf" "%SDK_DIR%lib\crt0.o" "%SDK_DIR%lib\img.o" "%TEMP%\sonion_app.o" "%SDK_DIR%lib\stdio.o" "%SDK_DIR%lib\time.o" "%SDK_DIR%lib\memory.o" "%SDK_DIR%lib\string.o" "%SDK_DIR%lib\stdlib.o" "%SDK_DIR%lib\math.o" "%SDK_DIR%lib\setjmp.o" "%SDK_DIR%lib\locale_signal.o"
if errorlevel 1 exit /b 1

objcopy -O binary "%TEMP%\sonion_app.elf" "%OUT%"
if errorlevel 1 exit /b 1
echo [SONION-SDK] Built Ring 3 .SON executable: %OUT%

