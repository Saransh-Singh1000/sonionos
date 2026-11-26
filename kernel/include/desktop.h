#ifndef SONION_DESKTOP_H
#define SONION_DESKTOP_H

#include <stdint.h>
#include "bootinfo.h"

void desktop_init(BootInfo *info, uint32_t *backbuf);
void desktop_update(const InputState *input);
void desktop_draw_cursor(void);
void desktop_console_bounds(int *x, int *y, int *w, int *h);
int desktop_console_visible(void);

#endif
