#ifndef SONION_PANIC_H
#define SONION_PANIC_H

#include <stdint.h>
#include "bootinfo.h"

void panic_init(BootInfo *info);
void kernel_panic(const char *reason, const char *file, int line);

#define PANIC(msg) kernel_panic((msg), __FILE__, __LINE__)

#endif
