#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "bootinfo.h"
#include "pat.h"
#include "panic.h"
#include "console.h"
#include "shell.h"
#include "blockdev.h"
#include "fs/fat32.h"

static BootInfo *g_info = 0;
static uint32_t backbuf[1920 * 1080];

static void swap_buffers(void) {
    if (!g_info || !g_info->framebuffer) return;
    uint32_t h = g_info->height, w = g_info->width, pitch = g_info->pitch;
    for (uint32_t y = 0; y < h; y++) {
        memcpy(&g_info->framebuffer[y * pitch], &backbuf[y * pitch], w * sizeof(uint32_t));
    }
}

void kernel_main(BootInfo *info) {
    g_info = info;
    panic_init(info);
    enable_write_combining((uintptr_t)info->framebuffer, (uint64_t)info->pitch * info->height * 4);

    console_init(info, backbuf, 2);
    block_init(info);
    int fs_ok = fat32_init();
    shell_init(info);

    console_set_color(0x0038BDF8, 0x00090D16);
    printf("sonionOS 0.1.0-x86_64 (tty1)\n");
    console_set_color(0x0022C55E, 0x00090D16);
    printf("[DISK] %s | [FS] %s (%s)\n",
           block_get_driver_name(), fs_ok ? "MOUNTED" : "NONE", fat32_get_type_name());
    console_set_color(0x0094A3B8, 0x00090D16);
    printf("Type 'help' for standard Linux commands.\n\n");

    char cmd[256];
    int pos = 0;
    console_set_color(0x0022C55E, 0x00090D16);
    printf("root@sonionOS:%s# ", shell_get_cwd());
    console_set_color(0x00E2E8F0, 0x00090D16);

    InputState in;
    while (1) {
        console_render();
        swap_buffers();
        info->poll_input(&in);
        if (in.scan_code == 0x17) info->shutdown();
        if (in.key_char != 0) {
            char c = in.key_char;
            if (c == '\r' || c == '\n') {
                putchar('\n');
                cmd[pos] = 0;
                shell_exec(cmd);
                pos = 0;
                console_set_color(0x0022C55E, 0x00090D16);
                printf("root@sonionOS:%s# ", shell_get_cwd());
                console_set_color(0x00E2E8F0, 0x00090D16);
            } else if (c == '\b') {
                if (pos > 0) { pos--; putchar('\b'); }
            } else if (pos < 254) {
                cmd[pos++] = c;
                putchar(c);
            }
        }
        info->sleep_ms(16);
    }
}

