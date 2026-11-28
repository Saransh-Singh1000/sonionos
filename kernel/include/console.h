#ifndef SONION_CONSOLE_H
#define SONION_CONSOLE_H

#include <stdint.h>
#include <stddef.h>
#include "bootinfo.h"

void console_init(BootInfo *info, uint32_t *backbuf, int scale);
void console_clear(void);
void console_set_color(uint32_t fg, uint32_t bg);
void console_putchar(char c);
void console_write(const char *buf, size_t len);
void console_puts(const char *s);
void console_render(void);
int  console_getchar(int blocking);

void console_set_flush(void (*fn)(void));
void console_flush_now(void);
void console_set_title(const char *title);
#endif


