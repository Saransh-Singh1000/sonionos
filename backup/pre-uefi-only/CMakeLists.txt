cmake_minimum_required(VERSION 3.25)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER clang)
set(CMAKE_C_COMPILER_TARGET x86_64-w64-windows-gnu)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(sonionOS LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(COMMON_FLAGS
    -O2 -ffast-math -ffreestanding
    -fno-stack-protector -fno-asynchronous-unwind-tables
    -fno-unwind-tables -mno-red-zone -mno-avx
    
)

set(KERNEL_INCLUDES
    kernel/include
    kernel/include/libc
    lib/include
)

set(KERNEL_SOURCES
    kernel/arch/gdt.c
    kernel/arch/paging.c
    kernel/arch/syscall.c
    kernel/drivers/acpi.c
    kernel/drivers/ahci.c
    kernel/drivers/ata.c
    kernel/drivers/blockdev.c
    kernel/drivers/nvme.c
    kernel/drivers/pci.c
    kernel/drivers/audio.c
    kernel/drivers/usb_msc.c
    kernel/fs/fat32.c
    kernel/libc/memory.c
    kernel/libc/string.c
    kernel/libc/time.c
    kernel/libc/setjmp.c
    lib/img/img.c
    kernel/console.c
    kernel/splash.c
    kernel/kernel.c
    kernel/sched.c
    kernel/kbd_events.c
    kernel/panic.c
)

function(sonion_objects target)
    add_library(${target} OBJECT ${ARGN})
    target_compile_options(${target} PRIVATE ${COMMON_FLAGS})
    target_include_directories(${target} PRIVATE ${KERNEL_INCLUDES})
    target_compile_definitions(${target} PRIVATE SONION_KERNEL)
endfunction()

sonion_objects(sonion_uefi
    boot/uefi/main.c
    ${KERNEL_SOURCES}
)
target_compile_options(sonion_uefi PRIVATE
    -O3 -fshort-wchar -mno-stack-arg-probe
)

sonion_objects(sonion_bios
    boot/bios/bios_hal.c
    ${KERNEL_SOURCES}
)
target_compile_options(sonion_bios PRIVATE -mno-stack-arg-probe)

add_library(sonion_init OBJECT
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
target_compile_options(sonion_init PRIVATE ${COMMON_FLAGS})
target_include_directories(sonion_init PRIVATE
    user ${KERNEL_INCLUDES}
)

file(GLOB DESKTOP_SOURCES CONFIGURE_DEPENDS
    user/ui/*.c
    user/ui/drivers/*.c
)

add_library(sonion_desktop OBJECT ${DESKTOP_SOURCES})
target_compile_options(sonion_desktop PRIVATE ${COMMON_FLAGS})
target_include_directories(sonion_desktop PRIVATE
    user/ui
    sdk/include
    kernel/include/libc
)


find_package(Python3 REQUIRED COMPONENTS Interpreter)

target_sources(sonion_desktop PRIVATE
    kernel/libc/string.c
    kernel/libc/stdlib.c
    kernel/libc/math.c
)


set(OUT_EFI "${CMAKE_SOURCE_DIR}/disk/EFI/BOOT/BOOTX64.EFI")
set(OUT_INIT "${CMAKE_SOURCE_DIR}/disk/USER/INIT.BIN")
set(OUT_DESKTOP "${CMAKE_SOURCE_DIR}/disk/USER/DESKTOP.SON")
set(OUT_BIOS "${CMAKE_SOURCE_DIR}/build/sonionos_bios.img")

set(BUILD_DIR "${CMAKE_SOURCE_DIR}/build")
set(INIT_EXE "${BUILD_DIR}/init.exe")
set(DESKTOP_EXE "${BUILD_DIR}/desktop.exe")
set(BIOS_KERNEL "${BUILD_DIR}/bios_kernel.elf")

file(MAKE_DIRECTORY
    "${CMAKE_SOURCE_DIR}/disk/EFI/BOOT"
    "${CMAKE_SOURCE_DIR}/disk/USER"
    "${BUILD_DIR}"
)

add_custom_command(OUTPUT "${OUT_EFI}"
    COMMAND lld-link /nologo /machine:x64 /subsystem:efi_application
        /entry:efi_main /nodefaultlib /fixed /out:${OUT_EFI}
        $<TARGET_OBJECTS:sonion_uefi>
    DEPENDS sonion_uefi
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMAND_EXPAND_LISTS VERBATIM
)

add_custom_command(OUTPUT "${OUT_INIT}"
    COMMAND lld-link /nologo /machine:x64 /subsystem:native
        /entry:_start /nodefaultlib /fixed /base:0x01000000
        /out:${INIT_EXE}
        $<TARGET_OBJECTS:sonion_init>
    COMMAND llvm-objcopy -O binary "${INIT_EXE}" "${OUT_INIT}"
    DEPENDS sonion_init user/user.ld
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMAND_EXPAND_LISTS VERBATIM
)

add_custom_command(OUTPUT "${OUT_DESKTOP}"
    COMMAND lld-link /nologo /machine:x64 /subsystem:native
        /entry:_start /nodefaultlib /fixed /base:0x02800000
        /out:${DESKTOP_EXE}
        $<TARGET_OBJECTS:sonion_desktop>
        "${CMAKE_SOURCE_DIR}/sdk/lib/crt0.o"
        "${CMAKE_SOURCE_DIR}/sdk/lib/img.o"
        "${CMAKE_SOURCE_DIR}/sdk/lib/stdio.o"
        "${CMAKE_SOURCE_DIR}/sdk/lib/time.o"
        "${CMAKE_SOURCE_DIR}/sdk/lib/memory.o"
    COMMAND llvm-objcopy -O binary "${DESKTOP_EXE}" "${OUT_DESKTOP}"
    DEPENDS sonion_desktop
        sdk/lib/child.ld
        sdk/lib/crt0.o sdk/lib/img.o sdk/lib/stdio.o
        sdk/lib/time.o sdk/lib/memory.o
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMAND_EXPAND_LISTS VERBATIM
)

add_custom_command(OUTPUT "${OUT_BIOS}"
    COMMAND nasm -f bin boot/bios/boot.asm
        -o "${BUILD_DIR}/bios_boot.bin"
    COMMAND nasm -f win64 boot/bios/entry64.asm
        -o "${BUILD_DIR}/entry64.obj"
    COMMAND ld.lld -mi386pep
        -T boot/bios/bios.ld
        -o "${BIOS_KERNEL}"
        "${BUILD_DIR}/entry64.obj"
        $<TARGET_OBJECTS:sonion_bios>
    COMMAND ${Python3_EXECUTABLE} scripts/build_flat.py
    COMMAND sh -c "cat '${BUILD_DIR}/bios_boot.bin' '${BUILD_DIR}/bios_kernel.bin' > '${OUT_BIOS}'"
    DEPENDS sonion_bios
        boot/bios/boot.asm boot/bios/entry64.asm
        boot/bios/bios.ld scripts/build_flat.py
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMAND_EXPAND_LISTS VERBATIM
)

add_custom_target(sonionos ALL
    DEPENDS "${OUT_EFI}" "${OUT_INIT}" "${OUT_DESKTOP}" "${OUT_BIOS}"
)

enable_testing()
add_test(NAME sonion_artifacts
    COMMAND ${Python3_EXECUTABLE}
        "${CMAKE_SOURCE_DIR}/scripts/check_artifacts.py"
        "${OUT_EFI}" "${OUT_BIOS}" "${OUT_INIT}" "${OUT_DESKTOP}"
)
set_tests_properties(sonion_artifacts PROPERTIES DEPENDS sonionos)

message(STATUS "SonionOS full build configured")
message(STATUS "SDK dependencies preserved")
