#ifndef SONION_SPLASH_H
#define SONION_SPLASH_H

#include <stdint.h>
#include "bootinfo.h"

void boot_splash_show(BootInfo *info, uint32_t *backbuf);

#endif
