#include "ui_drivers.h"
#include "sysinfo_page.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *fb_dev = 0;
static FILE *mouse_dev = 0;
static FILE *kbd_dev = 0;
static int32_t last_wheel_z = 0;

int ui_fb_open(UiFramebuffer *fb) {
    if (!fb) return 0;
    SonionSysInfo *sys = (SonionSysInfo *)(uintptr_t)SYSINFO_PAGE_ADDR;
    fb->width  = sys->screen_w ? sys->screen_w : 1024;
    fb->height = sys->screen_h ? sys->screen_h : 768;

    fb_dev = fopen("/dev/fb0", "wb");
    if (!fb_dev) return 0;

    size_t bytes = (size_t)fb->width * (size_t)fb->height * 4;
    fb->backbuf = (uint32_t *)malloc(bytes);
    if (!fb->backbuf) { fclose(fb_dev); fb_dev = 0; return 0; }
    memset(fb->backbuf, 0, bytes);

    mouse_dev = fopen("/dev/mouse", "rb");
    kbd_dev   = fopen("/dev/kbd", "rb");
    last_wheel_z = sys->mouse_z;
    return 1;
}

void ui_fb_present(UiFramebuffer *fb) {
    if (!fb || !fb->backbuf || !fb_dev) return;
    fseek(fb_dev, 0, SEEK_SET);
    fwrite(fb->backbuf, 4, (size_t)fb->width * (size_t)fb->height, fb_dev);
}

void ui_fb_close(UiFramebuffer *fb) {
    if (kbd_dev)   { fclose(kbd_dev);   kbd_dev = 0; }
    if (mouse_dev) { fclose(mouse_dev); mouse_dev = 0; }
    if (fb_dev)    { fclose(fb_dev);    fb_dev = 0; }
    if (fb && fb->backbuf) { free(fb->backbuf); fb->backbuf = 0; }
}

void ui_mouse_poll(UiMouseState *ms) {
    if (!ms) return;
    if (mouse_dev) {
        int32_t pkt[3] = {0};
        fread(pkt, 1, sizeof(pkt), mouse_dev);
    }
    SonionSysInfo *sys = (SonionSysInfo *)(uintptr_t)SYSINFO_PAGE_ADDR;
    ms->x = sys->mouse_x;
    ms->y = sys->mouse_y;
    ms->buttons = (uint8_t)(sys->mouse_btns & 0x07);
    ms->scroll_dz = (int8_t)(sys->mouse_z - last_wheel_z);
    last_wheel_z = sys->mouse_z;
}

int ui_kbd_poll(SonionKeyEvent *ev) {
    if (!ev) return 0;
    if (sys_poll_key(ev)) return 1;
    if (kbd_dev) {
        char raw[2] = {0};
        if (fread(raw, 1, 2, kbd_dev) > 0 && (raw[0] || raw[1])) {
            ev->ascii = raw[0];
            ev->scancode = (uint8_t)raw[1];
            ev->pressed = 1;
            ev->modifiers = (uint8_t)(((SonionSysInfo *)(uintptr_t)SYSINFO_PAGE_ADDR)->key_mods);
            return 1;
        }
    }
    return 0;
}
