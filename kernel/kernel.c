#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "bootinfo.h"
#include "pat.h"
#include "arch/gdt.h"
#include "arch/paging.h"
#include "arch/syscall.h"
#include "panic.h"
#include "console.h"
#include "blockdev.h"
#include "fs/fat32.h"

static BootInfo *g_info = 0;
static uint32_t * const backbuf = (uint32_t *)0x00400000ULL;

static void swap_buffers(void) {
    if (!g_info || !g_info->framebuffer) return;
    uint32_t h = g_info->height, w = g_info->width, pitch = g_info->pitch;
    for (uint32_t y = 0; y < h; y++) {
        memcpy(&g_info->framebuffer[y * pitch], &backbuf[y * pitch], w * sizeof(uint32_t));
    }
}

void kernel_main(BootInfo *info) {
    g_info = info;
    gdt_init();
    paging_init(info);
    syscall_init(info);
    panic_init(info);
    enable_write_combining((uintptr_t)info->framebuffer, (uint64_t)info->pitch * info->height * 4);

    console_init(info, backbuf, 2);
    console_set_flush(swap_buffers);
    block_init(info);
    fat32_init();

    console_set_color(0x0038BDF8, 0x00090D16);
    console_puts("[KERNEL] Ring 0 Ready -> Launching /USER/INIT.BIN in Ring 3...");
    console_flush_now();

    int rc = ring3_exec("/USER/INIT.BIN");
    console_set_color(0x00EF4444, 0x00090D16);
    console_puts("[KERNEL] INIT.BIN exited or missing! Halting.");
    console_flush_now();
    (void)rc;
    while (1) info->sleep_ms(100);
}

