#ifndef SONION_GFX_H
#define SONION_GFX_H

#include <stdint.h>
#include "bootinfo.h"

void gfx_init(BootInfo *info, uint32_t *backbuf);
void gfx_pixel(int x, int y, uint32_t color);
void gfx_fill(uint32_t color);
void gfx_fill_rect(int x, int y, int w, int h, uint32_t color);
void gfx_rect(int x, int y, int w, int h, uint32_t color);

#endif
